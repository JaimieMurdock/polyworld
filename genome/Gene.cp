#include "Gene.h"

#include <assert.h>

#include "Genome.h"
#include "GenomeSchema.h"
#include "misc.h"

using namespace genome;
using namespace std;


// ================================================================================
// ===
// === CLASS Gene
// ===
// ================================================================================
void Gene::seed( Genome *genome,
				 unsigned char rawval )
{
	assert( ismutable && offset > -1 );

	genome->set_raw( offset,
					 getMutableSize(),
					 rawval );
}

int Gene::getMutableSize()
{
	return ismutable ? getMutableSizeImpl() : 0;
}

void Gene::printIndexes( FILE *file )
{
	if( !ismutable )
	{
		return;
	}

	fprintf( file, "%d\t%s\n", offset, name.c_str() );	
}

void Gene::init( Type _type,
				 bool _ismutable,
				 const char *_name )
{
	schema = NULL;
	type = _type;
	ismutable = _ismutable;
	name = _name;
	offset = -1;
}

int Gene::getMutableSizeImpl()
{
	assert(false);
	return -1;
}

#define CAST_TO(TYPE)											\
	TYPE##Gene *Gene::to_##TYPE()								\
	{															\
		assert( this );											\
		TYPE##Gene *gene = dynamic_cast<TYPE##Gene *>( this );	\
		assert( gene ); /* catch cast failure */				\
		return gene;											\
	}

CAST_TO(NonVector);
CAST_TO(ImmutableScalar);
CAST_TO(MutableScalar);
CAST_TO(ImmutableInterpolated);
CAST_TO(NeurGroup);
CAST_TO(NeurGroupAttr);
CAST_TO(SynapseAttr);

#undef CAST_TO


// ================================================================================
// ===
// === CLASS __ConstantGene
// ===
// ================================================================================
__ConstantGene::__ConstantGene( Type _type,
								const char *_name,
								const Scalar &_value )
: value( _value )
{
	init( _type,
		  false /* immutable */,
		  _name );
}

const Scalar &__ConstantGene::get()
{
	return value;
}


// ================================================================================
// ===
// === CLASS __InterpolatedGene
// ===
// ================================================================================
__InterpolatedGene::__InterpolatedGene( Type _type,
										bool _ismutable,
										const char *_name,
										Gene *_gmin,
										Gene *_gmax,
										bool _round )
: smin( _gmin->to_ImmutableScalar()->get( NULL ) )
, smax( _gmax->to_ImmutableScalar()->get( NULL ) )
, round( _round )
{
	init( _type,
		  _ismutable,
		  _name );

	assert( smin.type == smax.type );
}

const Scalar &__InterpolatedGene::getMin()
{
	return smin;
}

const Scalar &__InterpolatedGene::getMax()
{
	return smax;
}

Scalar __InterpolatedGene::interpolate( unsigned char raw )
{
	static const float OneOver255 = 1. / 255.;

	// temporarily cast to double for backwards compatibility
	double ratio = float(raw) * OneOver255;

	switch(smin.type)
	{
	case Scalar::INT: {
		float result = interp( ratio, int(smin), int(smax) );
		if( round )
		{
			return nint( result );
		}
		else
		{
			return (int)result;
		}
	}
	case Scalar::FLOAT: {
		return (float)interp( ratio, float(smin), float(smax) );
	}
	default:
		assert(false);
	}
}


// ================================================================================
// ===
// === CLASS NonVectorGene
// ===
// ================================================================================
int NonVectorGene::getMutableSizeImpl()
{
	return sizeof(unsigned char);
}


// ================================================================================
// ===
// === CLASS ImmutableScalarGene
// ===
// ================================================================================
ImmutableScalarGene::ImmutableScalarGene( const char *name,
										  const Scalar &value )
: __ConstantGene( SCALAR,
				  name,
				  value )
{
}

Scalar ImmutableScalarGene::get( Genome *genome )
{
	return __ConstantGene::get();
}


// ================================================================================
// ===
// === CLASS MutableScalarGene
// ===
// ================================================================================
MutableScalarGene::MutableScalarGene( const char *name,
									  Gene *gmin,
									  Gene *gmax )
: __InterpolatedGene( SCALAR,
					  true /* mutable */,
					  name,
					  gmin,
					  gmax,
					  false /* don't round */)
{
}

Scalar MutableScalarGene::get( Genome *genome )
{
	return interpolate( genome->get_raw(offset) );	
}

const Scalar &MutableScalarGene::getMin()
{
	return __InterpolatedGene::getMin();
}

const Scalar &MutableScalarGene::getMax()
{
	return __InterpolatedGene::getMax();
}


// ================================================================================
// ===
// === CLASS ImmutableInterpolatedGene
// ===
// ================================================================================
ImmutableInterpolatedGene::ImmutableInterpolatedGene( const char *name,
													  Gene *gmin,
													  Gene *gmax )
: __InterpolatedGene( SCALAR,
					  false /* immutable */,
					  name,
					  gmin,
					  gmax,
					  true /* round */ )
{
}

Scalar ImmutableInterpolatedGene::interpolate( unsigned char raw )
{
	return __InterpolatedGene::interpolate( raw );
}


// ================================================================================
// ===
// === CLASS NeurGroupGene
// ===
// ================================================================================
NeurGroupGene::NeurGroupGene( NeurGroupType _group_type )
{
	first_group = -1; // this will be set by the schema
	group_type = _group_type;

	switch(group_type)
	{
	case NGT_INPUT:
	case NGT_OUTPUT:
	case NGT_INTERNAL:
		// okay
		break;
	default:
		assert(false);
	}
}

bool NeurGroupGene::isMember( NeurGroupType _group_type )
{
	if( (_group_type == NGT_ANY) || (_group_type == this->group_type) )
	{
		return true;
	}

	switch( this->group_type )
	{
	case NGT_INPUT:
		return false;
	case NGT_OUTPUT:
	case NGT_INTERNAL:
		return (_group_type == NGT_ANY) || (_group_type == NGT_NONINPUT);
	default:
		assert(false);
	}
}

NeurGroupType NeurGroupGene::getGroupType()
{
	return group_type;
}


// ================================================================================
// ===
// === CLASS MutableNeurGroupGene
// ===
// ================================================================================
MutableNeurGroupGene::MutableNeurGroupGene( const char *_name,
											NeurGroupType _group_type,
											Gene *_gmin,
											Gene *_gmax )
: NeurGroupGene( _group_type )
, __InterpolatedGene( NEURGROUP,
					  true /* mutable */,
					  _name,
					  _gmin,
					  _gmax,
					  true /* round */ )
{
}

Scalar MutableNeurGroupGene::get( Genome *genome )
{
	return interpolate( genome->get_raw(offset) );
}

int MutableNeurGroupGene::getMaxGroupCount()
{
	switch( getGroupType() )
	{
	case NGT_INPUT:
		return 1;
	case NGT_INTERNAL: {
		return getMax();
	}
	default:
		assert(false);
	}
}

int MutableNeurGroupGene::getMaxNeuronCount()
{
	switch( getGroupType() )
	{
	case NGT_INPUT:
		return getMax();
	case NGT_INTERNAL: {
		int ngroups = getMaxGroupCount();
		int numineur = schema->get("InhibitoryNeuronCount")->to_NeurGroupAttr()->getMax();
		int numeneur = schema->get("ExcitatoryNeuronCount")->to_NeurGroupAttr()->getMax();

		return ngroups * (numineur + numeneur);
	}
	default:
		assert(false);
	}
}


// ================================================================================
// ===
// === CLASS ImmutableNeurGroupGene
// ===
// ================================================================================
ImmutableNeurGroupGene::ImmutableNeurGroupGene( const char *_name,
												NeurGroupType _group_type )
: NeurGroupGene( _group_type )
, __ConstantGene( NEURGROUP,
				  _name,
				  1 )
{
}

Scalar ImmutableNeurGroupGene::get( Genome *genome )
{
	return 1;
}

int ImmutableNeurGroupGene::getMaxGroupCount()
{
	return 1;
}

int ImmutableNeurGroupGene::getMaxNeuronCount()
{
	return 1;
}


// ================================================================================
// ===
// === CLASS NeurGroupAttrGene
// ===
// ================================================================================
NeurGroupAttrGene::NeurGroupAttrGene( const char *_name,
									  NeurGroupType _group_type,
									  Gene *_gmin,
									  Gene *_gmax )
: __InterpolatedGene( NEURGROUP_ATTR,
					  true /* mutable */,
					  _name,
					  _gmin,
					  _gmax,
					  true /* round */ )
, group_type( _group_type )
{
}

const Scalar &NeurGroupAttrGene::getMin()
{
	return __InterpolatedGene::getMin();
}

const Scalar &NeurGroupAttrGene::getMax()
{
	return __InterpolatedGene::getMax();
}

Scalar NeurGroupAttrGene::get( Genome *genome,
							   int group )
{
	int offset = getOffset( group );

	return interpolate( genome->get_raw(offset) );
}

void NeurGroupAttrGene::seed( Genome *genome,
							  NeurGroupGene *group,
							  unsigned char rawval )
{
	int igroup = schema->getFirstGroup( group );
	int offset = getOffset( igroup );
	int ngroups = group->getMaxGroupCount();

	genome->set_raw( offset,
					 ngroups * sizeof(unsigned char),
					 rawval );
}

void NeurGroupAttrGene::printIndexes( FILE *file )
{
	int n = schema->getMaxGroupCount( group_type );

	for( int i = 0; i < n; i++ )
	{
		fprintf( file, "%d\t%s_%d\n", offset + i, name.c_str(), i );
	}
}

int NeurGroupAttrGene::getMutableSizeImpl()
{
	return sizeof(unsigned char) * schema->getMaxGroupCount( group_type );
}

int NeurGroupAttrGene::getOffset( int group )
{
	return this->offset + (group - schema->getFirstGroup( group_type ));
}


// ================================================================================
// ===
// === CLASS SynapseAttrGene
// ===
// ================================================================================
SynapseAttrGene::SynapseAttrGene( const char *_name,
								  bool _negateInhibitory,
								  bool _lessThanZero,
								  Gene *_gmin,
								  Gene *_gmax )
: __InterpolatedGene( SYNAPSE_ATTR,
					  true /* mutable */,
					  _name,
					  _gmin,
					  _gmax,
					  true /* round */ )
, negateInhibitory( _negateInhibitory )
, lessThanZero( _lessThanZero )
{
}

Scalar SynapseAttrGene::get( Genome *genome,
							 SynapseType *synapseType,
							 int group_from,
							 int group_to )
{
	int offset = getOffset( synapseType,
							group_from,
							group_to );

	Scalar result = interpolate( genome->get_raw(offset) );

	if( negateInhibitory && synapseType->nt_from == INHIBITORY )
	{
		if( lessThanZero )
		{
			// value must be < 0
			return min(-1.e-10, -(double)result);
		}
		else
		{
			return -(float)result;
		}
	}
	else
	{
		return result;
	}
}

void SynapseAttrGene::seed( Genome *genome,
							SynapseType *synapseType,
							NeurGroupGene *from,
							NeurGroupGene *to,
							unsigned char rawval )
{
	assert( (from->getGroupType() == NGT_INPUT) || (from->getGroupType() == NGT_OUTPUT) );
	assert( to->getGroupType() == NGT_OUTPUT );

	int offset = getOffset( synapseType,
							schema->getFirstGroup(from),
							schema->getFirstGroup(to) );

	genome->set_raw( offset,
					 sizeof(unsigned char),
					 rawval );
}

void SynapseAttrGene::printIndexes( FILE *file )
{
	int nin = schema->getMaxGroupCount( NGT_INPUT );
	int nany = schema->getMaxGroupCount( NGT_ANY );

	for( SynapseTypeList::const_iterator
			 it = schema->getSynapseTypes().begin(),
			 end = schema->getSynapseTypes().end();
		 it != end;
		 it++ )
	{
		SynapseType *st = *it;

		for( int to = nin; to < nany; to++ )
		{
			for( int from = 0; from < nany; from++ )
			{
				fprintf( file, "%d\t%s%s_%d->%d\n",
						 getOffset( st, from, to ),
						 st->name.c_str(), this->name.c_str(),
						 from, to );
			}
		}
	}	
}

int SynapseAttrGene::getMutableSizeImpl()
{
	int size = 0;

	for( SynapseTypeList::const_iterator
			 it = schema->getSynapseTypes().begin(),
			 end = schema->getSynapseTypes().end();
		 it != end;
		 it++ )
	{
		size += (*it)->getMutableSize();
	}

	return size;
}

int SynapseAttrGene::getOffset( SynapseType *synapseType,
								int from,
								int to )
{
	return this->offset + synapseType->getOffset( from, to );
}
