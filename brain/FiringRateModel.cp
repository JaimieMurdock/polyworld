#include "FiringRateModel.h"

#include "debug.h"
#include "Genome.h"
#include "GenomeSchema.h"
#include "misc.h"

#include "Brain.h" // temporary

using namespace std;

using namespace genome;


#define GaussianOutputNeurons 0
#if GaussianOutputNeurons
	#define GaussianActivationMean 0.0
	#define GaussianActivationStandardDeviation 1.5
	#define GaussianActivationVariance (GaussianActivationStandardDeviation * GaussianActivationStandardDeviation)
#endif


FiringRateModel::FiringRateModel( NervousSystem *cns )
: BaseNeuronModel<Neuron, Synapse>( cns )
{
}

FiringRateModel::~FiringRateModel()
{
}

void FiringRateModel::init_derived( float initial_activation )
{
	if( brain::gNeuralValues.model == brain::NeuralValues::TAU )
	{
		tauGene = genes->gene( "Tau" );
	}
	else
	{
		tauGene = NULL;
	}

	for( int i = 0; i < dims->numneurons; i++ )
	{
		neuronactivation[i] = initial_activation;
	}	
}

void FiringRateModel::set_neuron( int index,
								  int group,
								  float bias,
								  int startsynapses,
								  int endsynapses )
{
	BaseNeuronModel<Neuron, Synapse>::set_neuron( index,
												  group,
												  bias,
												  startsynapses,
												  endsynapses );
	Neuron &n = neuron[index];

	if( tauGene )
	{
		if( genes->schema->getNeurGroupType(group) != NGT_INPUT )
		{
			n.tau = genes->get( tauGene, group );
		}
	}
}

void FiringRateModel::dump( ostream &out )
{
	long i;

    for (i = 0; i < dims->numneurons; i++)
	{
		Neuron &n = neuron[i];
        out << n.group
			sp n.bias
			sp n.tau
			sp n.startsynapses
			sp n.endsynapses
			nl;
	}

    for (i = 0; i < dims->numneurons; i++)
        out << neuronactivation[i] nl;

    for (i = 0; i < dims->numsynapses; i++)
	{
		Synapse &s = synapse[i];
        out << s.efficacy
            sp s.fromneuron
			sp s.toneuron
			nl;
	}

    for (i = 0; i < dims->numgroups; i++)
        out << groupblrate[i] nl;

    for (i = 0; i < (dims->numgroups * dims->numgroups * 4); i++)
        out << grouplrate[i] nl;
}

void FiringRateModel::load( istream &in )
{
	long i;

    for (i = 0; i < dims->numneurons; i++)
	{
		Neuron &n = neuron[i];
        in >> n.group
		   >> n.bias
		   >> n.tau
		   >> n.startsynapses
		   >> n.endsynapses;

#if DesignerBrains
		groupsize[i]++;
#endif
	}

    for (i = 0; i < dims->numneurons; i++)
        in >> neuronactivation[i];

    for (i = 0; i < dims->numsynapses; i++)
	{
		Synapse &s = synapse[i];
        in >> s.efficacy
		   >> s.fromneuron
		   >> s.toneuron;
	}

    for (i = 0; i < dims->numgroups; i++)
        in >> groupblrate[i];

    for (i = 0; i < (dims->numgroups * dims->numgroups * 4); i++)
        in >> grouplrate[i];	
}

void FiringRateModel::update( bool bprint )
{
    debugcheck( "(firing-rate brain) on entry" );

    short i,j,ii,jj;
    long k;
    if ((neuron == NULL) || (synapse == NULL) || (neuronactivation == NULL))
        return;

	IF_BPRINTED
	(
        printf("neuron (toneuron)  fromneuron   synapse   efficacy\n");
        
        for( i = dims->firstNonInputNeuron; i < dims->numneurons; i++ )
        {
            for( k = neuron[i].startsynapses; k < neuron[i].endsynapses; k++ )
            {
				printf("%3d   %3d    %3d    %5ld    %f\n",
					   i, synapse[k].toneuron, synapse[k].fromneuron,
					   k, synapse[k].efficacy); 
            }
        }
	)


    debugcheck( "after updating vision" );

//	float minExcitation = FLT_MAX;
//	float maxExcitation = -FLT_MAX;
//	float avgExcitation = 0.0;
//	float minActivation = FLT_MAX;
//	float maxActivation = -FLT_MAX;
//	float avgActivation = 0.0;

	for( i = dims->firstOutputNeuron; i < dims->firstInternalNeuron; i++ )
	{
        newneuronactivation[i] = neuron[i].bias;
        for( k = neuron[i].startsynapses; k < neuron[i].endsynapses; k++ )
        {
            newneuronactivation[i] += synapse[k].efficacy *
               neuronactivation[abs(synapse[k].fromneuron)];
		}              
//		if( newneuronactivation[i] < minExcitation )
//			minExcitation = newneuronactivation[i];
//		if( newneuronactivation[i] > maxExcitation )
//			maxExcitation = newneuronactivation[i];
//		avgExcitation += newneuronactivation[i];
	#if GaussianOutputNeurons
        newneuronactivation[i] = gaussian( newneuronactivation[i], GaussianActivationMean, GaussianActivationVariance );
	#else
		if( tauGene )
		{
			float tau = neuron[i].tau;
			newneuronactivation[i] = (1.0 - tau) * neuronactivation[i]  +  tau * logistic( newneuronactivation[i], brain::gLogisticsSlope );
		}
		else
		{
			newneuronactivation[i] = logistic( newneuronactivation[i], brain::gLogisticsSlope );
		}
	#endif
//		if( newneuronactivation[i] < minActivation )
//			minActivation = newneuronactivation[i];
//		if( newneuronactivation[i] > maxActivation )
//			maxActivation = newneuronactivation[i];
//		avgActivation += newneuronactivation[i];
	}
//	avgExcitation /= dims->numOutputNeurons;
//	avgActivation /= dims->numOutputNeurons;

//	printf( "X:  min=%6.1f  max=%6.1f  avg=%6.1f\n", minExcitation, maxExcitation, avgExcitation );
//	printf( "X:  min=%6.1f  max=%6.1f  avg=%6.1f\n", minActivation, maxActivation, avgActivation );

	long numneurons = dims->numneurons;
	float logisticsSlope = brain::gLogisticsSlope;
    for( i = dims->firstInternalNeuron; i < numneurons; i++ )
    {
		float newactivation = neuron[i].bias;
        for( k = neuron[i].startsynapses; k < neuron[i].endsynapses; k++ )
        {
            newactivation += synapse[k].efficacy *
               neuronactivation[abs(synapse[k].fromneuron)];
		}
        //newneuronactivation[i] = logistic(newneuronactivation[i], brain::gLogisticsSlope);

		if( tauGene )
		{
			float tau = neuron[i].tau;
			newactivation = (1.0 - tau) * neuronactivation[i]  +  tau * logistic( newactivation, logisticsSlope );
		}
		else
		{
			newactivation = logistic( newactivation, logisticsSlope );
		}

        newneuronactivation[i] = newactivation;
    }

#define DebugBrain 0
#if DebugBrain
	static int numDebugBrains = 0;
	if( (TSimulation::fAge > 1) && (numDebugBrains < 10) )
	{
		numDebugBrains++;
//		if( numDebugBrains > 2 )
//			exit( 0 );
		printf( "***** age = %ld *****\n", TSimulation::fAge );
		printf( "random neuron [%d] = %g\n", randomneuron, neuronactivation[randomneuron] );
		printf( "energy neuron [%d] = %g\n", energyneuron, neuronactivation[energyneuron] );
		printf( "red neurons (%d):\n", fNumRedNeurons );
		for( i = 0; i < fNumRedNeurons; i++ )
			printf( "    %3d  %2d  %g\n", i+redneuron, i, neuronactivation[i+redneuron] );
		printf( "green neurons (%d):\n", fNumGreenNeurons );
		for( i = 0; i < fNumGreenNeurons; i++ )
			printf( "    %3d  %2d  %g\n", i+greenneuron, i, neuronactivation[i+greenneuron] );
		printf( "blue neurons (%d):\n", fNumBlueNeurons );
		for( i = 0; i < fNumBlueNeurons; i++ )
			printf( "    %3d  %2d  %g\n", i+blueneuron, i, neuronactivation[i+blueneuron] );
		printf( "output neurons (%d):\n", dims->numOutputNeurons );
		for( i = dims->firstOutputNeuron; i < dims->firstInternalNeuron; i++ )
		{
			printf( "    %3d  %g  %g synapses (%ld):\n", i, neuron[i].bias, newneuronactivation[i], neuron[i].endsynapses - neuron[i].startsynapses );
			for( k = neuron[i].startsynapses; k < neuron[i].endsynapses; k++ )
				printf( "        %4ld  %2ld  %2d  %g  %g\n", k, k-neuron[i].startsynapses, synapse[k].fromneuron, synapse[k].efficacy, neuronactivation[abs(synapse[k].fromneuron)] );
		}
		printf( "internal neurons (%d):\n", numnoninputneurons-dims->numOutputNeurons );
		for( i = dims->firstInternalNeuron; i < dims->numneurons; i++ )
		{
			printf( "    %3d  %g  %g synapses (%ld):\n", i, neuron[i].bias, newneuronactivation[i], neuron[i].endsynapses - neuron[i].startsynapses );
			for( k = neuron[i].startsynapses; k < neuron[i].endsynapses; k++ )
				printf( "        %4ld  %2ld  %2d  %g  %g\n", k, k-neuron[i].startsynapses, synapse[k].fromneuron, synapse[k].efficacy, neuronactivation[abs(synapse[k].fromneuron)] );
		}
	}
#endif

    debugcheck( "after updating neurons" );

	IF_BPRINT
	(
        printf("  i neuron[i].bias neuronactivation[i] newneuronactivation[i]\n");
        for (i = 0; i < dims->numneurons; i++)
            printf( "%3d  %1.4f  %1.4f  %1.4f\n", i, neuron[i].bias, neuronactivation[i], newneuronactivation[i] );
	)

//	printf( "yaw activation = %g\n", newneuronactivation[yawneuron] );

    float learningrate;
	long numsynapses = dims->numsynapses;
	long numgroups = dims->numgroups;
    for (k = 0; k < numsynapses; k++)
    {
		FiringRateModel__Synapse &syn = synapse[k];

        if (syn.toneuron >= 0) // 0 can't happen it's an input neuron
        {
            i = syn.toneuron;
            ii = 0;
        }
        else
        {
            i = -syn.toneuron;
            ii = 1;
        }
        if ( (syn.fromneuron > 0) ||
            ((syn.toneuron  == 0) && (syn.efficacy >= 0.0)) )
        {
            j = syn.fromneuron;
            jj = 0;
        }
        else
        {
            j = -syn.fromneuron;
            jj = 1;
        }
        // Note: If .toneuron == 0, and .efficacy were to equal
        // 0.0 for an inhibitory synapse, we would choose the
        // wrong learningrate, but we prevent efficacy from going
        // to zero below & during initialization to prevent this.
        // Similarly, learningrate is guaranteed to be < 0.0 for
        // inhibitory synapses.
        learningrate = grouplrate[index4(neuron[i].group,neuron[j].group,ii,jj, numgroups,2,2)];

		float efficacy = syn.efficacy + learningrate
			             * (newneuronactivation[i]-0.5f)
			             * (   neuronactivation[j]-0.5f);

        if (fabs(efficacy) > (0.5f * brain::gMaxWeight))
        {
            efficacy *= 1.0f - (1.0f - brain::gDecayRate) *
                (fabs(efficacy) - 0.5f * brain::gMaxWeight) / (0.5f * brain::gMaxWeight);
            if (efficacy > brain::gMaxWeight)
                efficacy = brain::gMaxWeight;
            else if (efficacy < -brain::gMaxWeight)
                efficacy = -brain::gMaxWeight;
        }
        else
        {
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
            // not strictly correct for this to be in an else clause,
            // but if lrate is reasonable, efficacy should never change
            // sign with a new magnitude greater than 0.5 * brain::gMaxWeight
            if (learningrate >= 0.0f)  // excitatory
                efficacy = MAX(0.0f, efficacy);
            if (learningrate < 0.0f)  // inhibitory
                efficacy = MIN(-1.e-10f, efficacy);
        }

		syn.efficacy = efficacy;
    }

    debugcheck( "after updating synapses" );

#if 0
	if( gNeuralValues.maxbiaslrate > 0.0 )
	{
		for (i = firstnoninputneuron; i < dims->numneurons; i++)
		{
			neuron[i].bias += groupblrate[neuron[i].group]
							* (newneuronactivation[i]-0.5f)
							* 0.5f;
			if (fabs(neuron[i].bias) > (0.5 * gNeuralValues.maxbias))
			{
				neuron[i].bias *= 1.0 - (1.0f - brain::gDecayRate) *
					(fabs(neuron[i].bias) - 0.5f * gNeuralValues.maxbias) / (0.5f * gNeuralValues.maxbias);
				if (neuron[i].bias > gNeuralValues.maxbias)
					neuron[i].bias = gNeuralValues.maxbias;
				else if (neuron[i].bias < -gNeuralValues.maxbias)
					neuron[i].bias = -gNeuralValues.maxbias;
			}
		}
	}

    debugcheck( "after updating biases" );
#endif

    float* saveneuronactivation = neuronactivation;
    neuronactivation = newneuronactivation;
    newneuronactivation = saveneuronactivation;
}
