#include "PropertyFile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <iostream>

#include "misc.h"

using namespace std;

namespace PropertyFile
{

	static void dump( list<char *> &l )
	{
		itfor( list<char *>, l, it )
		{
			cout << *it << "/";
		}
		cout << endl;
	}

	static void freeall( list<char *> &l )
	{
		itfor( list<char *>, l, it )
		{
			free( *it );
		}
		l.clear();
	}

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Identifier
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------

	Identifier::Identifier( const string &name )
	{
		this->name = name;
	}

	Identifier::Identifier( int index )
	{
		char buf[32];
		sprintf( buf, "%d", index );
		this->name = buf;
	}

	Identifier::Identifier( unsigned int index )
	{
		char buf[32];
		sprintf( buf, "%u", index );
		this->name = buf;
	}

	Identifier::Identifier( size_t index )
	{
		char buf[32];
		sprintf( buf, "%lu", index );
		this->name = buf;
	}

	Identifier::Identifier( const char *name)
	{
		this->name = name;
	}

	Identifier::Identifier( const Identifier &other )
	{
		this->name = other.name;
	}

	Identifier::~Identifier()
	{
	}

	const char *Identifier::getName() const
	{
		return this->name.c_str();
	}

	bool operator<( const Identifier &a, const Identifier &b )
	{
		return strcmp( a.getName(), b.getName() ) < 0;
	}

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS DocumentLocation
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	DocumentLocation::DocumentLocation( Document *_doc, unsigned int _lineno )
		: doc( _doc )
		, lineno( _lineno )
	{
	}

	string DocumentLocation::getDescription()
	{
		char buf[1024 * 4];

		sprintf( buf, "%s:%u", doc->getName(), lineno );

		return buf;
	}

	void DocumentLocation::err( string msg )
	{
		fprintf( stderr, "%s:%u: %s\n", doc->getName(), lineno, msg.c_str() );
		exit( 1 );
	}

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Property
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------

	Property::Property( DocumentLocation _loc, Identifier _id, bool _isArray )
		: loc(_loc)
		, id( _id )
		, isArray( _isArray )
	{
		type =  OBJECT;
		oval = new PropertyMap();
	}

	Property::Property( DocumentLocation _loc, Identifier _id, const char *val )
		: loc( _loc )
		, id( _id )
	{
		type = SCALAR;
		sval = strdup( val );
	}

	Property::~Property()
	{
		switch( type )
		{
		case OBJECT:
			itfor( PropertyMap, *oval, it )
			{
				delete it->second;
			}
			delete oval;
			break;
		case SCALAR:
			free( sval );
			break;
		default:
			assert( false );
		}
	}

	const char *Property::getName()
	{
		return id.getName();
	}

	Property &Property::get( Identifier id )
	{
		assert( type == OBJECT );

		Property *prop = getp( id );

		if( prop == NULL )
		{
			err( string("Expecting property '") + id.getName() + "' for '" + getName() + "'");
		}

		return *prop;
	}

	Property *Property::getp( Identifier id )
	{
		assert( type == OBJECT );

		return (*oval)[ id ];
	}

	Property::operator int()
	{
		int ival;

		if( (type != SCALAR) || !Parser::parseInt(sval, &ival) )
		{
			err( string("Expecting INT value for property '") + getName() + "'." );
		}

		return ival;
	}

	Property::operator float()
	{
		float fval;

		if( (type != SCALAR) || !Parser::parseFloat(sval, &fval) )
		{
			err( string("Expecting FLOAT value for property '") + getName() + "'." );
		}

		return fval;
	}

	Property::operator bool()
	{
		bool bval;

		if( (type != SCALAR) || !Parser::parseBool(sval, &bval) )
		{
			err( string("Expecting BOOL value for property '") + getName() + "'." );
		}

		return bval;
	}

	Property::operator string()
	{
		assert( type == SCALAR );

		return string( sval );
	}

	void Property::add( Property *prop )
	{
		assert( type == OBJECT );

		(*oval)[ prop->id ] = prop;
	}

	void Property::err( string message )
	{
		loc.err( message );
	}

	void Property::dump( ostream &out, const char *indent )
	{
		out << loc.getDescription() << " ";
		out << indent << getName();

		if( type == OBJECT )
		{
			if( isArray )
			{
				out << " [";
			}
			out << endl;
			itfor( PropertyMap, *oval, it )
			{
				it->second->dump( out, (string(indent) + "  ").c_str() );
			}
			if( isArray )
			{
				out << loc.getDescription() << " ";
				out << indent << "]" << endl;
			}
		}
		else
		{
			out << " = " << sval << endl;
		}

	}

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Document
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------

	Document::Document( const char *name )
		: Property( DocumentLocation(this,0), string(name) )
	{
	}

	Document::~Document()
	{
	}

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Parser
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	
	Document *Parser::parse( const char *path )
	{
		Document *doc = new Document( path );
		DocumentLocation loc( doc, 0 );
		PropertyStack propertyStack;

		propertyStack.push( doc );

		ifstream in( path );
		char *line;

		while( (line = readline(in, loc)) != NULL )
		{
			StringList tokens;
			tokenize( line, tokens );

			processLine( doc,
						 loc,
						 propertyStack,
						 tokens );

			freeall( tokens );
			delete line;
		}

		if( propertyStack.size() > 1 )
		{
			loc.err( "Unexpected end of document. Likely missing '}' or ']'" );
		}

		return doc;
	}

	bool Parser::isValidIdentifier( const char *text )
	{
		bool valid = false;

		if( isalpha(*text) )
		{
			valid = true;

			for( const char *c = text; *c && valid; c++ )
			{
				valid = isalnum(*c);
			}
		}

		return valid;
	}

	bool Parser::parseInt( const char *text, int *result )
	{
		char *end;
		int ival = (int)strtol( text, &end, 10 );
		if( *end != '\0' )
		{
			return false;
		}

		if( result != NULL )
		{
			*result = ival;
		}

		return true;
	}

	bool Parser::parseFloat( const char *text, float *result )
	{
		char *end;
		float fval = (float)strtof( text, &end );
		if( *end != '\0' )
		{
			return false;
		}

		if( result != NULL )
		{
			*result = fval;
		}

		return true;
	}

	bool Parser::parseBool( const char *text, bool *result )
	{
		bool bval;

		if( 0 == strcmp(text, "True") )
		{
			bval = true;
		}
		else if( 0 == strcmp(text, "False") )
		{
			bval = false;
		}
		else
		{
			return false;
		}

		if( result != NULL )
		{
			*result = bval;
		}

		return true;
	}

	char *Parser::readline( istream &in,
							DocumentLocation &loc )
	{
		char buf[1024 * 4];
		char *result = NULL;

		do
		{
			in.getline( buf, sizeof(buf) );
			if( !in.good() )
			{
				break;
			}

			char *line = buf;

			loc.lineno++;

			// remove comments
			{
				char *comment = strchr( line, '#' );
				if( comment != NULL )
				{
					*comment = '\0';
				}
			}

			// remove leading whitespace
			{
				for( ; *line != '\0' && isspace(*line); line++ )
				{
				}
			}

			// remove trailing whitespace
			{
				char *end = line + strlen(line) - 1;
				for( ; end >= line && isspace(*end); end-- )
				{
					*end = '\0';
				}
			}

			// is it a non-empty string?
			if( *line != '\0' )
			{
				result = strdup( line );
			}

		} while( result == NULL );

		return result;
	}

	void Parser::tokenize( char *line, StringList &list )
	{
		char *saveptr;
		char *tok;

		for( ; (tok = strtok_r(line, " \t\n", &saveptr)) != NULL; line = NULL )
		{
			list.push_back( strdup(tok) );
		}
	}

	void Parser::processLine( Document *doc,
							  DocumentLocation &loc,
							  PropertyStack &propertyStack,
							  StringList &tokens )
	{
		size_t ntokens = tokens.size();
		string nameToken = tokens.front();
		string valueToken = tokens.back();
		

		if( valueToken == "]" )
		{
			if( ntokens != 1 )
			{
				loc.err( "Expecting only ']'" );
			}
			if( propertyStack.size() < 2 )
			{
				loc.err( "Extraneous ']'" );
			}

			
			if( !propertyStack.top()->isArray )
			{
				// close out the object for the last element.
				propertyStack.pop();

				if( !propertyStack.top()->isArray )
				{
					loc.err( "Unexpected ']'." );
				}

				if( propertyStack.size() < 2 )
				{
					loc.err( "Extraneous ']'" );
				}
			}
			else
			{
				Property *propArray = propertyStack.top();
				int nelements = (int)propArray->oval->size();

				if( nelements > 0 )
				{
					if( propArray->get(0).type == Property::OBJECT )
					{
						// we must have had a hanging ','. We force an empty object as a final element.
						propArray->add( new Property( loc, nelements ) );
					}
				}
			}

			propertyStack.pop();
		}
		else
		{
			bool isScalarArray = false;

			if( propertyStack.top()->isArray )
			{
				Property *propArray = propertyStack.top();

				// If this is the first element.
				if( propArray->oval->size() == 0 )
				{
					if( (ntokens == 1) && (valueToken != ",") )
					{
						isScalarArray = true;
					}
				}
				else
				{
					isScalarArray = propArray->get(0).type == Property::SCALAR;
				}

				if( !isScalarArray )
				{
					// we need a container for the coming element.
					size_t index = propertyStack.top()->oval->size();
					Property *prop = new Property( loc, index );
					propertyStack.top()->add( prop );
					propertyStack.push( prop );
				}
			}

			if( isScalarArray )
			{
				if( ntokens != 1 )
				{
					loc.err( "Expecting only 1 token in scalar array element." );
				}
				if( valueToken == "," )
				{
					loc.err( "Commas not allowed in scalar array." );
				}

				size_t index = propertyStack.top()->oval->size();
				propertyStack.top()->add( new Property(loc, index, valueToken.c_str()) );
			}
			else
			{
				if( valueToken == "{" )
				{
					if( ntokens != 2 )
					{
						loc.err( "Expecting 'NAME {'" );
					}

					string &objectName = nameToken;
			
					Property *prop = new Property( loc, objectName );
					propertyStack.top()->add( prop );
					propertyStack.push( prop );
				}
				else if( valueToken == "}" )
				{
					if( ntokens != 1 )
					{
						loc.err( "Expecting '}' only." );
					}

					if( propertyStack.size() < 2 )
					{
						loc.err( "Extraneous '}'" );
					}

					propertyStack.pop();
				}
				else if( valueToken == "[" )
				{
					if( ntokens != 2 )
					{
						loc.err( "Expecting 'NAME ['" );
					}

					string &arrayName = nameToken;
			
					Property *propArray = new Property( loc, arrayName, true );
					propertyStack.top()->add( propArray );
					propertyStack.push( propArray );
				}
				else if( valueToken == "," )
				{
					if( ntokens != 1 )
					{
						loc.err( "Expecting comma only." );
					}

					propertyStack.pop();
				}
				else
				{
					if( ntokens != 2 )
					{
						loc.err( "Expecting 'NAME VALUE'" );
					}

					string &propName = nameToken;
					string &value = valueToken;

					if( !isValidIdentifier(propName.c_str()) )
					{
						loc.err( "Invalid name: " + propName );
					}

					Property *prop = new Property( loc, propName, value.c_str() );
			
					propertyStack.top()->add( prop );
				}
			}
		}
	}

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Schema
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------

	void Schema::apply( Document *docSchema, Document *docValues )
	{
		validateChildren( *docSchema, *docValues );
	}

	void Schema::validateChildren( Property &propSchema, Property &propValue )
	{
		assert( propSchema.type == Property::OBJECT );
		assert( propValue.type == Property::OBJECT );

		itfor( Property::PropertyMap, *(propValue.oval), it )
		{
			Property *childValue = it->second;
			Property *childSchema = propSchema.getp( childValue->id );

			if( childSchema == NULL )
			{
				childValue->err( string(childValue->getName()) + " not in schema." );
			}

			validateProperty( *childSchema, *childValue );
		}
	}

	void Schema::validateProperty(  Property &propSchema, Property &propValue )
	{
		string name = propValue.getName();
		Property &propType = propSchema.get( "type" );
		string type = propType;

		// --
		// -- Verify the value is of the correct type.
		// --
		if( type == "INT" )
		{
			(int)propValue;
		}
		else if( type == "FLOAT" )
		{
			(float)propValue;
		}
		else if( type == "BOOL" )
		{
			(bool)propValue;
		}
		else if( type == "ARRAY" )
		{
			if( !propValue.isArray )
			{
				propValue.err( string("Expecting ARRAY value for property '") + propValue.getName() + "'" );
			}
		}
		else if( type == "OBJECT" )
		{
			if( (propValue.type != Property::OBJECT) || propValue.isArray )
			{
				propValue.err( string("Expecting OBJECT value for property '") + propValue.getName() + "'" );
			}
		}
		else if( type == "ENUM" )
		{
			Property &propEnumValues = propSchema.get( "values" );
			if( !propEnumValues.isArray )
			{
				propEnumValues.err( "Expecting array of enum values." );
			}

			bool valid = false;

			itfor( Property::PropertyMap, *(propEnumValues.oval), it )
			{
				if( (string)propValue == (string)*(it->second) )
				{
					valid = true;
					break;
				}
			}

			if( !valid )
			{
				Property *propScalar = propSchema.getp( "scalar" );
				if( propScalar )
				{
					string scalarType = propScalar->get( "type" );
					if( scalarType == "INT" || scalarType == "FLOAT" || scalarType == "BOOL" )
					{
						validateProperty( *propScalar, propValue );
						valid = true;
					}
					else
					{
						propScalar->err( "Invalid scalar type for enum." );
					}
				}
			}

			if( !valid )
			{
				propValue.err( "Invalid enum value." );
			}
		}
		else
		{
			propType.err( string("Invalid type '") + type + "'"  );
		}

		// --
		// -- Iterate over schema attributes for this property.
		// --
		itfor( Property::PropertyMap, *propSchema.oval, it )
		{
			string attrName = it->first.getName();
			Property &attrVal = *(it->second);

			// --
			// -- min
			// --
			if( attrName == "min" )
			{
				if( type == "ARRAY" )
				{
					bool valid = propValue.isArray
						&& (int)propValue.oval->size() >= (int)attrVal;

					if( !valid )
					{
						propValue.err( name + " element count less than min " + (string)attrVal );
					}
				}
				else
				{
					bool valid = true;

					if( type == "INT" )
					{
						valid = (int)propValue >= (int)attrVal;
					}
					else if( type == "FLOAT" )
					{
						valid = (float)propValue >= (float)attrVal;
					}
					else
					{
						propSchema.err( string("'min' is not valid for type ") + type );
					}

					if( !valid )
					{
						propValue.err( (string)propValue + " less than min " + (string)attrVal );
					}
				}
			}

			// --
			// -- xmin
			// --
			else if( attrName == "xmin" )
			{
				if( type == "ARRAY" )
				{
					bool valid = propValue.isArray
						&& (int)propValue.oval->size() > (int)attrVal;

					if( !valid )
					{
						propValue.err( name + " element count <= xmin " + (string)attrVal );
					}
				}
				else
				{
					bool valid = true;

					if( type == "INT" )
					{
						valid = (int)propValue > (int)attrVal;
					}
					else if( type == "FLOAT" )
					{
						valid = (float)propValue > (float)attrVal;
					}
					else
					{
						propSchema.err( string("'xmin' is not valid for type ") + type );
					}

					if( !valid )
					{
						propValue.err( (string)propValue + " <= xmin " + (string)attrVal );
					}
				}
			}

			// --
			// -- max
			// --
			else if( attrName == "max" )
			{
				if( type == "ARRAY" )
				{
					bool valid = propValue.isArray
						&& (int)propValue.oval->size() <= (int)attrVal;

					if( !valid )
					{
						propValue.err( name + " element count greater than max " + (string)attrVal );
					}
				}
				else
				{
					bool valid = true;

					if( type == "INT" )
					{
						valid = (int)propValue <= (int)attrVal;
					}
					else if( type == "FLOAT" )
					{
						valid = (float)propValue <= (float)attrVal;
					}
					else
					{
						propSchema.err( string("'max' is not valid for type ") + type );
					}

					if( !valid )
					{
						propValue.err( (string)propValue + " greater than max " + (string)attrVal );
					}
				}
			}

			// --
			// -- xmax
			// --
			else if( attrName == "xmax" )
			{
				if( type == "ARRAY" )
				{
					bool valid = propValue.isArray
						&& (int)propValue.oval->size() < (int)attrVal;

					if( !valid )
					{
						propValue.err( name + " element count >= than xmax " + (string)attrVal );
					}
				}
				else
				{
					bool valid = true;

					if( type == "INT" )
					{
						valid = (int)propValue < (int)attrVal;
					}
					else if( type == "FLOAT" )
					{
						valid = (float)propValue < (float)attrVal;
					}
					else
					{
						propSchema.err( string("'xmax' is not valid for type ") + type );
					}

					if( !valid )
					{
						propValue.err( (string)propValue + " >= xmax " + (string)attrVal );
					}
				}
			}

			// --
			// -- element
			// --
			else if( attrName == "element" )
			{
				if( type != "ARRAY" )
				{
					attrVal.err( "'element' only valid for ARRAY" );
				}

				itfor( Property::PropertyMap, *(propValue.oval), itelem )
				{
					validateProperty( attrVal, *itelem->second );
				}

			}

			// --
			// -- values & scalar
			// --
			else if( attrName == "values" || attrName == "scalar" )
			{
				// These attributes were handled prior to this loop.
				if( type != "ENUM" )
				{
					attrVal.err( string("Invalid schema attribute for non-ENUM type.") );
				}
			}

			// --
			// -- properties
			// --
			else if( attrName == "properties" )
			{
				if( type != "OBJECT" )
				{
					attrVal.err( "'properties' only valid for OBJECT type." );
				}

				validateChildren( attrVal, propValue );
			}

			// --
			// -- Invalid attr
			// --
			else if( (attrName != "type") && (attrName != "default") )
			{
				attrVal.err( string("Invalid schema attribute '") + attrName + "'" );
			}
		}
	}
}

#if 0
using namespace PropertyFile;

int main( int argc, char **argv )
{
	Document *docValues = Parser::parse( "values.txt" );
	Document *docSchema = Parser::parse( "schema.txt" );

	docValues->dump( cout );
	docSchema->dump( cout );

	Schema::apply( docSchema, docValues );

	delete docValues;
	delete docSchema;
	
	return 0;
}

#endif
