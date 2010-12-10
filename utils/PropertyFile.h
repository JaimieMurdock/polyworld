#pragma once

#include <istream>
#include <list>
#include <map>
#include <stack>

namespace PropertyFile
{
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Identifier
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	class Identifier
	{
	public:
		Identifier( const char *name );
		Identifier( int index );
		Identifier( const Identifier &other );

		~Identifier();

		const char *getName() const;

	private:
		char *name;
	};

	bool operator<( const Identifier &a, const Identifier &b );

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Property
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	class Property
	{
	public:
		enum Type
		{
			SCALAR,
			OBJECT
		};

		Property( Identifier id, bool _isArray = false );
		Property( Identifier id, const char *val );
		virtual ~Property();

		const char *getName();

		Property *get( Identifier id );
		
		bool get( Identifier id, bool def );
		int get( Identifier id, int def );

		Property &operator[]( Identifier id );

		void add( Property *prop );

		void dump( std::ostream &out, const char *indent = "" );

	private:
		friend class Parser;

		char *getscalar( Identifier id );

		Type type;
		Identifier id;
		bool isArray;

		typedef std::map<Identifier, Property *> PropertyMap;
		union
		{
			char *sval;
			PropertyMap *oval;
		};
	};

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Document
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	class Document : public Property
	{
	public:
		Document( Identifier id );
		virtual ~Document();
	};

	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	// --- CLASS Parser
	// ----------------------------------------------------------------------
	// ----------------------------------------------------------------------
	class Parser
	{
	public:
		static Document *parse( const char *path );

	private:
		typedef std::list<char *> StringList;
		typedef std::stack<Property *> PropertyStack;

		static char *readline( std::istream &in );
		static void tokenize( char *line, StringList &list );
		static void processLine( Document *doc,
								 PropertyStack &propertyStack,
								 StringList &tokens );
	};
};
