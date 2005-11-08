/********************************************************************/
/* PolyWorld:  An Artificial Life Ecological Simulator              */
/* by Larry Yaeger                                                  */
/* Copyright Apple Computer 1990,1991,1992                          */
/********************************************************************/

// Self
#include "genome.h"

// System
#include <iostream>

// qt
#include <qapplication.h>

// Local
#include "brain.h"
#include "error.h"
#include "globals.h"
#include "graybin.h"
#include "misc.h"

#ifdef __ALTIVEC__
	// Turn pw_UseAltivec on to use ALTIVEC if it's available
	#define pw_UseAltivec 0
#endif

#if pw_UseAltivec
	// veclib
	#include <vecLib/vBLAS.h>
#endif

using namespace std;

//#define DUMPBITS

#pragma mark -

// Internal globals
bool genome::classinited;
long genome::gNumBytes;
long genome::numphysbytes;
long genome::mrategene;
long genome::ncptsgene;
long genome::lifespangene;
long genome::idgene;
long genome::strengthgene;
long genome::sizegene;
long genome::maxspeedgene;
long genome::mateenergygene;
long genome::numrneurgene;
long genome::numgneurgene;
long genome::numbneurgene;
long genome::numneurgroupsgene;
long genome::numeneurgene;
long genome::numineurgene;
long genome::biasgene;
long genome::biaslrategene;
long genome::eecdgene;
long genome::eicdgene;
long genome::iicdgene;
long genome::iecdgene;
long genome::eelrgene;
long genome::eilrgene;
long genome::iilrgene;
long genome::ielrgene;
long genome::eetdgene;
long genome::eitdgene;
long genome::iitdgene;
long genome::ietdgene;
long* genome::gCrossoverPoints;


// External globals
float genome::gMinStrength;
float genome::gMaxStrength;
float genome::gMinMutationRate;
float genome::gMaxMutationRate;
long genome::gMinNumCpts;
long genome::gMaxNumCpts;
long genome::gMinLifeSpan;
long genome::gMaxLifeSpan;
float genome::gMiscBias;
float genome::gMiscInvisSlope;
float genome::gMinBitProb;
float genome::gMaxBitProb;
bool genome::gGrayCoding;
short genome::gMinvispixels;
short genome::gMaxvispixels;
float genome::gMinmateenergy;
float genome::gMaxmateenergy;
float genome::gMinmaxspeed;
float genome::gMaxmaxspeed;
float genome::gMinlrate;
float genome::gMaxlrate;

float OneOver255 = 1. / 255.;


#pragma mark -

//===========================================================================
// genome
//===========================================================================

//---------------------------------------------------------------------------
// genome::genomeinit
//---------------------------------------------------------------------------
void genome::genomeinit()
{
    if (genome::classinited)
        return;

    genome::classinited = true;
    
    long geneVal = 0;
    genome::mrategene = geneVal++;
    genome::ncptsgene = geneVal++;
    genome::lifespangene = geneVal++;
    genome::idgene = geneVal++;
    genome::strengthgene = geneVal++;
    genome::sizegene = geneVal++;
    genome::maxspeedgene = geneVal++;
    genome::mateenergygene = geneVal++;
    genome::numphysbytes = genome::mateenergygene + 1;	// not geneVal++;

    // number of neurons in red vision input group
    genome::numrneurgene = geneVal++;
    
    // number of neurons in green vision input group
    genome::numgneurgene = geneVal++;
    
    // number of neurons in blue vision input group
    genome::numbneurgene = geneVal++;;

    // Note the distinction between "non-input" neuronal groups and
    // the 5 input groups -- 3 groups of vision neurons (r,g,b), the
    // energy-neuron "group" (of one), and the random-neuron "group"
    // (again, of size one).  The input neurons do not themselves
    // accept inputs, though they obviously do generate outputs.
    // This is the reason for the odd sizing of the connection
    // density, learning rate, and topological distortion groupings
    // below (maxneurgroups*maxnoninputneurgroups).
    // Input neurons double as both excitatory and inhibitory
    // neurons (as do output/behavior neurons), but only when they
    // are the pre-synaptic source neuron.  As post-synaptic target
    // neurons, they are exclusively excitatory.  This is a bit of
    // a hack to permit them to be used as inhibitors, without
    // going through a separate, signal-inverting group, primarily
    // for the sake of computational efficiency.

    // Correspondingly, since the last few (7 originally) neuronal groups
    // are designated to be the "output" (behavior) groups, the number of
    // neurons (again doubling as both excitatory and inhibitory) in
    // those groups are fixed (1); hence only maxneurgroups-numoutneurgroups
	// specifications are needed to specify all the non-input group sizes.
	// On the other hand, the output group's neurons *are* allowed to feedback
    // their activation levels to other non-input groups, hence their
    // inclusion as just another non-input group (rather than
    // separating them as is done with the input groups).
    // Also, the output neurons need a bias and biaslearningrate, just
    // like the rest of the non-input neurons, hence the full number
    // of maxneurgroups values must be specified for those parameters.

    // number of internal neuronal groups
    genome::numneurgroupsgene = genome::numbneurgene + 1;
    
    // number of excitatory neurons (per group)
    genome::numeneurgene = genome::numneurgroupsgene + 1;
    
    // number of inhibitory neurons (per group)
    genome::numineurgene = genome::numeneurgene + brain::gNeuralValues.maxinternalneurgroups;

    // bias level for all neurons in this group
    genome::biasgene = genome::numineurgene + brain::gNeuralValues.maxinternalneurgroups;
    
    // bias learning rate for all neurons in this group
    genome::biaslrategene = genome::biasgene + brain::gNeuralValues.maxnoninputneurgroups;

    // excitatory-to-excitatory connection density        (for pairs of groups)
    // excitatory-to-inhibitory connection density        (for pairs of groups)
    // inhibitory-to-inhibitory connection density        (for pairs of groups)
    // inhibitory-to-excitatory connection density        (for pairs of groups)
    genome::eecdgene = genome::biaslrategene + brain::gNeuralValues.maxnoninputneurgroups;
    genome::eicdgene = genome::eecdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::iicdgene = genome::eicdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::iecdgene = genome::iicdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;

    // excitatory-to-excitatory learning rate             (for pairs of groups)
    // excitatory-to-inhibitory learning rate             (for pairs of groups)
    // inhibitory-to-inhibitory learning rate             (for pairs of groups)
    // inhibitory-to-excitatory learning rate             (for pairs of groups)
    genome::eelrgene = genome::iecdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::eilrgene = genome::eelrgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::iilrgene = genome::eilrgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::ielrgene = genome::iilrgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;

    // excitatory-to-excitatory topological distortion    (for pairs of groups)
    // excitatory-to-inhibitory topological distortion    (for pairs of groups)
    // inhibitory-to-inhibitory topological distortion    (for pairs of groups)
    // inhibitory-to-excitatory topological distortion    (for pairs of groups)
    genome::eetdgene = genome::ielrgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::eitdgene = genome::eetdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::iitdgene = genome::eitdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;
    genome::ietdgene = genome::iitdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;

    genome::gNumBytes = genome::ietdgene + brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups;

    genome::gCrossoverPoints = new long[gMaxNumCpts + 1];
    Q_CHECK_PTR(genome::gCrossoverPoints);
    
	// may someday want to make the learning rate an evolvable schedule
	// may also want to support overgrowth and dieback of neural connections,
	// both here and in the brain class.


}


//---------------------------------------------------------------------------
// genome::PrintGeneIndexes
//---------------------------------------------------------------------------
void genome::PrintGeneIndexes( FILE* f )
{
	if( !f )
	{
		fprintf( stderr, "%s called with NULL file pointer\n", __FUNCTION__ );
		return;
	}
	
	// Physiology genes
	fprintf( f, "%d\tGeneCount\t%d\tPhysiologyGeneCount\t%d\tNeurophysiologyGeneCount\n", gNumBytes, numphysbytes, gNumBytes - numphysbytes );
	fprintf( f, "%d\tMutationRate\n", mrategene );
	fprintf( f, "%d\tControlPointCount\n", ncptsgene );
	fprintf( f, "%d\tLifeSpan\n", lifespangene );
	fprintf( f, "%d\tID\n", idgene );
	fprintf( f, "%d\tStrength\n", strengthgene );
	fprintf( f, "%d\tSize\n", sizegene );
	fprintf( f, "%d\tMaxSpeed\n", maxspeedgene );
	fprintf( f, "%d\tMateEnergyFraction\n", mateenergygene );

	// Input (Vision) genes
	fprintf( f, "%d\tRedNeuronCount\n", numrneurgene );
	fprintf( f, "%d\tGreenNeuronCount\n", numgneurgene );
	fprintf( f, "%d\tBlueNeuronCount\n", numbneurgene );;

	// Neurophysiology genes
	// Per group
	// Internal groups only
	fprintf( f, "%d\tInternalNeuronGroupCount\n", numneurgroupsgene );
	for( int i = 0; i < brain::gNeuralValues.maxinternalneurgroups; i++ )
		fprintf( f, "%d\tExcitatoryNeuronCount_%d\n", numeneurgene+i, i );
	for( int i = 0; i < brain::gNeuralValues.maxinternalneurgroups; i++ )
		fprintf( f, "%d\tInhibitoryNeuronCount_%d\n", numineurgene+i, i );
	// Internal plus output groups (all non-input groups)
	for( int i = 0; i < brain::gNeuralValues.maxnoninputneurgroups; i++ )
		fprintf( f, "%d\tBias_%d\n", biasgene+i, i );
	for( int i = 0; i < brain::gNeuralValues.maxnoninputneurgroups; i++ )
		fprintf( f, "%d\tBiasLearningRate_%d\n", biaslrategene+i, i );

	// Per pair of groups
	// Connection density
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tEEConnectionDensity_%d->%d\n", eecdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tEIConnectionDensity_%d->%d\n", eicdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tIIConnectionDensity_%d->%d\n", iicdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tIEConnectionDensity_%d->%d\n", iecdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );

	// Learning rate
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tEELearningRate_%d->%d\n", eelrgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tEILearningRate_%d->%d\n", eilrgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tIILearningRate_%d->%d\n", iilrgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tIELearningRate_%d->%d\n", ielrgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );

	// Topological distortion
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tEETopologicalDistortion_%d->%d\n", eetdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tEITopologicalDistortion_%d->%d\n", eitdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tIITopologicalDistortion_%d->%d\n", iitdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
	for( int j = brain::gNeuralValues.numinputneurgroups; j < brain::gNeuralValues.maxneurgroups; j++ )	// target group
		for( int i = 0; i < brain::gNeuralValues.maxneurgroups; i++ )	// source group
			fprintf( f, "%d\tIETopologicalDistortion_%d->%d\n", ietdgene + i + (j - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups, i, j );
}


//---------------------------------------------------------------------------
// genome::genomedestruct
//---------------------------------------------------------------------------
void genome::genomedestruct()
{
	delete genome::gCrossoverPoints;
}


//---------------------------------------------------------------------------
// genome::genome
//---------------------------------------------------------------------------
genome::genome()
	:	fGenes(NULL)
{
	Init();
} 


//---------------------------------------------------------------------------
// genome::~genome
//---------------------------------------------------------------------------       
genome::~genome()
{
	delete fGenes;
}
 

//---------------------------------------------------------------------------
// genome::Init
//---------------------------------------------------------------------------    
void genome::Init(bool randomized)
{
    if (!classinited)
        genomeinit();
        
    fGenes = new unsigned char[genome::gNumBytes];
    Q_CHECK_PTR(fGenes);

	if (randomized)       
		Randomize();
}


//---------------------------------------------------------------------------
// genome::Dump
//---------------------------------------------------------------------------
void genome::Dump(std::ostream& out)
{
    for (long i = 0; i < genome::gNumBytes; i++)
        out << (int)(fGenes[i]) nl;
}


//---------------------------------------------------------------------------
// genome::Load
//---------------------------------------------------------------------------
void genome::Load(std::istream& in)
{
    int num = 0;
    for (long i = 0; i < genome::gNumBytes; i++)
    {
        in >> num;
        fGenes[i] = (unsigned char)num;
    }
}


//---------------------------------------------------------------------------
// genome::Randomize
//---------------------------------------------------------------------------
void genome::Randomize(float bitonprob)
{
	// do a random initialization of the bitstring
    for (long byte = 0; byte < genome::gNumBytes; byte++)
    {
        for (long bit = 0; bit < 8; bit++)
        {
            if (drand48() < bitonprob)
                fGenes[byte] |= char(1 << (7-bit));
            else
                fGenes[byte] &= char(255 ^ (1 << (7-bit)));
		}
	}		              
}

//---------------------------------------------------------------------------
// genome::Randomize
//---------------------------------------------------------------------------
void genome::Randomize()
{
#if DesignerGenes
//check crossover point array size, whether the gene value is just non-physiological or not, whether that is accounted for
	SeedGenes();
#else
	Randomize(gMinBitProb + drand48() * (gMaxBitProb - gMinBitProb));
#endif
}


//---------------------------------------------------------------------------
// genome::SeedGenes
//---------------------------------------------------------------------------
void genome::SeedGenes()
{
	// Assumes the minimum number of internal neural groups is 0, and uses binary gene coding (as opposed to gray-coding)
	fGenes[mrategene]			= 0;	// about 0.1
	fGenes[ncptsgene]			= 127;	// about 5
	fGenes[lifespangene]		= 127;	// about 750
	fGenes[idgene]				= 127;	// 127
	fGenes[strengthgene]		= 127;	// about 1.25
	fGenes[sizegene]			= 127;	// about 1.25
	fGenes[maxspeedgene]		= 127;	// about 1.0
	fGenes[mateenergygene]		= 127;	// about 0.5
	fGenes[numrneurgene]		= 127;	// about 8 red neurons
	fGenes[numgneurgene]		= 127;	// about 8 green neurons
	fGenes[numbneurgene]		= 127;	// about 8 blue neurons
	fGenes[numneurgroupsgene]	= 0;	// internal neural groups will be zero if mininternalneurgroups is set to zero in worldfile
	for( int i = 0; i < brain::gNeuralValues.maxinternalneurgroups; i++ )
	{
		fGenes[numeneurgene+i]	= 0;	// won't matter if there aren't any
		fGenes[numineurgene+i]	= 0;	// ditto
	}
	for( int i = 0; i < brain::gNeuralValues.maxnoninputneurgroups; i++ )
	{
		fGenes[biasgene+i]		= 127;	// about 0.0 bias
		fGenes[biaslrategene+i]	= 0;	// about 0.0 bias learning rate
	}
	fGenes[biasgene+brain::gNeuralValues.maxinternalneurgroups+1] = 255;	// always want to mate
	for( int i = 0; i < brain::gNeuralValues.maxneurgroups * brain::gNeuralValues.maxnoninputneurgroups; i++ )
	{
		fGenes[eecdgene+i] = 0;	// 1,   but won't matter for the non-existent internal groups
		fGenes[eicdgene+i] = 0;	// 1,   but won't matter for the non-existent internal groups
		fGenes[iicdgene+i] = 0;	// 1,   but won't matter for the non-existent internal groups
		fGenes[iecdgene+i] = 0;	// 1,   but won't matter for the non-existent internal groups
		fGenes[eelrgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
		fGenes[eilrgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
		fGenes[iilrgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
		fGenes[ielrgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
		fGenes[eetdgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
		fGenes[eitdgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
		fGenes[iitdgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
		fGenes[ietdgene+i] = 0;	// 0.0, but won't matter for the non-existent internal groups
	}
	// Now establish the few connections we actually want, between the vision neurons and key behaviors
	fGenes[eecdgene + (7 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 2] = 255;	// full e-e connectivity from red vision (2) to fight behavior (7)
	fGenes[eecdgene + (5 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 3] = 255;	// full e-e connectivity from green vision (3) to eat behavior (5)
	fGenes[eecdgene + (6 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 4] = 255;	// full e-e connectivity from blue vision (4) to mate behavior (6)
	fGenes[iecdgene + (8 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 2] = 127;	// 1/2  i-e connectivity from red vision (2) to speed (move) behavior (8)
	fGenes[ietdgene + (8 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 2] = 255;	// maximum topo distortion, so it's random red pixels that force a slow-down
	fGenes[eecdgene + (8 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 3] = 127;	// 1/2  e-e connectivity from green vision (3) to speed (move) behavior (8)
	fGenes[eetdgene + (8 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 3] = 255;	// maximum topo distortion, so it's random green pixels that force a speed-up
	fGenes[eecdgene + (8 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 4] = 127;	// 1/2  e-e connectivity from blue vision (4) to speed (move) behavior (8)
	fGenes[eetdgene + (8 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 4] = 255;	// maximum topo distortion, so it's random blue pixels that force a speed-up
	fGenes[eecdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 2] = 127;	// 1/2  e-e connectivity from red vision (2) to yaw behavior (9) [left pixels on causes left turn]
	fGenes[iecdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 2] = 127;	// 1/2  i-e connectivity from red vision (2) to yaw behavior (9) [some pixels cause right turn, with top. dist.]
	fGenes[ietdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 2] = 255;	// maximum topo distortion, so some red pixels are likely to connect to right side
	fGenes[eecdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 3] = 127;	// 1/2  e-e connectivity from green vision (3) to yaw behavior (9) [left pixels on causes left turn]
	fGenes[iecdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 3] = 127;	// 1/2  i-e connectivity from green vision (3) to yaw behavior (9) [some pixels casue right turn, with top. dist.]
	fGenes[ietdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 3] = 255;	// maximum topo distortion, so some green pixels are likely to connect to right side
	fGenes[eecdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 4] = 127;	// 1/2  e-e connectivity from blue vision (4) to yaw behavior (9) [left pixels on causes left turn]
	fGenes[iecdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 4] = 127;	// 1/2  i-e connectivity from blue vision (4) to yaw behavior (9) [some pixels casue right turn, with top. dist.]
	fGenes[ietdgene + (9 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 4] = 255;	// maximum topo distortion, so some blue pixels are likely to connect to right side
	fGenes[iecdgene + (7 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 5] = 255;	// full i-e connectivity from eat behavior (5) to fight behavior (7) ["full" is one to one for two behavior groups]
	fGenes[iecdgene + (7 - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + 6] = 255;	// full i-e connectivity from mate behavior (6) to fight behavior (7) ["full" is one to one for two behavior groups]
//	Print();
}


//---------------------------------------------------------------------------
// genome::Mutate
//---------------------------------------------------------------------------
// Mutate this genome (mutation rate is one of the genes)
// genome* crossover(const genome* g);
// given second genome, g, return ptr to
// new genome after performing crossover
void genome::Mutate()
{
    float rate = MutationRate();
    for (long byte = 0; byte < genome::gNumBytes; byte++)
    {
        for (long bit = 0; bit < 8; bit++)
        {
            if (drand48() < rate)
                fGenes[byte] ^= char(1 << (7-bit));
        }
    }
}


//---------------------------------------------------------------------------
// genome::Crossover
//---------------------------------------------------------------------------
void genome::Crossover(genome* g1, genome* g2, bool mutate)
{
	Q_ASSERT(g1 != NULL && g2 != NULL);
	Q_ASSERT(g1 != g2);
	Q_ASSERT(fGenes != NULL);
	
#if DesignerGenesOnly
	// If we're only allowing the original DesignerGenes, then make sure that's what we get.
	Randomize();	// will enforce DesignerGenes, if enabled
	return;
#endif
	
    // Randomly select number of crossover points from chosen genome
    long numCrossPoints;
    if (drand48() < 0.5)
        numCrossPoints = g1->CrossoverPoints();
    else
		numCrossPoints = g2->CrossoverPoints();
    
    // guarantee crossover in "physiology" genes
    gCrossoverPoints[0] = long(drand48() * numphysbytes * 8 - 1);
    gCrossoverPoints[1] = numphysbytes * 8;

	// Sanity checking
	Q_ASSERT(numCrossPoints <= gMaxNumCpts);
	
	// Generate & order the crossover points.
	// Start iteration at [2], as [0], [1] set above
    long i, j;
    
    for (i = 2; i <= numCrossPoints; i++) 
    {
        long newCrossPoint = long(drand48() * (genome::gNumBytes - numphysbytes) * 8 - 1) + gCrossoverPoints[1];
        bool equal;
        do
        {
            equal = false;
            for (j = 1; j < i; j++)
            {
                if (newCrossPoint == gCrossoverPoints[j])
                    equal = true;
            }
            
            if (equal)
                newCrossPoint = long(drand48() * (genome::gNumBytes - numphysbytes) * 8 - 1) + gCrossoverPoints[1];
                
        } while (equal);
        
        if (newCrossPoint > gCrossoverPoints[i-1])
        {
            gCrossoverPoints[i] = newCrossPoint;  // happened to come out ordered
		}           
        else
        {
            for (j = 2; j < i; j++)
            {
                if (newCrossPoint < gCrossoverPoints[j])
                {
                    for (long k = i; k > j; k--)
                        gCrossoverPoints[k] = gCrossoverPoints[k-1];
                    gCrossoverPoints[j] = newCrossPoint;
                    break;
                }
            }
        }
    }
    
#ifdef DUMPBITS    
    cout << "**The crossover bits(bytes) are:" nl << "  ";
    for (i = 0; i <= numCrossPoints; i++)
    {
        long byte = gCrossoverPoints[i] >> 3;
        cout << gCrossoverPoints[i] << "(" << byte << ") ";
    }
    cout nlf;
#endif
    
	float mrate = 0.0;
    if (mutate)
    {
        if (drand48() < 0.5)
            mrate = g1->MutationRate();
        else
            mrate = g2->MutationRate();
    }
    
    long begbyte = 0;
    long endbyte = -1;
    long bit;
    bool first = (drand48() < 0.5);
    const genome* g;
    
	// now do crossover using the ordered pts
    for (i = 0; i <= numCrossPoints + 1; i++)
    {
		// for copying the end of the genome
        if (i == numCrossPoints + 1)
        {
            if (endbyte == genome::gNumBytes - 1)
            {
            	// already copied last byte (done) so just get out of the loop
                break;
			}             
                
            endbyte = genome::gNumBytes - 1;
        }
        else
        {
            endbyte = gCrossoverPoints[i] >> 3;
		}
        g = first ? g1 : g2;
        
#ifdef DUMPBITS    
        cout << "**copying bytes " << begbyte << " to " << endbyte
             << " from the ";
        if (first)
            cout << "first genome" nl;
        else
            cout << "second genome" nl;
        cout.flush();
#endif

        if (mutate)
        {
            for (j = begbyte; j < endbyte; j++)
            {
                fGenes[j] = g->fGenes[j];    // copy from the appropriate genome
                for (bit = 0; bit < 8; bit++)
                {
                    if (drand48() < mrate)
                        fGenes[j] ^= char(1 << (7-bit));	// this goes left to right, corresponding more directly to little-endian machines, but leave it alone (at least for now)
                }
            }
        }
        else
        {
            for (j = begbyte; j < endbyte; j++)
                fGenes[j] = g->fGenes[j];    // copy from the appropriate genome
        }
        
        if (i != (numCrossPoints + 1))  // except on the last stretch...
        {
            first = !first;
            bit = gCrossoverPoints[i] - (endbyte << 3);
            
            if (first)
            {	// this goes left to right, corresponding more directly to little-endian machines, but leave it alone (at least for now)
                fGenes[endbyte] = char((g2->fGenes[endbyte] & (255 << (8 - bit)))
                   					 | (g1->fGenes[endbyte] & (255 >> bit)));
			}                  
            else
            {	// this goes left to right, corresponding more directly to little-endian machines, but leave it alone (at least for now)
                fGenes[endbyte] = char((g1->fGenes[endbyte] & (255 << (8 - bit)))
                   					 | (g2->fGenes[endbyte] & (255 >> bit)));
			}
						              
            begbyte = endbyte + 1;
        }
        else
        {
            fGenes[endbyte] = g->fGenes[endbyte];
		}
		            
        if (mutate)
        {
            for (bit = 0; bit < 8; bit++)
            {
                if (drand48() < mrate)
                    fGenes[endbyte] ^= char(1 << (7 - bit));	// this goes left to right, corresponding more directly to little-endian machines, but leave it alone (at least for now)
            }
        }
    }
}


//---------------------------------------------------------------------------
// genome::CopyGenes
//---------------------------------------------------------------------------
void genome::CopyGenes(genome* sgenome)
{
	unsigned char* sg = sgenome->fGenes;
	unsigned char* tg = fGenes;
	
	for (long i = 0; i < genome::gNumBytes; i++)
		*(tg++) = *(sg++);
}


//---------------------------------------------------------------------------
// genome::CalcSeparation
//---------------------------------------------------------------------------
float genome::CalcSeparation(genome* g)
{
#if pw_UseAltivec
	#warning compiling with (broken) ALTIVEC code
	long local_numbytes = genome::gNumBytes;
#else
	int sep = 0;
#endif
	float fsep = 0.f;
    unsigned char* gi = fGenes;
    unsigned char* gj = g->fGenes;
    
    if (gGrayCoding)
    {
#if pw_UseAltivec
		float val_gi[ genome::gNumBytes ];
		float val_gj[ genome::gNumBytes ];
		short size = 4;

                long max = local_numbytes / size;
                long left = local_numbytes - (max * size);

		float result[ max * size ];

		for (long i = 0; i < local_numbytes; i++)
		{
			val_gi[i] = binofgray[gi[i]];
			val_gj[i] = binofgray[gj[i]];
		}
		

		for (long i = 0; i < max; i++)
		{
			vector float vGi = vec_ld(size, val_gi + size * i);
			vector float vGj = vec_ld(size, val_gj + size * i);
			
			vector float diff = vec_sub(vGi, vGj);
			
			vec_st(diff, size, result + i * size);
			
		}
		
		fsep = cblas_sasum(size * max, result, 1);
		vector float vGi = vec_ld(left, val_gi + size * max);
		vector float vGj = vec_ld(left, val_gj + size * max);
		
		vector float diff = vec_sub(vGi, vGj);
		vector float absdiff = vec_abs(diff);
		
		vec_st(absdiff, left, result);
		
		for (long j = 0; j < left ;  j++)
		{
			fsep += result[j];
		}
#else
    for (long i = 0; i < genome::gNumBytes; i++)
        {
            short vi, vj;
            vi = binofgray[ * (gi++)];
            vj = binofgray[ * (gj++)];
            sep += abs(vi - vj);
        }
#endif
    }
    else
    {
#if pw_UseAltivec
		float val_gi[ genome::gNumBytes ];
		float val_gj[ genome::gNumBytes ];
		short size = 4;
		
		long max = local_numbytes / size;
		long left = local_numbytes - (max * size);
		
		float result[ max * size ];
		
		for (long i = 0; i < local_numbytes; i++)
		{
			val_gi[i] = gi[i];
			val_gj[i] = gj[i];
		}
		
		
		for (long i = 0; i < max; i++)
		{
			vector float vGi = vec_ld(size, val_gi + size * i);
			vector float vGj = vec_ld(size, val_gj + size * i);
			
			vector float diff = vec_sub(vGi, vGj);
			
			vec_st(diff, size, result + i * size);
			
		}
		
		fsep = cblas_sasum(size * max, result, 1);
		vector float vGi = vec_ld(left, val_gi + size * max);
		vector float vGj = vec_ld(left, val_gj + size * max);
		
		vector float diff = vec_sub(vGi, vGj);
		vector float absdiff = vec_abs(diff);
		
		vec_st(absdiff, left, result);
		
		for (long j = 0; j < left ;  j++)
		{
			fsep += result[j];
		}
#else
        for (long i = 0; i < genome::gNumBytes; i++)
        {
            short vi, vj;
            vi = * (gi++);
            vj = * (gj++);
            sep += abs(vi - vj);
        }
#endif
    }
#if pw_UseAltivec
    fsep /= (255.0f * float(local_numbytes));
    return fsep;
#else
	fsep = float(sep / (255 * genome::gNumBytes));
    return fsep;
#endif
}


//---------------------------------------------------------------------------
// genome::MateProbability
//---------------------------------------------------------------------------
float genome::MateProbability(genome* g)
{
    // returns probability that two critters will successfully mate
    // based on their degree of genetic similarity/difference
    if (gMiscBias == 0.0)
        return 1.0;
        
    float a = CalcSeparation(g);
    float cosa = cos(pow(a, gMiscBias) * PI);
    float s = cosa > 0.0 ? 0.5 : -0.5;
    float p = 0.5  +  s * pow(fabs(cosa), gMiscInvisSlope);
    
    return p;
}


//---------------------------------------------------------------------------
// genome::MutationRate
//---------------------------------------------------------------------------
// 8 bits between minimum and maximum mutation rate
float genome::MutationRate() 
{
	return interp(GeneValue(mrategene), gMinMutationRate, gMaxMutationRate);
}


//---------------------------------------------------------------------------
// genome::CrossoverPoints
//---------------------------------------------------------------------------
// 8 bits between minnumcpts and maxnumcpts
long genome::CrossoverPoints() 
{
	return (long)interp(GeneValue(ncptsgene), gMinNumCpts, gMaxNumCpts);
}


//---------------------------------------------------------------------------
// genome::Lifespan
//---------------------------------------------------------------------------
// 8 bits between minlifespan and maxlifespan
long genome::Lifespan() 
{
	return (long)interp(GeneValue(lifespangene), gMinLifeSpan, gMaxLifeSpan);
}


//---------------------------------------------------------------------------
// genome::ID
//---------------------------------------------------------------------------
// 8 bits, mapped to green color
float genome::ID() 
{
	return GeneValue(idgene);
}


//---------------------------------------------------------------------------
// genome::Strength
//---------------------------------------------------------------------------
// 8 bits between minstrength and maxstrength
// relative to size, so 1.0 is normal for all sizes
// affects power (overall, non-size-relative strength)
// and scales all energy consumption
float genome::Strength() 
{
	return interp(GeneValue(strengthgene), gMinStrength, gMaxStrength);
}


//---------------------------------------------------------------------------
// genome::Size
//---------------------------------------------------------------------------
// 8 bits between mincsize and maxcsize
// also affects power (overall, non-size-relative strength)
// so power is a combination of strength & size
float genome::Size(float minSize, float maxSize)
{
	return interp(GeneValue(sizegene), minSize, maxSize);
}


//---------------------------------------------------------------------------
// genome::MaxSpeed
//---------------------------------------------------------------------------
// 8 bits between minmaxspeed and maxmaxspeed
float genome::MaxSpeed() 
{
	return interp(GeneValue(maxspeedgene), gMinmaxspeed, gMaxmaxspeed);
}


//---------------------------------------------------------------------------
// genome::MateEnergy
//---------------------------------------------------------------------------
// 8 bits between minlrate and maxlrate
float genome::MateEnergy() 
{
	return interp(GeneValue(mateenergygene), gMinmateenergy, gMaxmateenergy);
}


//---------------------------------------------------------------------------
// genome::NumNeuronGroups
//---------------------------------------------------------------------------
short genome::NumNeuronGroups()
{
	return nint(interp(GeneValue(numneurgroupsgene),
				brain::gNeuralValues.mininternalneurgroups, brain::gNeuralValues.maxinternalneurgroups))
				+ brain::gNeuralValues.numinputneurgroups + brain::gNeuralValues.numoutneurgroups;
}


//---------------------------------------------------------------------------
// genome::numeneur
//---------------------------------------------------------------------------
short genome::numeneur(short i)
{
    if ((i >= brain::gNeuralValues.numinputneurgroups) && (i < NumNeuronGroups()))
    {
        if (i >= (NumNeuronGroups() - brain::gNeuralValues.numoutneurgroups))
            return 1; // each output (behavior) group has just one neuron
        else
            return nint(interp(GeneValue(numeneurgene + i - brain::gNeuralValues.numinputneurgroups),
                               brain::gNeuralValues.mineneurpergroup, brain::gNeuralValues.maxeneurpergroup));
    }
    else if (i == 0)
    {
        return 1; // single random neuron
	}
    else if (i == 1)
	{
        return 1; // single energy neuron
	}
    else if (i == 2)
    {
        return nint(interp(GeneValue(numrneurgene), gMinvispixels, gMaxvispixels));
	}
    else if (i == 3)
	{
        return nint(interp(GeneValue(numgneurgene),gMinvispixels, gMaxvispixels));
	}
    else if (i == 4)
	{
        return nint(interp(GeneValue(numbneurgene),gMinvispixels, gMaxvispixels));
	}
    else
	{
        error(2, "numeneur called for invalid group number (", i,")");
	}
	
	return -1;
}


//---------------------------------------------------------------------------
// genome::numineur
//---------------------------------------------------------------------------
short genome::numineur(short i)
{
    if ((i >= brain::gNeuralValues.numinputneurgroups) && (i < NumNeuronGroups()))
    {
        if (i >= (NumNeuronGroups() - brain::gNeuralValues.numoutneurgroups))
            return 1; // each output (behavior) group has just one neuron
        else
            return nint(interp(GeneValue(numineurgene + i - brain::gNeuralValues.numinputneurgroups),
                               brain::gNeuralValues.minineurpergroup, brain::gNeuralValues.maxineurpergroup));
    }
    else if (i == 0)
        return 1; // single random neuron
    else if (i == 1)
        return 1; // single energy neuron
    else if (i == 2)
        return nint(interp(GeneValue(numrneurgene), gMinvispixels, gMaxvispixels));
    else if (i == 3)
        return nint(interp(GeneValue(numgneurgene), gMinvispixels, gMaxvispixels));
    else if (i == 4)
        return nint(interp(GeneValue(numbneurgene), gMinvispixels, gMaxvispixels));
    else
        error(2,"numineur called for invalid group number (",i,")");
        
	return -1;        
}


//---------------------------------------------------------------------------
// genome::numeneurons
//---------------------------------------------------------------------------
short genome::numneurons(short i)
{
    if ((i >= brain::gNeuralValues.numinputneurgroups) && (i < NumNeuronGroups()))
    {
        if (i >= (NumNeuronGroups() - brain::gNeuralValues.numoutneurgroups))
            return 1; // each output (behavior) group has just one neuron
        else
            return (numeneur(i)+numineur(i));
    }
    else if (i >= 0)
        return numeneur(i); // only excitatory
    else
        error(2,"numneurons called for invalid group number (",i,")");
        
	return -1;        
}


//---------------------------------------------------------------------------
// genome::Bias
//---------------------------------------------------------------------------
float genome::Bias(short i)
{
	return interp(GeneValue(biasgene+i), -brain::gNeuralValues.maxbias, brain::gNeuralValues.maxbias);
}


//---------------------------------------------------------------------------
// genome::BiasLearningRate
//---------------------------------------------------------------------------
float genome::BiasLearningRate(short i)
{
	return interp(GeneValue(biaslrategene+i), brain::gNeuralValues.minbiaslrate, brain::gNeuralValues.maxbiaslrate);
}


//---------------------------------------------------------------------------
// genome::numesynapses
//---------------------------------------------------------------------------
long genome::numeesynapses(short i, short j)
{
	return nint(eecd(i,j)*numeneur(i)*(numeneur(j)-((i==j)?1:0)));
}


//---------------------------------------------------------------------------
// genome::numeisynapses
//---------------------------------------------------------------------------
long genome::numeisynapses(short i, short j)
{
	if (i >= (NumNeuronGroups() - brain::gNeuralValues.numoutneurgroups))
		return 0;
	else
		return nint(eicd(i,j)*numineur(i)*numeneur(j));
}


//---------------------------------------------------------------------------
// genome::numiisynapses
//---------------------------------------------------------------------------
long genome::numiisynapses(short i, short j)
{
	if (i >= (NumNeuronGroups() - brain::gNeuralValues.numoutneurgroups))
		return 0;
	else
		return nint(iicd(i,j)*numineur(i)*(numineur(j)-((i==j)?1:0)));
}


//---------------------------------------------------------------------------
// genome::numiesynapses
//---------------------------------------------------------------------------
long genome::numiesynapses(short i, short j)
{
	return nint(iecd(i,j)*numeneur(i) * (numineur(j) - (((i == j)
				&& (i >= (NumNeuronGroups() - brain::gNeuralValues.numoutneurgroups))) ? 1 : 0)));
}


//---------------------------------------------------------------------------
// genome::numsynapses
//---------------------------------------------------------------------------
long genome::numsynapses(short i, short j)
{
	return numeesynapses(i,j)
		   + numeisynapses(i,j)
		   + numiisynapses(i,j)
		   + numiesynapses(i,j);
}


//---------------------------------------------------------------------------
// genome::eecd
//---------------------------------------------------------------------------
float genome::eecd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2, "eecd called with input group as to-group (",i,")");
	
	return interp(GeneValue(eecdgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.minconnectiondensity, brain::gNeuralValues.maxconnectiondensity);
}


//---------------------------------------------------------------------------
// genome::eicd
//---------------------------------------------------------------------------
float genome::eicd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"eicd called with input group as to-group (",i,")");
		
	return interp(GeneValue(eicdgene+(i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.minconnectiondensity, brain::gNeuralValues.maxconnectiondensity);
}


//---------------------------------------------------------------------------
// genome::iicd
//---------------------------------------------------------------------------
float genome::iicd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"iicd called with input group as to-group (",i,")");
	return interp(GeneValue(iicdgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.minconnectiondensity, brain::gNeuralValues.maxconnectiondensity);
}


//---------------------------------------------------------------------------
// genome::iecd
//---------------------------------------------------------------------------
float genome::iecd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"iecd called with input group as to-group (",i,")");
		
	return interp(GeneValue(iecdgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.minconnectiondensity, brain::gNeuralValues.maxconnectiondensity);
}


//---------------------------------------------------------------------------
// genome::eetd
//---------------------------------------------------------------------------
float genome::eetd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"eetd called with input group as to-group (",i,")");
	
	return interp(GeneValue(eetdgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.mintopologicaldistortion, brain::gNeuralValues.maxtopologicaldistortion);
}


//---------------------------------------------------------------------------
// genome::eitd
//---------------------------------------------------------------------------
float genome::eitd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"eitd called with input group as to-group (",i,")");
		
	return interp(GeneValue(eitdgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.mintopologicaldistortion, brain::gNeuralValues.maxtopologicaldistortion);
}


//---------------------------------------------------------------------------
// genome::iitd
//---------------------------------------------------------------------------
float genome::iitd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"iitd called with input group as to-group (",i,")");
	
	return interp(GeneValue(iitdgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.mintopologicaldistortion, brain::gNeuralValues.maxtopologicaldistortion);
}


//---------------------------------------------------------------------------
// genome::ietd
//---------------------------------------------------------------------------
float genome::ietd(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"ietd called with input group as to-group (",i,")");
		
	return interp(GeneValue(ietdgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  brain::gNeuralValues.mintopologicaldistortion, brain::gNeuralValues.maxtopologicaldistortion);
}


//---------------------------------------------------------------------------
// genome::eelr
//---------------------------------------------------------------------------
float genome::eelr(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"eelr called with input group as to-group (",i,")");
		
	return interp(GeneValue(eelrgene + (i - brain::gNeuralValues.numinputneurgroups) * brain::gNeuralValues.maxneurgroups + j),
				  gMinlrate, gMaxlrate);
}


//---------------------------------------------------------------------------
// genome::eilr
//---------------------------------------------------------------------------
float genome::eilr(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"eilr called with input group as to-group (",i,")");
		
	return interp(GeneValue(eilrgene+(i-brain::gNeuralValues.numinputneurgroups)*brain::gNeuralValues.maxneurgroups+j),
				  gMinlrate, gMaxlrate);
}


//---------------------------------------------------------------------------
// genome::iilr
//---------------------------------------------------------------------------
float genome::iilr(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"iilr called with input group as to-group (",i,")");
		
	// note: learning rate at inhibitory synapses must be < 0.0 (not <= 0.0)
	return min(-1.e-10, (double)(-interp(GeneValue(iilrgene + (i - brain::gNeuralValues.numinputneurgroups)
									     * brain::gNeuralValues.maxneurgroups + j), gMinlrate, gMaxlrate)));
}


//---------------------------------------------------------------------------
// genome::ielr
//---------------------------------------------------------------------------
float genome::ielr(short i, short j)
{
	if (i < brain::gNeuralValues.numinputneurgroups)
		error(2,"ielr called with input group as to-group (",i,")");
		
	// note: learning rate at inhibitory synapses must be < 0.0 (not <= 0.0)
	return min(-1.e-10, (double)(-interp(GeneValue(ielrgene+(i-brain::gNeuralValues.numinputneurgroups)*brain::gNeuralValues.maxneurgroups+j),
										 gMinlrate, gMaxlrate)));
}


//---------------------------------------------------------------------------
// genome::Print
//---------------------------------------------------------------------------
void genome::Print()
{
	long lobit = 0;
	long hibit = genome::gNumBytes * 8;
	Print( lobit, hibit );
}


//---------------------------------------------------------------------------
// genome::Print
//---------------------------------------------------------------------------
void genome::Print(long lobit, long hibit)
{
    cout << "genome bits " << lobit << " through " << hibit << " =" nl;
    for (long i = lobit; i <= hibit; i++)
    {
        long byte = i >> 3; // 0-based
        long bit = i % 8; // 0-based,from left
        cout << ((fGenes[byte] >> (7-bit)) & 1);
    }
    cout nlf;
}


//---------------------------------------------------------------------------
// genome::SetOne
//---------------------------------------------------------------------------
void genome::SetOne(long lobit, long hibit)
{
    for (long i = lobit; i <= hibit; i++)
    {
        long byte = i >> 3; // 0-based
        long bit = i % 8; // 0-based,from left
        fGenes[byte] |= (unsigned char)(1 << (7-bit));
    }
}


//---------------------------------------------------------------------------
// genome::SetZero
//---------------------------------------------------------------------------
void genome::SetZero(long lobit, long hibit)
{
    for (long i = lobit; i <= hibit; i++)
    {
        long byte = i >> 3; // 0-based
        long bit = i % 8; // 0-based,from left
        fGenes[byte] &= (unsigned char)(~(1 << (7-bit)));
    }
}


//---------------------------------------------------------------------------
// genome::SetOne
//---------------------------------------------------------------------------
void genome::SetOne()
{
    for (long i = 0; i < genome::gNumBytes; i++)
        fGenes[i] = 0xff;
}


//---------------------------------------------------------------------------
// genome::SetZero
//---------------------------------------------------------------------------
void genome::SetZero()
{
    for (long i = 0; i < genome::gNumBytes; i++)
        fGenes[i] = 0;
}
