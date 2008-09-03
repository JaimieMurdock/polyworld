// The first batch of Debug flags below are the common ones to enable for figuring out problems in the Interact()
#define TEXTTRACE 0
#define DebugShowSort 0
#define DebugShowBirths 0
#define DebugShowEating 0
#define DebugGeneticAlgorithm 0
#define DebugSmite 0

#define DebugMaxFitness 0
#define DebugLinksEtAl 0
#define DebugDomainFoodBands 0
#define DebugLockStep 0

#define MinDebugStep 0
#define MaxDebugStep INT_MAX

#define CurrentWorldfileVersion 29

// CompatibilityMode makes the new code with a single x-sorted list behave *almost* identically to the old code.
// Discrepancies still arise due to the old food list never being re-sorted and critters at the exact same x location
// not always ending up sorted in the same order.  [Food centers remain sorted as long as they exist, but these lists
// are actually sorted on x at the left edge (x increases from left to right) of the objects, which changes as the
// food is eaten and shrinks.]
#define CompatibilityMode 1

#define UseMaxSpeedAsFitness 0

// Self
#include "Simulation.h"

// System
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <assert.h>

// qt
#include <qapplication.h>
#include <qdesktopwidget.h>

// Local
#include "barrier.h"
#include "brain.h"
#include "BrainMonitorWindow.h"
#include "ChartWindow.h"
#include "CritterPOVWindow.h"
#include "debug.h"
#include "food.h"
#include "globals.h"
#include "food.h"
#include "SceneView.h"
#include "TextStatusWindow.h"
#include "PwMovieTools.h"
#include "complexity.h"

#include "objectxsortedlist.h"
#include "Patch.h"
#include "FoodPatch.h"
#include "BrickPatch.h"
#include "brick.h"

using namespace std;

//===========================================================================
// TSimulation
//===========================================================================

static long numglobalcreated = 0;    // needs to be static so we only get warned about influence of global creations once ever

long TSimulation::fMaxCritters;
long TSimulation::fStep;
short TSimulation::fOverHeadRank = 1;
critter* TSimulation::fMonitorCritter = NULL;
double TSimulation::fFramesPerSecondOverall;
double TSimulation::fSecondsPerFrameOverall;
double TSimulation::fFramesPerSecondRecent;
double TSimulation::fSecondsPerFrameRecent;
double TSimulation::fFramesPerSecondInstantaneous;
double TSimulation::fSecondsPerFrameInstantaneous;
double TSimulation::fTimeStart;

//---------------------------------------------------------------------------
// Prototypes
//---------------------------------------------------------------------------

float AverageAngles( float a, float b );

//---------------------------------------------------------------------------
// Inline functions
//---------------------------------------------------------------------------

// Average two angles ranging from 0.0 to 360.0, dealing with the fact that you can't
// just sum and divide by two when they are separated numerically by more than 180.0,
// and making sure the result stays less than 360 degrees.
inline float AverageAngles( float a, float b )
{
	float c;
	
	if( fabs( a - b ) > 180.0 )
	{
		c = 0.5 * (a + b)  +  180.0;
		if( c >= 360.0 )
			c -= 360.0;
	}
	else
		c = 0.5 * (a + b);
	
	return( c );
}


//---------------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------------

#if TEXTTRACE
	#define ttPrint( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) printf( x ); }
#else
	#define ttPrint( x... )
#endif

#if DebugSmite
	#define smPrint( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) printf( x ); }
#else
	#define smPrint( x... )
#endif

#if DebugLinksEtAl
	#define link( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) { ( printf( "%lu link %s to %s (%s/%d)\n", fStep, s, t, __FUNCTION__, __LINE__ ), link( x ) ) } }
	#define unlink( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) { ( printf( "%lu unlink %s (%s/%d)\n", fStep, s, __FUNCTION__, __LINE__ ), unlink( x ) ) } }
	#define rename( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) { ( printf( "%lu rename %s to %s (%s/%d)\n", fStep, s, t, __FUNCTION__, __LINE__ ), rename( x ) ) } }
#endif

#if DebugShowBirths
	#define birthPrint( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) printf( x ); }
#else
	#define birthPrint( x... )
#endif

#if DebugShowEating
	#define eatPrint( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) printf( x ); }
#else
	#define eatPrint( x... )
#endif

#if DebugGeneticAlgorithm
	#define gaPrint( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) printf( x ); }
#else
	#define gaPrint( x... )
#endif

#if DebugLockStep
	#define lsPrint( x... ) { if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) ) printf( x ); }
#else
	#define lsPrint( x... )
#endif

//---------------------------------------------------------------------------
// TSimulation::TSimulation
//---------------------------------------------------------------------------
TSimulation::TSimulation( TSceneView* sceneView, TSceneWindow* sceneWindow )
	:
		fLockStepWithBirthsDeathsLog(false),
		fLockstepFile(NULL),
		fCritterTracking(false),
//		fMonitorCritterRank(0),
		fMonitorCritterRankOld(0),
		fRotateWorld(false),
//		fOverHeadRank(0),
//		fMonitorCritter(NULL),
		fBestSoFarBrainAnatomyRecordFrequency(0),
		fBestSoFarBrainFunctionRecordFrequency(0),
		fBestRecentBrainAnatomyRecordFrequency(0),
		fBestRecentBrainFunctionRecordFrequency(0),
		fRecordBirthsDeaths(false),
		fBrainAnatomyRecordAll(false),
		fBrainFunctionRecordAll(false),
		fBrainAnatomyRecordSeeds(false),
		fBrainFunctionRecordSeeds(false),
		fApplyLowPopulationAdvantage(false),

		fRecordComplexity(false),

		fRecordAdamiComplexity(false),
		fAdamiComplexityRecordFrequency(0),


		fSceneView(sceneView),
		fSceneWindow(sceneWindow),
		fBirthrateWindow(NULL),
		fFitnessWindow(NULL),
		fFoodEnergyWindow(NULL),
		fPopulationWindow(NULL),
		fBrainMonitorWindow(NULL),
		fGeneSeparationWindow(NULL),
		fMaxSteps(0),
		fPaused(false),
		fDelay(0),
		fDumpFrequency(500),
		fStatusFrequency(100),
		fLoadState(false),
		inited(false),

		fHealing(0),
		fGroundClearance(0.0),
		fOverHeadRankOld(0),
		fOverheadCritter(NULL),

		fChartBorn(true),
		fChartFitness(true),
		fChartFoodEnergy(true),
		fChartPopulation(true),
//		fShowBrain(false),
		fShowTextStatus(true),
		fRecordGeneStats(false),
		fRecordFoodPatchStats(false),

		fNewDeaths(0),
		fNumberFit(0),
		fFittest(NULL),

		fNumberRecentFit(0),
		fRecentFittest(NULL),
		fFogFunction('O'),
		fBrainMonitorStride(25),
		fGeneSum(NULL),
		fGeneSum2(NULL),
		fGeneStatsFile(NULL),
		fFoodPatchStatsFile(NULL),
		fNumCrittersNotInOrNearAnyFoodPatch(0)
{
	Init();
}


//---------------------------------------------------------------------------
// TSimulation::~TSimulation
//---------------------------------------------------------------------------
TSimulation::~TSimulation()
{
	Stop();


	// If we were keeping the simulation in sync with a LOCKSTEP-BirthsDeaths.log, close the file now that the simulation is over.
	if( fLockstepFile )
		fclose( fLockstepFile );
	
	if( fGeneSum )
		free( fGeneSum );

	if( fGeneSum2 )
		free( fGeneSum2 );
		
	if( fGeneStatsFile )
		fclose( fGeneStatsFile );

	// Close windows and save prefs
	if (fBirthrateWindow != NULL)
		delete fBirthrateWindow;

	if (fFitnessWindow != NULL)
		delete fFitnessWindow;

	if (fFoodEnergyWindow != NULL)
		delete fFoodEnergyWindow;

	if (fPopulationWindow != NULL)
		delete fPopulationWindow;
	
	if (fBrainMonitorWindow != NULL)
		delete fBrainMonitorWindow;	

	if (fGeneSeparationWindow != NULL)
		delete fGeneSeparationWindow;

	if (fCritterPOVWindow != NULL)
		delete fCritterPOVWindow;

	if (fTextStatusWindow != NULL)
		delete fTextStatusWindow;
	
	if (fOverheadWindow != NULL)
		delete fOverheadWindow;
}


//---------------------------------------------------------------------------
// TSimulation::Start
//---------------------------------------------------------------------------
void TSimulation::Start()
{

}


//---------------------------------------------------------------------------
// TSimulation::Stop
//---------------------------------------------------------------------------
void TSimulation::Stop()
{
	// delete all objects
	gobject* gob = NULL;

	// delete all non-critters
	objectxsortedlist::gXSortedObjects.reset();
	while (objectxsortedlist::gXSortedObjects.next(gob))
		if (gob->getType() != CRITTERTYPE)
		{
			//delete gob;	// ??? why aren't these being deleted?  do we need to delete them by type?  make the destructor virtual?  what???
		}
	// delete all critters
	// all the critters are deleted in critterdestruct
	// rather than cycling through them in xsortedcritters here
	objectxsortedlist::gXSortedObjects.clear();

	
	// TODO who owns items on stage?
	fStage.Clear();

    for (short id = 0; id < fNumDomains; id++)
    {
        if (fDomains[id].fittest)
        {
            for (int i = 0; i < fNumberFit; i++)
            {
				if (fDomains[id].fittest[i])
				{
					if (fDomains[id].fittest[i]->genes != NULL)
						delete fDomains[id].fittest[i]->genes;
					delete fDomains[id].fittest[i];
				}
			}
            delete fDomains[id].fittest;
        }
		
		if( fDomains[id].fLeastFit )
			delete[] fDomains[id].fLeastFit;
    }


    if (fFittest != NULL)
    {
        for (int i = 0; i < fNumberFit; i++)
        {
			if (fFittest[i])
			{
				if (fFittest[i]->genes != NULL)
					delete fFittest[i]->genes;
				delete fFittest[i];
			}
		}		
        delete[] fFittest;
    }
	
    if( fRecentFittest != NULL )
    {
        for( int i = 0; i < fNumberRecentFit; i++ )
        {
			if( fRecentFittest[i] )
			{
//				if( fRecentFittest[i]->genes != NULL )	// we don't save the genes in the bestRecent list
//					delete fFittest[i]->genes;
				delete fRecentFittest[i];
			}
		}		
        delete[] fRecentFittest;
    }
	
	if( fLeastFit )
		delete fLeastFit;

    genome::genomedestruct();

    brain::braindestruct();

    critter::critterdestruct();
}


//---------------------------------------------------------------------------
// TSimulation::Step
//---------------------------------------------------------------------------
void TSimulation::Step()
{
	if( !inited )
	{
		printf( "%s: called before TSimulation::Init()\n", __FUNCTION__ );
		return;
	}
	
	static unsigned long frame = 0;
	frame++;
//	printf( "%s: frame = %lu\n", __FUNCTION__, frame );
	
#define RecentSteps 10
	static double	sTimePrevious[RecentSteps];
	double			timeNow;
	
	if( fMaxSteps && ((fStep+1) > fMaxSteps) )
	{
		// Stop simulation
		printf( "Simulation stopped after step %ld\n", fStep );
		exit( 0 );
	}
		
	fStep++;
	
#ifdef DEBUGCHECK
	char debugstring[256];
	sprintf(debugstring, "in main loop at age %ld", fStep);
	debugcheck(debugstring);
#endif

	// compute some frame rates
	timeNow = hirestime();
	if( fStep == 1 )
	{
		fFramesPerSecondOverall = 0.;
		fSecondsPerFrameOverall = 0.;

		fFramesPerSecondRecent = 0.;
		fSecondsPerFrameRecent = 0.;

		fFramesPerSecondInstantaneous = 0.;
		fSecondsPerFrameInstantaneous = 0.;

		fTimeStart = timeNow;
	}
	else
	{
		fFramesPerSecondOverall = fStep / (timeNow - fTimeStart);
		fSecondsPerFrameOverall = 1. / fFramesPerSecondOverall;
		
		if( fStep > RecentSteps )
		{
			fFramesPerSecondRecent = RecentSteps / (timeNow - sTimePrevious[RecentSteps-1]);
			fSecondsPerFrameRecent = 1. / fFramesPerSecondRecent;
		}

		fFramesPerSecondInstantaneous = 1. / (timeNow - sTimePrevious[0]);
		fSecondsPerFrameInstantaneous = 1. / fFramesPerSecondInstantaneous;

		int numSteps = fStep < RecentSteps ? fStep : RecentSteps;
		for( int i = numSteps-1; i > 0; i-- )
			sTimePrevious[i] = sTimePrevious[i-1];
	}
	sTimePrevious[0] = timeNow;
	
	if (((fStep - fLastCreated) > fMaxGapCreate) && (fLastCreated > 0) )
		fMaxGapCreate = fStep - fLastCreated;

	if (fNumDomains > 1)
	{
		for (short id = 0; id < fNumDomains; id++)
		{
			if (((fStep - fDomains[id].lastcreate) > fDomains[id].maxgapcreate)
				  && (fDomains[id].lastcreate > 0))
			{
				fDomains[id].maxgapcreate = fStep - fDomains[id].lastcreate;
			}
		}
	}
	
	// Set up some per-step values
	fFoodEnergyIn = 0.0;
	fFoodEnergyOut = 0.0;

#ifdef DEBUGCHECK
	long critnum = 0;
#endif

	// Update the barriers, now that they can be dynamic
	barrier* b;
	barrier::gXSortedBarriers.reset();
	while( barrier::gXSortedBarriers.next( b ) )
		b->update( fStep );
	barrier::gXSortedBarriers.xsort();

	// Update all critters, using their neurally controlled behaviors
	{
		// Make the critter POV window the current GL context
		fCritterPOVWindow->makeCurrent();
		
		// Clear the window's color and depth buffers
		fCritterPOVWindow->qglClearColor( Qt::black );
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		// These are the critter counts to be used in applying either the LowPopulationAdvantage or the (high) PopulationPenalty
		// Assume global settings apply, until we know better
		long numCritters = objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE);
		long initNumCritters = fInitNumCritters;
		long minNumCritters = fMinNumCritters  +  lround( 0.1 * (fInitNumCritters - fMinNumCritters) );	// 10% buffer, to help prevent reaching actual min value and invoking GA
		long maxNumCritters = fMaxCritters;
		long excess = numCritters - fInitNumCritters;	// global excess

		// Use the lowest excess value to produce the most help or the least penalty
		if( fNumDomains > 1 )
		{
			for( int id = 0; id < fNumDomains; id++ )
			{
				long domainExcess = fDomains[id].numCritters - fDomains[id].initNumCritters;
				if( domainExcess < excess )	// This is the domain that is in the worst shape
				{
					numCritters = fDomains[id].numCritters;
					initNumCritters = fDomains[id].initNumCritters;
					minNumCritters = fDomains[id].minNumCritters  +  lround( 0.1 * (fDomains[id].initNumCritters - fDomains[id].minNumCritters) );	// 10% buffer, to help prevent reaching actual min value and invoking GA
					maxNumCritters = fDomains[id].maxNumCritters;
					excess = domainExcess;
				}
			}
		}
		
		// If the population is too low, globally or in any domain, then either help it or leave it alone
		if( excess < 0 )
		{
			critter::gPopulationPenaltyFraction = 0.0;	// we're not currently applying the high population penalty

			// If we want to help it, apply the low population advantage
			if( fApplyLowPopulationAdvantage )
			{
				critter::gLowPopulationAdvantageFactor = 1.0 - (float) (initNumCritters - numCritters) / (initNumCritters - minNumCritters);
				if( critter::gLowPopulationAdvantageFactor < 0.0 )
					critter::gLowPopulationAdvantageFactor = 0.0;
				if( critter::gLowPopulationAdvantageFactor > 1.0 )
					critter::gLowPopulationAdvantageFactor = 1.0;
			}
		}
		else if( excess > 0 )	// population is greater than initial level everywhere
		{
			critter::gLowPopulationAdvantageFactor = 1.0;	// we're not currently applying the low population advantage

			// apply the high population penalty (if the max-penalty is non-zero)
			critter::gPopulationPenaltyFraction = critter::gMaxPopulationPenaltyFraction * (numCritters - initNumCritters) / (maxNumCritters - initNumCritters);
			if( critter::gPopulationPenaltyFraction < 0.0 )
				critter::gPopulationPenaltyFraction = 0.0;
			if( critter::gPopulationPenaltyFraction > critter::gMaxPopulationPenaltyFraction )
				critter::gPopulationPenaltyFraction = critter::gMaxPopulationPenaltyFraction;
		}
		
//		printf( "step=%4ld, pop=%3d, initPop=%3ld, minPop=%2ld, maxPop=%3ld, maxPopPenaltyFraction=%g, popPenaltyFraction=%g, lowPopAdvantageFactor=%g\n",
//				fStep, numCritters, initNumCritters, minNumCritters, maxNumCritters,
//				critter::gMaxPopulationPenaltyFraction, critter::gPopulationPenaltyFraction, critter::gLowPopulationAdvantageFactor );
		
		critter* c;
		objectxsortedlist::gXSortedObjects.reset();
		while (objectxsortedlist::gXSortedObjects.nextObj(CRITTERTYPE, (gobject**)&c))
		{
		#ifdef DEBUGCHECK
			debugstring[256];
			sprintf(debugstring,"in critter loop at age %ld, critnum = %ld", fStep, critnum);
			debugcheck(debugstring);
			critnum++;
		#endif DEBUGCHECK
			// Set mark on current critter since we need to check for bricks
			// in the list in order to find intersections of critters and bricks
			objectxsortedlist::gXSortedObjects.setMark(CRITTERTYPE);
			fFoodEnergyOut += c->Update(fMoveFitnessParameter, critter::gSpeed2DPosition);
			objectxsortedlist::gXSortedObjects.toMark(CRITTERTYPE);
		}
		
		// Swap buffers for the critter POV window when they're all done
		fCritterPOVWindow->swapBuffers();
	}

					
	long oldNumBorn = fNumberBorn;
	long oldNumCreated = fNumberCreated;
	
//  if( fDoCPUWork )
	{
		// Handles collisions, matings, fights, deaths, births, etc
		Interact();
	}
		
#ifdef DEBUGCHECK
	debugstring[256];
	sprintf(debugstring, "after interact at age %ld", fStep);
	debugcheck(debugstring);
#endif DEBUGCHECK

	fTotalFoodEnergyIn += fFoodEnergyIn;
	fTotalFoodEnergyOut += fFoodEnergyOut;

	fAverageFoodEnergyIn = (float(fStep - 1) * fAverageFoodEnergyIn + fFoodEnergyIn) / float(fStep);
	fAverageFoodEnergyOut = (float(fStep - 1) * fAverageFoodEnergyOut + fFoodEnergyOut) / float(fStep);
	
	// Update the various graphical windows
	if (fGraphics)
	{
		fSceneView->Draw();
		
		// Text Status window
		fTextStatusWindow->Draw();
		
		// Overhead window
		if (fOverHeadRank)
		{
			if (!fCritterTracking
				|| (fOverHeadRank != fOverHeadRankOld)
				|| !fOverheadCritter
				|| !(fOverheadCritter->Alive()))
			{
				fOverheadCritter = fCurrentFittestCritter[fOverHeadRank - 1];
				fOverHeadRankOld = fOverHeadRank;
			}
			
			if (fOverheadCritter != NULL)
			{
				fOverheadCamera.setx(fOverheadCritter->x());
                                fOverheadCamera.setz(fOverheadCritter->z());
			}
		}
		//Update the title of the overhead window with the rank, critter number and whether or not we are tracking (T) (CMB 3/26/06)
		if( fOverheadWindow && fOverheadWindow->isVisible() && fOverheadCritter)
		{
			char overheadTitle[64];
			if (fCritterTracking)
			{
			    sprintf( overheadTitle, "Overhead View (T%ld:%ld)", (long int) fOverHeadRank, fOverheadCritter->Number() );
			}else{			
			    sprintf( overheadTitle, "Overhead View (%ld:%ld)", (long int) fOverHeadRank, fOverheadCritter->Number() );
			}
			fOverheadWindow->setWindowTitle( QString(overheadTitle) );			
		}
		fOverheadWindow->Draw();
		
		// Born / (Born + Created) window
		if (fChartBorn
			&& fBirthrateWindow != NULL
		/*  && fBirthrateWindow->isVisible() */
			&& ((oldNumBorn != fNumberBorn) || (oldNumCreated != fNumberCreated)))
		{
			fBirthrateWindow->AddPoint(float(fNumberBorn) / float(fNumberBorn + fNumberCreated));
		}

		// Fitness window
		if( fChartFitness && fFitnessWindow != NULL /* && fFitnessWindow->isVisible() */ )
		{
			fFitnessWindow->AddPoint(0, fMaxFitness / fTotalFitness);
			fFitnessWindow->AddPoint(1, fCurrentMaxFitness[0] / fTotalFitness);
			fFitnessWindow->AddPoint(2, fAverageFitness / fTotalFitness);
		}
		
		// Energy flux window
		if( fChartFoodEnergy && fFoodEnergyWindow != NULL /* && fFoodEnergyWindow->isVisible() */ )
		{
			fFoodEnergyWindow->AddPoint(0, (fFoodEnergyIn - fFoodEnergyOut) / (fFoodEnergyIn + fFoodEnergyOut));
			fFoodEnergyWindow->AddPoint(1, (fTotalFoodEnergyIn - fTotalFoodEnergyOut) / (fTotalFoodEnergyIn + fTotalFoodEnergyOut));
			fFoodEnergyWindow->AddPoint(2, (fAverageFoodEnergyIn - fAverageFoodEnergyOut) / (fAverageFoodEnergyIn + fAverageFoodEnergyOut));
		}
		
		// Population window
		if( fChartPopulation && fPopulationWindow != NULL /* && fPopulationWindow->isVisible() */ )
		{				
			{
				fPopulationWindow->AddPoint(0, float(objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE)));
			}				
			
			if (fNumDomains > 1)
			{
				for (int id = 0; id < fNumDomains; id++)
					fPopulationWindow->AddPoint((id + 1), float(fDomains[id].numCritters));
			}
		}

//		dbprintf( "age=%ld, rank=%d, rankOld=%d, tracking=%s, fittest=%08lx, monitored=%08lx, alive=%s\n",
//				  fStep, fMonitorCritterRank, fMonitorCritterRankOld, BoolString( fCritterTracking ), (ulong) fCurrentFittestCritter[fMonitorCritterRank-1], (ulong) fMonitorCritter, BoolString( !(!fMonitorCritter || !fMonitorCritter->Alive()) ) );

		// Brain window
		if ((fMonitorCritterRank != fMonitorCritterRankOld)
			 || (fMonitorCritterRank && !fCritterTracking && (fCurrentFittestCritter[fMonitorCritterRank - 1] != fMonitorCritter))
			 || (fMonitorCritterRank && fCritterTracking && (!fMonitorCritter || !fMonitorCritter->Alive())))
		{			
			if (fMonitorCritter != NULL)
			{
				if (fBrainMonitorWindow != NULL)
					fBrainMonitorWindow->StopMonitoring();
			}					
							
			if (fMonitorCritterRank && fBrainMonitorWindow != NULL && fBrainMonitorWindow->isVisible() )
			{
				Q_CHECK_PTR(fBrainMonitorWindow);
				fMonitorCritter = fCurrentFittestCritter[fMonitorCritterRank - 1];
				fBrainMonitorWindow->StartMonitoring(fMonitorCritter);					
			}
			else
			{
				fMonitorCritter = NULL;
			}
		
			fMonitorCritterRankOld = fMonitorCritterRank;			
		}
		//Added T for title if we are in tracking mode (CMB 3/26/06)
		if( fBrainMonitorWindow && fBrainMonitorWindow->isVisible() && fMonitorCritter && (fStep % fBrainMonitorStride == 0) )
		{
			char title[64];
			if (fCritterTracking)
			{
			    sprintf( title, "Brain Monitor (T%ld:%ld)", fMonitorCritterRank, fMonitorCritter->Number() );
			}else{			
			    sprintf( title, "Brain Monitor (%ld:%ld)", fMonitorCritterRank, fMonitorCritter->Number() );
			}			
			fBrainMonitorWindow->setWindowTitle( QString(title) );
			fBrainMonitorWindow->Draw();
		}
	}

// xxx
//sleep( 10 );

#ifdef DEBUGCHECK
	debugstring[256];
	sprintf(debugstring,"after extra graphics at age %ld", fStep);
	debugcheck(debugstring);
#endif DEBUGCHECK

	// Archive the bestSoFar brain anatomy and function files, if we're doing that
	if( fBestSoFarBrainAnatomyRecordFrequency && ((fStep % fBestSoFarBrainAnatomyRecordFrequency) == 0) )
	{
		char s[256];
		int limit = fNumberDied < fNumberFit ? fNumberDied : fNumberFit;

		sprintf( s, "run/brain/bestSoFar/%ld", fStep );
		mkdir( s, PwDirMode );
		for( int i = 0; i < limit; i++ )
		{
			char t[256];	// target (use s for source)
			sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_incept.txt", fFittest[i]->critterID );
			sprintf( t, "run/brain/bestSoFar/%ld/%d_brainAnatomy_%ld_incept.txt", fStep, i, fFittest[i]->critterID );
			if( link( s, t ) )
				eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
			sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_birth.txt", fFittest[i]->critterID );
			sprintf( t, "run/brain/bestSoFar/%ld/%d_brainAnatomy_%ld_birth.txt", fStep, i, fFittest[i]->critterID );
			if( link( s, t ) )
				eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
			sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_death.txt", fFittest[i]->critterID );
			sprintf( t, "run/brain/bestSoFar/%ld/%d_brainAnatomy_%ld_death.txt", fStep, i, fFittest[i]->critterID );
			if( link( s, t ) )
				eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
			sprintf( s, "run/brain/function/brainFunction_%ld.txt", fFittest[i]->critterID );
			sprintf( t, "run/brain/bestSoFar/%ld/%d_brainFunction_%ld.txt", fStep, i, fFittest[i]->critterID );
			if( link( s, t ) )
				eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
			
			// Generate the bestSoFar Complexity, if we're doing that.
			if(	fRecordComplexity )
			{
				if( fUseComplexityAsFitnessFunc != 'O' )	// If using Complexity as FitnessFunc we already have this.  Virgil
				{
					fFittest[i]->Complexity = fFittest[i]->fitness;
				}
			
				if( fFittest[i]->Complexity == 0.0 )		// if Complexity is zero -- means we have to Calculate it
				{
					fFittest[i]->Complexity = CalcComplexity( t, 'P', 0 );		// Complexity of Processing Units Only, all time steps
					cout << "[COMPLEXITY] Critter: " << fFittest[i]->critterID << "\t Processing Complexity: " << fFittest[i]->Complexity << endl;
				}
			}

		}
	}
	
	// Archive the bestRecent brain anatomy and function files, if we're doing that
	if( fBestRecentBrainAnatomyRecordFrequency && ((fStep % fBestRecentBrainAnatomyRecordFrequency) == 0) )
	{
		char s[256];
		int limit = fNumberDied < fNumberRecentFit ? fNumberDied : fNumberRecentFit;

		sprintf( s, "run/brain/bestRecent/%ld", fStep );
		mkdir( s, PwDirMode );
		for( int i = 0; i < limit; i++ )
		{
			if( fRecentFittest[i]->critterID > 0 )
			{
				char t[256];	// target (use s for source)
				sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_incept.txt", fRecentFittest[i]->critterID );
				sprintf( t, "run/brain/bestRecent/%ld/%d_brainAnatomy_%ld_incept.txt", fStep, i, fRecentFittest[i]->critterID );
				if( link( s, t ) )
					eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
				sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_birth.txt", fRecentFittest[i]->critterID );
				sprintf( t, "run/brain/bestRecent/%ld/%d_brainAnatomy_%ld_birth.txt", fStep, i, fRecentFittest[i]->critterID );
				if( link( s, t ) )
					eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
				sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_death.txt", fRecentFittest[i]->critterID );
				sprintf( t, "run/brain/bestRecent/%ld/%d_brainAnatomy_%ld_death.txt", fStep, i, fRecentFittest[i]->critterID );
				if( link( s, t ) )
					eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
				sprintf( s, "run/brain/function/brainFunction_%ld.txt", fRecentFittest[i]->critterID );
				sprintf( t, "run/brain/bestRecent/%ld/%d_brainFunction_%ld.txt", fStep, i, fRecentFittest[i]->critterID );
				if( link( s, t ) )
					eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );


				if(	fRecordComplexity )
				{
					if( fUseComplexityAsFitnessFunc != 'O' )	// If using Complexity as FitnessFunc we've already computed Complexity for this critter. Virgil
					{
						fRecentFittest[i]->Complexity = fRecentFittest[i]->fitness;
//						cout << "[" << fRecentFittest[i]->critterID << "]::: Yeehaw! Was going to compute Complexity again but I don't have to.  The Fitness = Complexity = " << fRecentFittest[i]->fitness << endl;				
					}

					else	// Okay, not using ComplexityAsFitnessFunc, but if the critter is in the BestSoFar list we can get the Complexity from there w/o computing it again.
					{
	//DEBUG				cout << ":::BestRecent [" << fRecentFittest[i]->critterID << "]: ";

						bool inBestSoFarList = false;
						int j;
						for( j = 0; j < limit; j++ )
						{
							if( fRecentFittest[i]->critterID == fFittest[j]->critterID )
							{
								inBestSoFarList = true;
								break;
							}
						}

						// Check to see if we would have already computed the Complexity, if so, dont recompute it.
						if( inBestSoFarList )
						{
							fRecentFittest[i]->Complexity = fFittest[j]->Complexity;
	//DEBUG					cout << "Had critter in BestSoFarList -- no computing Complexity." << endl;
						}
						else
						{
//							char AnatFilename[256];
//							sprintf( AnatFilename, "run/brain/anatomy/brainAnatomy_%ld_death.txt", fFittest[i]->critterID );
							fRecentFittest[i]->Complexity = CalcComplexity( t, 'P', 0 );		// Complexity of Processing Units Only, all time steps
							cout << "[COMPLEXITY] Critter: " << fRecentFittest[i]->critterID << "\t Processing Complexity: " << fRecentFittest[i]->Complexity << endl;
						}
					}

				}

			}

		
		}
		
		//Calculate Mean and StdDev of the fRecentFittest Complexities.
		if( fRecordComplexity )
		{
			int limit2 = limit <= fNumberRecentFit ? limit : fNumberRecentFit;

		
			double mean=0;
			double stddev=0;	//Complexity: Time to Average and StdDev the BestRecent List
			int count=0;		//Keeps a count of the number of entries in fRecentFittest.
			
			for( int i=0; i<limit2; i++ )
			{
				if( fRecentFittest[i]->critterID > 0 )
				{
//					cout << "[" <<  fStep << "] " << fRecentFittest[i]->critterID << ": " << fRecentFittest[i]->Complexity << endl;
					mean += fRecentFittest[i]->Complexity;		// Get Sum of all Complexities
					count++;
				}
			}
		
			mean = mean / count;			// Divide by count to get the average
		

			if( ! (mean >= 0) )			// If mean is 'nan', make it zero instead of 'nan'  -- Only true before any agents have died.
			{
				mean = 0;
				stddev = 0;
			}
			else						// Calculate the stddev (You'll do this except when before an agent has died)
			{
				for( int i=0; i<limit2; i++ )
				{
					if( fRecentFittest[i]->critterID > 0 )
					{
						stddev += pow(fRecentFittest[i]->Complexity - mean, 2);		// Get Sum of all Complexities
					}
				}
			}
		
			stddev = sqrt(stddev / (count-1) );		// note that this stddev is divided by N-1 (MATLAB default)
			double StandardError = stddev / sqrt(count);
//DEBUG			cout << "Mean = " << mean << "  //  StdDev = " << stddev << endl;
			
			FILE * cFile;
			
			if( (cFile =fopen("run/brain/bestRecent/complexity.txt", "a")) == NULL )
			{
				cerr << "could not open run/brain/bestRecent/complexity.txt for writing" << endl;
				exit(1);
			}
			
			// print to complexity.txt
			fprintf( cFile, "%ld %f %f %f %d\n", fStep, mean, stddev, StandardError, count );
			fclose( cFile );
			
		}

		
		// Now delete all bestRecent critter files, unless they are also on the bestSoFar list
		// Also empty the bestRecent list here, so we start over each epoch
		int limit2 = fNumberDied < fNumberFit ? fNumberDied : fNumberFit;
		for( int i = 0; i < limit; i++ )
		{
			// First determine whether or not each bestRecent critter is in the bestSoFar list or not
			bool inBestSoFarList = false;
			for( int j = 0; j < limit2; j++ )
			{
				if( fRecentFittest[i]->critterID == fFittest[j]->critterID )
				{
					inBestSoFarList = true;
					break;
				}
			}
			
			// If each bestRecent critter is NOT in the bestSoFar list, then unlink all its files from their original location
			if( !inBestSoFarList && (fRecentFittest[i]->critterID > 0) )
			{
				sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_incept.txt", fRecentFittest[i]->critterID );
				if( unlink( s ) )
					eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
				sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_birth.txt", fRecentFittest[i]->critterID );
				if( unlink( s ) )
					eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
				sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_death.txt", fRecentFittest[i]->critterID );
				if( unlink( s ) )
					eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
				sprintf( s, "run/brain/function/brainFunction_%ld.txt", fRecentFittest[i]->critterID );
				if( unlink( s ) )
					eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
			}
			
			// Empty the bestRecent list by zeroing out critter IDs and fitnesses
			fRecentFittest[i]->critterID = 0;
			fRecentFittest[i]->fitness = 0.0;
			fRecentFittest[i]->Complexity = 0.0;
		}
	}
	
	if( fRecordAdamiComplexity && ((fStep % fAdamiComplexityRecordFrequency) == 0) )		// lets compute our AdamiComplexity -- 1-bit window
	{
			unsigned int genevalue;
			FILE * FileOneBit;
			FILE * FileTwoBit;
			FILE * FileFourBit;
			FILE * FileSummary;

			float SumInformationOneBit = 0;
			float SumInformationTwoBit = 0;
			float SumInformationFourBit = 0;
			
			float entropyOneBit[8];
			float informationOneBit[8];
			
			float entropyTwoBit[4];
			float informationTwoBit[4];
			float entropyFourBit[2];
			float informationFourBit[2];
						
			critter* c = NULL;

			if( (FileOneBit = fopen("run/genome/AdamiComplexity-1bit.txt", "a")) == NULL )
			{
				cerr << "could not open run/genome/AdamiComplexity-1bit.txt for writing [2].  Exiting." << endl;
				exit(1);
			}
			if( (FileTwoBit = fopen("run/genome/AdamiComplexity-2bit.txt", "a")) == NULL )
			{
				cerr << "could not open run/genome/AdamiComplexity-2bit.txt for writing [2].  Exiting." << endl;
				exit(1);
			}		
			if( (FileFourBit = fopen("run/genome/AdamiComplexity-4bit.txt", "a")) == NULL )
			{
				cerr << "could not open run/genome/AdamiComplexity-4bit.txt for writing [2].  Exiting." << endl;
				exit(1);
			}
			if( (FileSummary = fopen("run/genome/AdamiComplexity-summary.txt", "a")) == NULL )
			{
				cerr << "could not open run/genome/AdamiComplexity-summary.txt for writing [2].  Exiting." << endl;
				exit(1);
			}


			fprintf( FileOneBit,  "%ld:", fStep );		// write the timestep on the beginning of the line
			fprintf( FileTwoBit,  "%ld:", fStep );		// write the timestep on the beginning of the line
			fprintf( FileFourBit, "%ld:", fStep );		// write the timestep on the beginning of the line
			fprintf( FileSummary, "%ld ", fStep );		// write the timestep on the beginning of the line
			
			int numcritters = objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE);

			bool bits[numcritters][8];
		
			for( int gene = 0; gene < genome::gNumBytes; gene++ )			// for each gene ...
			{
				int count = 0;
				while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &c ) )	// for each critter ...
				{
					genevalue = c->Genes()->GeneUIntValue(gene);

					
					if( genevalue >= 128 ) { bits[count][0]=true; genevalue -= 128; } else { bits[count][0] = false; }
					if( genevalue >= 64  ) { bits[count][1]=true; genevalue -= 64; }  else { bits[count][1] = false; }
					if( genevalue >= 32  ) { bits[count][2]=true; genevalue -= 32; }  else { bits[count][2] = false; }
					if( genevalue >= 16  ) { bits[count][3]=true; genevalue -= 16; }  else { bits[count][3] = false; }
					if( genevalue >= 8   ) { bits[count][4]=true; genevalue -= 8; }   else { bits[count][4] = false; }
					if( genevalue >= 4   ) { bits[count][5]=true; genevalue -= 4; }   else { bits[count][5] = false; }
					if( genevalue >= 2   ) { bits[count][6]=true; genevalue -= 2; }   else { bits[count][6] = false; }
					if( genevalue == 1   ) { bits[count][7]=true; }                   else { bits[count][7] = false; }

					count++;
				}
				
				
				/* PRINT THE BYTE UNDER CONSIDERATION */
				
				/* DOING ONE BIT WINDOW */
				for( int i=0; i<8; i++ )		// for each window 1-bits wide...
				{
					int number_of_ones=0;
					
					for( int critter=0; critter<numcritters; critter++ )
						if( bits[critter][i] == true ) { number_of_ones++; }		// if critter has a 1 in the column, increment number_of_ones
										
					float prob_1 = (float) number_of_ones / (float) numcritters;
					float prob_0 = 1.0 - prob_1;
					float logprob_0, logprob_1;
					
					if( prob_1 == 0.0 ) logprob_1 = 0.0;
					else logprob_1 = log2(prob_1);

					if( prob_0 == 0.0 ) logprob_0 = 0.0;
					else logprob_0 = log2(prob_0);

					
					entropyOneBit[i] = -1 * ( (prob_0 * logprob_0) + (prob_1 * logprob_1) );			// calculating the entropy for this bit...
					informationOneBit[i] = 1.0 - entropyOneBit[i];										// 1.0 = maxentropy
					SumInformationOneBit += informationOneBit[i];
//DEBUG				cout << "Gene" << gene << "_bit[" << i << "]: probs =\t{" << prob_0 << "," << prob_1 << "}\tEntropy =\t" << entropyOneBit[i] << " bits\t\tInformation =\t" << informationOneBit[i] << endl;
				}
			
				fprintf( FileOneBit, " %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f", informationOneBit[0], informationOneBit[1], informationOneBit[2], informationOneBit[3], informationOneBit[4], 
					informationOneBit[5], informationOneBit[6], informationOneBit[7] );



				/* DOING TWO BIT WINDOW */

				for( int i=0; i<4; i++ )		// for each window 2-bits wide...
				{
					int number_of[4];
					for( int j=0; j<4; j++) { number_of[j] = 0; }		// zero out the array

					for( int critter=0; critter<numcritters; critter++ )
					{
						if( bits[critter][i*2] )			// Bits: 1 ?
						{
							if( bits[critter][(i*2)+1] ) { number_of[3]++; }	// Bits: 1 1
							else						 { number_of[2]++; }		// Bits: 1 0							
						}
						else							// Bits: 0 ?
						{
							if( bits[critter][(i*2)+1] ) { number_of[1]++; }		// Bits: 0 1
							else						 { number_of[0]++; }		// Bits: 0 0
						}
					}



					float prob[4];
					float logprob[4];
					float sum=0;
					
					for( int j=0; j<4; j++ )
					{
						prob[j] = (float) number_of[j] / (float) numcritters;
						if( prob[j] == 0.0 ) { logprob[j] = 0.0; }
						else { logprob[j] = log2(prob[j]); }						// in units of mers

						sum += prob[j] * logprob[j];						// H(X) = -1 * SUM{ p(x) * log p(x) }
					}

					entropyTwoBit[i] = (sum * -1);							// multiplying the sum by -1 to get the Entropy
					informationTwoBit[i] = 2.0 - entropyTwoBit[i];			// since we're in units of mers, maxentroopy is always 1.0
					SumInformationTwoBit += informationTwoBit[i];

//DEBUG				cout << "Gene" << gene << "_window2bit[" << i << "]: probs =\t{" << prob[0] << "," << prob[1] << "," << prob[2] << "," << prob[3] << "}\tEntropy =\t" << entropyTwoBit[i] << " mers\t\tInformation =\t" << informationTwoBit[i] << endl;
				}
				
				fprintf( FileTwoBit, " %.4f %.4f %.4f %.4f", informationTwoBit[0], informationTwoBit[1], informationTwoBit[2], informationTwoBit[3] );


				/* DOING FOUR BIT WINDOW */

				for( int i=0; i<2; i++ )		// for each window four-bits wide...
				{
					int number_of[16];
					for( int j=0; j<16; j++) { number_of[j] = 0; }		// zero out the array

					
					for( int critter=0; critter<numcritters; critter++ )
					{
						if( bits[critter][i*4] )					// 1 ? ? ?.  Possibilities are 8-15
						{
							if( bits[critter][(i*4)+1] )			// 1 1 ? ?.  Possibilities are 12-15
							{
								if( bits[critter][(i*4)+2] )		// 1 1 1 ?.  Possibilities are 14-15
								{
									if( bits[critter][(i*4)+3] )	{ number_of[15]++; }	// 1 1 1 1
									else							{ number_of[14]++; }	// 1 1 1 0
								}
								else								// 1 1 0 ?.  Possibilities are 12-13
								{
									if( bits[critter][(i*4)+3] )	{ number_of[13]++; }	// 1 1 0 1
									else							{ number_of[12]++; }	// 1 1 0 0
								}
							}
							else									// 1 0 ? ?.  Possibilities are 8-11
							{
								if( bits[critter][(i*4)+2] )		// 1 0 1 ?.  Possibilities are 14-15
								{
									if( bits[critter][(i*4)+3] )	{ number_of[11]++; }	// 1 0 1 1
									else							{ number_of[10]++; }	// 1 0 1 0
								}
								else								// 1 0 0 ?.  Possibilities are 12-13
								{
									if( bits[critter][(i*4)+3] )	{ number_of[9]++; }		// 1 0 0 1
									else							{ number_of[8]++; }		// 1 0 0 0
								}
							}
						}
						else										// 0 ? ? ?.  Possibilities are 0-7
						{
							if( bits[critter][(i*4)+1] )			// 0 1 ? ?.  Possibilities are 4-8
							{
								if( bits[critter][(i*4)+2] )		// 0 1 1 ?.  Possibilities are 6-7
								{
									if( bits[critter][(i*4)+3] )	{ number_of[7]++; } // 0 1 1 1
									else							{ number_of[6]++; }	// 0 1 0 0
								}
								else								// 0 1 0 ?.  Possibilities are 4-5
								{
									if( bits[critter][(i*4)+3] )	{ number_of[5]++; } // 0 1 0 1
									else							{ number_of[4]++; }	// 0 1 0 0
								}
							}
							else									// 0 0 ? ?.  Possibilities are 0-3
							{
								if( bits[critter][(i*4)+2] )		// 0 0 1 ?.  Possibilities are 2-3
								{
									if( bits[critter][(i*4)+3] )	{ number_of[3]++; } // 0 0 1 1
									else							{ number_of[2]++; }	// 0 0 1 0
								}
								else								// 0 0 0 ?.  Possibilities are 0-1
								{
									if( bits[critter][(i*4)+3] )	{ number_of[1]++; } // 0 0 0 1
									else							{ number_of[0]++; }	// 0 0 0 0
								}							
							}
						}
					}
					
					float prob[16];
					float logprob[16];
					float sum=0;
					
					for( int j=0; j<16; j++ )
					{
						prob[j] = (float) number_of[j] / (float) numcritters;
						if( prob[j] == 0.0 ) { logprob[j] = 0.0; }
						else { logprob[j] = log2(prob[j]); }

						sum += prob[j] * logprob[j];						// H(X) = -1 * SUM{ p(x) * log p(x) }
					}
					
					entropyFourBit[i] = (sum * -1);							// multiplying the sum by -1 to get the Entropy
					informationFourBit[i] = 4.0 - entropyFourBit[i];		// since we're in units of mers, maxentroopy is always 1.0
					SumInformationFourBit += informationFourBit[i];
//DEBUG				cout << "Gene" << gene << "_window4bit[" << i << "]: probs =\t{" << prob[0] << "," << prob[1] << "," << prob[2] << "," << prob[3] << "," << prob[4] << "," << 
//DEBUG					prob[5] << "," << prob[6] << "," << prob[7] << "," << prob[8] << "," << prob[9] << "," << prob[10] << "," << prob[11] << "," << 
//DEBUG					prob[12] << "," << prob[13] << "," << prob[14] << "," << prob[15] << "}\tEntropy =\t" << entropyFourBit[i] << " mers\t\tInformation =\t" << informationFourBit[i] << endl;
				}

				fprintf( FileFourBit, " %.4f %.4f", informationFourBit[0], informationFourBit[1] );
	

			}

			fprintf( FileOneBit,  "\n" );		// end the line
			fprintf( FileTwoBit,  "\n" );		// end the line
			fprintf( FileFourBit, "\n" );		// end the line
			fprintf( FileSummary, "%.4f %.4f %.4f\n", SumInformationOneBit, SumInformationTwoBit, SumInformationFourBit );		// write the timestep on the beginning of the line

			// Done computing AdamiComplexity.  Close our log files.
			fclose( FileOneBit );
			fclose( FileTwoBit );
			fclose( FileFourBit );
			fclose( FileSummary );
	}


	// Handle tracking gene Separation
	if( fMonitorGeneSeparation && fRecordGeneSeparation )
		RecordGeneSeparation();
		
	//Rotate the world a bit each time step... (CMB 3/10/06)
	if (fRotateWorld)
	{
		fCameraAngle += fCameraRotationRate;
		float camrad = fCameraAngle * DEGTORAD;
		fCamera.settranslation((0.5+fCameraRadius*sin(camrad))*globals::worldsize, fCameraHeight*globals::worldsize,(-.5+fCameraRadius*cos(camrad))*
globals::worldsize);
	}	
}


//---------------------------------------------------------------------------
// TSimulation::Init
//
// The order that initializer functions are called is important as values
// may depend on a variable initialized earlier.
//---------------------------------------------------------------------------
void TSimulation::Init()
{
 	// Set up graphical constructs
    "ground.obj" >> fGround;

    srand(1);

    // Initialize world with default values
    InitWorld();
	
	// Initialize world state from saved file if present
	ReadWorldFile("worldfile");
	
	InitNeuralValues();	 // Must be called before genome and brain init
	
	genome::genomeinit();
    brain::braininit();
    critter::critterinit();
	
	 // Following is part of one way to speed up the graphics
	 // Note:  this code must agree with the critter sizing in critter::grow()
	 // and the food sizing in food::initlen().

	float maxcritterlenx = critter::gMaxCritterSize / sqrt(genome::gMinmaxspeed);
	float maxcritterlenz = critter::gMaxCritterSize * sqrt(genome::gMaxmaxspeed);
	float maxcritterradius = 0.5 * sqrt(maxcritterlenx*maxcritterlenx + maxcritterlenz*maxcritterlenz);
	float maxfoodlen = 0.75 * food::gMaxFoodEnergy / food::gSize2Energy;
	float maxfoodradius = 0.5 * sqrt(maxfoodlen * maxfoodlen * 2.0);
	food::gMaxFoodRadius = maxfoodradius;
	critter::gMaxRadius = maxcritterradius > maxfoodradius ?
						  maxcritterradius : maxfoodradius;

    InitMonitoringWindows();

    critter* c = NULL;

    if (fNumberFit > 0)
    {
        fFittest = new FitStruct*[fNumberFit];
		Q_CHECK_PTR( fFittest );

        for (int i = 0; i < fNumberFit; i++)
        {
			fFittest[i] = new FitStruct;
			Q_CHECK_PTR( fFittest[i] );
            fFittest[i]->genes = new genome();
			Q_CHECK_PTR( fFittest[i]->genes );
            fFittest[i]->fitness = 0.0;
			fFittest[i]->critterID = 0;
			fFittest[i]->Complexity = 0.0;
        }
		
        fRecentFittest = new FitStruct*[fNumberRecentFit];
		Q_CHECK_PTR( fRecentFittest );

        for (int i = 0; i < fNumberRecentFit; i++)
        {
			fRecentFittest[i] = new FitStruct;
			Q_CHECK_PTR( fRecentFittest[i] );
            fRecentFittest[i]->genes = NULL;	// new genome();	// we don't save the genes in the bestRecent list
            fRecentFittest[i]->fitness = 0.0;
			fRecentFittest[i]->critterID = 0;
			fRecentFittest[i]->Complexity = 0.0;
        }
		
        for( int id = 0; id < fNumDomains; id++ )
        {
            fDomains[id].fittest = new FitStruct*[fNumberFit];
			Q_CHECK_PTR( fDomains[id].fittest );

            for (int i = 0; i < fNumberFit; i++)
            {
				fDomains[id].fittest[i] = new FitStruct;
				Q_CHECK_PTR( fDomains[id].fittest[i] );
                fDomains[id].fittest[i]->genes = new genome();
				Q_CHECK_PTR( fDomains[id].fittest[i]->genes );
                fDomains[id].fittest[i]->fitness = 0.0;
				fDomains[id].fittest[i]->critterID = 0;
				fDomains[id].fittest[i]->Complexity = 0.0;
            }
        }
    }

	if( fSmiteFrac > 0.0 )
	{
        for( int id = 0; id < fNumDomains; id++ )
        {
			fDomains[id].fNumLeastFit = 0;
			fDomains[id].fMaxNumLeastFit = lround( fSmiteFrac * fDomains[id].maxNumCritters );
			
			smPrint( "for domain %d fMaxNumLeastFit = %d\n", id, fDomains[id].fMaxNumLeastFit );
			
			if( fDomains[id].fMaxNumLeastFit > 0 )
			{
				fDomains[id].fLeastFit = new critter*[fDomains[id].fMaxNumLeastFit];
				
				for( int i = 0; i < fDomains[id].fMaxNumLeastFit; i++ )
					fDomains[id].fLeastFit[i] = NULL;
			}
        }
	}

	// Set up the run directory and its subsidiaries
	char s[256];
	char t[256];
	// First save the old directory, if it exists
	sprintf( s, "run" );
	sprintf( t, "run_%ld", time(NULL) );
	(void) rename( s, t );
	if( mkdir( "run", PwDirMode ) )
		eprintf( "Error making run directory (%d)\n", errno );
	if( mkdir( "run/stats", PwDirMode ) )
		eprintf( "Error making run/stats directory (%d)\n", errno );
	if( fBestSoFarBrainAnatomyRecordFrequency || fBestSoFarBrainFunctionRecordFrequency ||
		fBestRecentBrainAnatomyRecordFrequency || fBestRecentBrainFunctionRecordFrequency ||
		fBrainAnatomyRecordAll || fBrainFunctionRecordAll ||
		fBrainAnatomyRecordSeeds || fBrainFunctionRecordSeeds || fAdamiComplexityRecordFrequency )
	{
		// If we're going to be saving info on all these files, must increase the number allowed open
		if( SetMaximumFiles( fMaxCritters * 2 ) )	// 2x is overkill, but let's be safe
			eprintf( "Error setting maximum files to %ld (%d)\n", fMaxCritters * 2, errno );

		if( mkdir( "run/brain", PwDirMode ) )
			eprintf( "Error making run/brain directory (%d)\n", errno );
			
		if( mkdir( "run/genome", PwDirMode ) )
			eprintf( "Error making run/genome directory (%d)\n", errno );

			
	#define RecordRandomBrainAnatomies 0
	#if RecordRandomBrainAnatomies
		if( mkdir( "run/brain/random", PwDirMode ) )
			eprintf( "Error making run/brain/random directory (%d)\n", errno );
	#endif
		if( fBestSoFarBrainAnatomyRecordFrequency || fBestRecentBrainAnatomyRecordFrequency || fBrainAnatomyRecordAll || fBrainAnatomyRecordSeeds )
			if( mkdir( "run/brain/anatomy", PwDirMode ) )
				eprintf( "Error making run/brain/anatomy directory (%d)\n", errno );
		if( fBestSoFarBrainFunctionRecordFrequency || fBestRecentBrainFunctionRecordFrequency || fBrainFunctionRecordAll || fBrainFunctionRecordSeeds )
			if( mkdir( "run/brain/function", PwDirMode ) )
				eprintf( "Error making run/brain/function directory (%d)\n", errno );
		if( fBestSoFarBrainAnatomyRecordFrequency || fBestSoFarBrainFunctionRecordFrequency )
			if( mkdir( "run/brain/bestSoFar", PwDirMode ) )
				eprintf( "Error making run/brain/bestSoFar directory (%d)\n", errno );
		if( fBestRecentBrainAnatomyRecordFrequency || fBestRecentBrainFunctionRecordFrequency )
			if( mkdir( "run/brain/bestRecent", PwDirMode ) )
				eprintf( "Error making run/brain/bestRecent directory (%d)\n", errno );
		if( fBrainAnatomyRecordSeeds || fBrainFunctionRecordSeeds )
		{
			if( mkdir( "run/brain/seeds", PwDirMode ) )
				eprintf( "Error making run/brain/seeds directory (%d)\n", errno );
			if( fBrainAnatomyRecordSeeds )
				if( mkdir( "run/brain/seeds/anatomy", PwDirMode ) )
					eprintf( "Error making run/brain/seeds/anatomy directory (%d)\n", errno );
			if( fBrainFunctionRecordSeeds )
				if( mkdir( "run/brain/seeds/function", PwDirMode ) )
					eprintf( "Error making run/brain/seeds/function directory (%d)\n", errno );
		}
	}


	if( fRecordAdamiComplexity )
	{
		FILE * File;
		
		if( (File = fopen("run/genome/AdamiComplexity-1bit.txt", "a")) == NULL )
		{
			cerr << "could not open run/genome/AdamiComplexity-1bit.txt for writing [1]. Exiting." << endl;
			exit(1);
		}
		fprintf( File, "%% BitsInGenome: %ld WindowSize: 1\n", genome::gNumBytes * 8 );		// write the number of bits into the top of the file.
		fclose( File );

		if( (File = fopen("run/genome/AdamiComplexity-2bit.txt", "a")) == NULL )
		{
			cerr << "could not open run/genome/AdamiComplexity-2bit.txt for writing [1]. Exiting." << endl;
			exit(1);
		}
		fprintf( File, "%% BitsInGenome: %ld WindowSize: 2\n", genome::gNumBytes * 8 );		// write the number of bits into the top of the file.
		fclose( File );


		if( (File = fopen("run/genome/AdamiComplexity-4bit.txt", "a")) == NULL )
		{
			cerr << "could not open run/genome/AdamiComplexity-4bit.txt for writing [1]. Exiting." << endl;
			exit(1);
		}
		fprintf( File, "%% BitsInGenome: %ld WindowSize: 4\n", genome::gNumBytes * 8 );		// write the number of bits into the top of the file.
		fclose( File );

		if( (File = fopen("run/genome/AdamiComplexity-summary.txt", "a")) == NULL )
		{
			cerr << "could not open run/genome/AdamiComplexity-summary.txt for writing [1]. Exiting." << endl;
			exit(1);
		}
		fprintf( File, "%% Timestep 1bit 2bit 4bit\n" );
		fclose( File );
	}
	
	if( fRecordBirthsDeaths )
	{
		FILE * File;
		if( (File = fopen("run/BirthsDeaths.log", "a")) == NULL )
		{
			cerr << "could not open run/BirthsDeaths.log for writing [1]. Exiting." << endl;
			exit(1);
		}
		fprintf( File, "%% Timestep Event Critter# Parent1 Parent2\n" );
		fclose( File );
	}

	if( fLockStepWithBirthsDeathsLog )
	{

		cout << "*** Running in LOCKSTEP MODE with file 'LOCKSTEP-BirthsDeaths.log' ***" << endl;

		if( (fLockstepFile = fopen("LOCKSTEP-BirthsDeaths.log", "r")) == NULL )
		{
			cerr << "ERROR/Init(): Could not open 'LOCKSTEP-BirthsDeaths.log' for reading. Exiting." << endl;
			exit(1);
		}
		
		char LockstepLine[512];
		int currentpos=0;			// current position in fLockstepFile.

		// bypass any header information
		while( (fgets(LockstepLine, sizeof(LockstepLine), fLockstepFile)) != NULL )
		{
			if( LockstepLine[0] == '#' || LockstepLine[0] == '%' ) { currentpos = ftell( fLockstepFile ); continue; 	}				// if the line begins with a '#' or '%' (implying it is a header line), skip it.
			else { fseek( fLockstepFile, currentpos, 0 ); break; }
		}
		
		if( feof(fLockstepFile) )	// this should never happen, but lets make sure.
		{
			cout << "ERROR/Init(): Did not find any data lines in 'LOCKSTEP-BirthsDeaths.log'.  Exiting." << endl;
			exit(1);
		}
		
		system( "cp LOCKSTEP-BirthsDeaths.log run/" );		// copy the LOCKSTEP file into the run/ directory.
		SetNextLockstepEvent();								// setup for the first timestep in which Birth/Death events occurred.

	}



	// If we're going to record the gene means and std devs, we need to allocate a couple of stat arrays
	if( fRecordGeneStats )
	{
		fGeneSum  = (unsigned long*) malloc( sizeof( *fGeneSum  ) * genome::gNumBytes );
		Q_CHECK_PTR( fGeneSum );
		fGeneSum2 = (unsigned long*) malloc( sizeof( *fGeneSum2 ) * genome::gNumBytes );
		Q_CHECK_PTR( fGeneSum2 );
		
		fGeneStatsFile = fopen( "run/genestats.txt", "w" );
		Q_CHECK_PTR( fGeneStatsFile );
		
		fprintf( fGeneStatsFile, "%ld\n", genome::gNumBytes );
	}

#define PrintGeneIndexesFlag 1
#if PrintGeneIndexesFlag
	{
		FILE* f = fopen( "run/geneindex.txt", "w" );
		Q_CHECK_PTR( f );
		
		genome::PrintGeneIndexes( f );
		
		fclose( f );
	}
#endif

	//If we're recording the number of critters in or near various foodpatches, then open the stat file
	if( fRecordFoodPatchStats )
	{
			fFoodPatchStatsFile = fopen( "run/foodpatchstats.txt", "w" );
			Q_CHECK_PTR( fFoodPatchStatsFile );
	}
	
	system( "cp worldfile run/" );

    // Pass ownership of the cast to the stage [TODO] figure out ownership issues
    fStage.SetCast(&fWorldCast);

	fFoodEnergyIn = 0.0;
	fFoodEnergyOut = 0.0;

	srand48(fGenomeSeed);

	if (!fLoadState)
	{
		long numSeededDomain;
		long numSeededTotal = 0;
		int id;
		
		// first handle the individual domains
		for (id = 0; id < fNumDomains; id++)
		{
			numSeededDomain = 0;	// reset for each domain

			int limit = min((fMaxCritters - (long)objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE)), fDomains[id].initNumCritters);
			for (int i = 0; i < limit; i++)
			{
				c = critter::getfreecritter(this, &fStage);
				Q_ASSERT(c != NULL);
				
				fNumberCreated++;
				fNumberCreatedRandom++;
				fDomains[id].numcreated++;
				
				if( numSeededDomain < fDomains[id].numberToSeed )
				{
					c->Genes()->SeedGenes();
					if( drand48() < fDomains[id].probabilityOfMutatingSeeds )
						c->Genes()->Mutate();
					numSeededDomain++;
				}
				else
					c->Genes()->Randomize();

				c->grow( RecordBrainAnatomy( c->Number() ), RecordBrainFunction( c->Number() ) );
				
				fFoodEnergyIn += c->FoodEnergy();
				fStage.AddObject(c);
				
				float x = drand48() * (fDomains[id].absoluteSizeX - 0.02) + fDomains[id].startX + 0.01;
				float z = drand48() * (fDomains[id].absoluteSizeZ - 0.02) + fDomains[id].startZ + 0.01;
				//float z = -0.01 - drand48() * (globals::worldsize - 0.02);
				float y = 0.5 * critter::gCritterHeight;
			#if TestWorld
				// evenly distribute the critters
				x = fDomains[id].xleft  +  0.666 * fDomains[id].xsize;
				z = - globals::worldsize * ((float) (i+1) / (fDomains[id].initNumCritters + 1));
			#endif
				c->settranslation(x, y, z);
				
				float yaw =  360.0 * drand48();
			#if TestWorld
				// point them all the same way
				yaw = 95.0;
			#endif
				c->setyaw(yaw);
				
				objectxsortedlist::gXSortedObjects.add(c);	// stores c->listLink

				c->Domain(id);
				fDomains[id].numCritters++;
				fNeuronGroupCountStats.add( c->Brain()->NumNeuronGroups() );

			#if RecordRandomBrainAnatomies
				c->Brain()->dumpAnatomical( "run/brain/random", "birth", c->Number(), 0.0 );
			#endif
			}
			
			numSeededTotal += numSeededDomain;
		}
	
		// Handle global initial creations, if necessary
		Q_ASSERT( fInitNumCritters <= fMaxCritters );
		
		while( (int)objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE) < fInitNumCritters )
		{
			c = critter::getfreecritter( this, &fStage );
			Q_CHECK_PTR(c);

			fNumberCreated++;
			fNumberCreatedRandom++;
			
			if( numSeededTotal < fNumberToSeed )
			{
				c->Genes()->SeedGenes();
				if( drand48() < fProbabilityOfMutatingSeeds )
					c->Genes()->Mutate();
				numSeededTotal++;
			}
			else
				c->Genes()->Randomize();
			c->grow( RecordBrainAnatomy( c->Number() ), RecordBrainFunction( c->Number() ) );
			
			fFoodEnergyIn += c->FoodEnergy();
			fStage.AddObject(c);
			
			float x =  0.01 + drand48() * (globals::worldsize - 0.02);
			float z = -0.01 - drand48() * (globals::worldsize - 0.02);
			float y = 0.5 * critter::gCritterHeight;
			c->settranslation(x, y, z);

			float yaw =  360.0 * drand48();
			c->setyaw(yaw);
			
			objectxsortedlist::gXSortedObjects.add(c);	// stores c->listLink
			
			id = WhichDomain(x, z, 0);
			c->Domain(id);
			fDomains[id].numCritters++;
			fNeuronGroupCountStats.add( c->Brain()->NumNeuronGroups() );
		}
			
		// Add food to the food patches until they each have their initFoodCount number of food pieces
		for( int domainNumber = 0; domainNumber < fNumDomains; domainNumber++ )
		{
			fDomains[domainNumber].numFoodPatchesGrown = 0;
			
			for( int foodPatchNumber = 0; foodPatchNumber < fDomains[domainNumber].numFoodPatches; foodPatchNumber++ )
			{
				if( fDomains[domainNumber].fFoodPatches[foodPatchNumber].on( 0 ) )
				{
					for( int j = 0; j < fDomains[domainNumber].fFoodPatches[foodPatchNumber].initFoodCount; j++ )
					{
						if( fDomains[domainNumber].foodCount < fDomains[domainNumber].maxFoodCount )
						{
							fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[foodPatchNumber].addFood();
							fDomains[domainNumber].foodCount++;
						}
					}
					fDomains[domainNumber].fFoodPatches[foodPatchNumber].initFoodGrown( true );
					fDomains[domainNumber].numFoodPatchesGrown++;
				}
			}
		}

	#if 0
		// Add initial bricks 
		for( int i = 0; i < fNumBrickPatches; i++ )
		{
			for( int j = 0; j < fBrickPatches[i].brickCount; j++ )
			{
				fBrickPatches[i].addBrick();
			}

		}
	#endif
	}

    fTotalFoodEnergyIn = fFoodEnergyIn;
    fTotalFoodEnergyOut = fFoodEnergyOut;
    fAverageFoodEnergyIn = 0.0;
    fAverageFoodEnergyOut = 0.0;

    fGround.sety(-fGroundClearance);
    fGround.setscale(globals::worldsize);
    fGround.setcolor(fGroundColor);
    fWorldSet.Add(&fGround);
    fStage.SetSet(&fWorldSet);

	// Add barriers
	barrier* b = NULL;
	while( barrier::gXSortedBarriers.next(b) )
		fWorldSet.Add(b);

	// Set up scene and camera
	fScene.SetStage(&fStage);
	fScene.SetCamera(&fCamera);
	fCamera.SetPerspective(fCameraFOV, (float)fSceneView->width() / (float)fSceneView->height(), 0.01, 1.5 * globals::worldsize);	
	
	//The main camera will rotate around the world, so we need to set up the angle and translation  (CMB 03/10/06)
	fCameraAngle = fCameraAngleStart;
	float camrad = fCameraAngle * DEGTORAD;
	fCamera.settranslation((0.5 + fCameraRadius * sin(camrad)) * globals::worldsize,
						fCameraHeight * globals::worldsize, (-0.5 + fCameraRadius * cos(camrad)) * globals::worldsize);

//	fCamera.SetRotation(0.0, -fCameraFOV / 3.0, 0.0);	
	fCamera.setcolor(fCameraColor);
	fCamera.UseLookAt();
	fCamera.SetFixationPoint(0.5 * globals::worldsize, 0.0, -0.5 * globals::worldsize);	
	fCamera.SetRotation(0.0, 90, 0.0);  	
//	fStage.AddObject(&fCamera);
	
	//Set up the overhead camera view (CMB 3/13/06)
	// Set up overhead scene and overhead camera
	fOverheadScene.SetStage(&fStage);
	fOverheadScene.SetCamera(&fOverheadCamera);

	//Set up the overhead camera (CMB 3/13/06)
	fOverheadCamera.setcolor(fCameraColor);
//	fOverheadCamera.SetFog(false, glFogFunction(), glExpFogDensity(), glLinearFogEnd() );
	fOverheadCamera.SetPerspective(fCameraFOV, (float)fSceneView->width() / (float)fSceneView->height(),0.01, 1.5 * globals::worldsize);
	fOverheadCamera.settranslation(0.5*globals::worldsize, 0.2*globals::worldsize,-0.5*globals::worldsize);
	fOverheadCamera.SetRotation(0.0, -fCameraFOV, 0.0);
	//fOverheadCamera.UseLookAt();

	//Add the overhead camera into the scene (CMB 3/13/06)
//	fStage.AddObject(&fOverheadCamera);

	// Add scene to scene view and to overhead view
	Q_CHECK_PTR(fSceneView);
	fSceneView->SetScene(&fScene);
	fOverheadWindow->SetScene( &fOverheadScene );  //Set up overhead view (CMB 3/13/06)

#define DebugGenetics 0
#if DebugGenetics
	// This little snippet of code confirms that genetic copying, crossover, and mutation are behaving somewhat reasonably
	objectxsortedlist::gXSortedObjects.reset();
	critter* c1 = NULL;
	critter* c2 = NULL;
	genome* g1 = NULL;
	genome* g2 = NULL;
	genome g1c, g1x2, g1x2m;
	objectxsortedlist::gXSortedObjects.nextObj(CRITTERTYPE, c1 );
	objectxsortedlist::gXSortedObjects.nextObj(CRITTERTYPE, c2 );
	g1 = c1->Genes();
	g2 = c2->Genes();
	cout << "************** G1 **************" nl;
	g1->Print();
	cout << "************** G2 **************" nl;
	g2->Print();
	g1c.CopyGenes( g1 );
	g1x2.Crossover( g1, g2, false );
	g1x2m.Crossover( g1, g2, true );
	cout << "************** G1c **************" nl;
	g1c.Print();
	cout << "************** G1x2 **************" nl;
	g1x2.Print();
	cout << "************** G1x2m **************" nl;
	g1x2m.Print();
	exit(0);
#endif
	
	// Set up gene Separation monitoring
	if (fMonitorGeneSeparation)
    {
		fGeneSepVals = new float[fMaxCritters * (fMaxCritters - 1) / 2];
        fNumGeneSepVals = 0;
        CalculateGeneSeparationAll();

        if (fRecordGeneSeparation)
        {
            fGeneSeparationFile = fopen("run/genesep", "w");
	    	RecordGeneSeparation();
        }

		if (fChartGeneSeparation && fGeneSeparationWindow != NULL)
			fGeneSeparationWindow->AddPoint(fGeneSepVals, fNumGeneSepVals);
    }
	
	// Set up to record a movie, if requested
	if( fRecordMovie )
	{
		char	movieFileName[256] = "run/movie.pmv";	// put date and time into the name TODO
				
		fMovieFile = fopen( movieFileName, "wb" );
		if( !fMovieFile )
		{
			eprintf( "Error opening movie file: %s\n", movieFileName );
			exit( 1 );
		}
		
		fSceneView->setRecordMovie( fRecordMovie, fMovieFile );
	}
	
	inited = true;
}


//---------------------------------------------------------------------------
// TSimulation::InitNeuralValues
//
// Calculate neural values based either defalt neural values or persistant
// values that were loaded prior to this call.
//---------------------------------------------------------------------------
void TSimulation::InitNeuralValues()
{
    brain::gNeuralValues.maxnoninputneurgroups = brain::gNeuralValues.maxinternalneurgroups + brain::gNeuralValues.numoutneurgroups;
    brain::gNeuralValues.maxneurgroups = brain::gNeuralValues.maxnoninputneurgroups + brain::gNeuralValues.numinputneurgroups;
    brain::gNeuralValues.maxneurpergroup = brain::gNeuralValues.maxeneurpergroup + brain::gNeuralValues.maxineurpergroup;
    brain::gNeuralValues.maxinternalneurons = brain::gNeuralValues.maxneurpergroup * brain::gNeuralValues.maxinternalneurgroups;
    brain::gNeuralValues.maxinputneurons = genome::gMaxvispixels * 3 + 2;
    brain::gNeuralValues.maxnoninputneurons = brain::gNeuralValues.maxinternalneurons + brain::gNeuralValues.numoutneurgroups;
    brain::gNeuralValues.maxneurons = brain::gNeuralValues.maxinternalneurons + brain::gNeuralValues.maxinputneurons + brain::gNeuralValues.numoutneurgroups;

    // the 2's are due to the input & output neurons
    //     doubling as e & i presynaptically
    // the 3's are due to the output neurons also acting as e-neurons
    //     postsynaptically, accepting internal connections
    // the -'s are due to the output & internal neurons not self-stimulating
    brain::gNeuralValues.maxsynapses = brain::gNeuralValues.maxinternalneurons * brain::gNeuralValues.maxinternalneurons 	// internal
                + 2 * brain::gNeuralValues.numoutneurgroups * brain::gNeuralValues.numoutneurgroups					// output
                + 3 * brain::gNeuralValues.maxinternalneurons * brain::gNeuralValues.numoutneurgroups       			// internal/output
                + 2 * brain::gNeuralValues.maxinternalneurons * brain::gNeuralValues.maxinputneurons  				// internal/input
                + 2 * brain::gNeuralValues.maxinputneurons * brain::gNeuralValues.numoutneurgroups       				// input/output
                - 2 * brain::gNeuralValues.numoutneurgroups													// output/output
                - brain::gNeuralValues.maxinternalneurons;                   									// internal/internal
}


//---------------------------------------------------------------------------
// TSimulation::InitWorld
//
// Initialize simulation values. Pay attention to the ordering of the
// value initialization, as values may depend on a previously set value.
//---------------------------------------------------------------------------
void TSimulation::InitWorld()
{
	globals::worldsize = 100.0;
	globals::wraparound = false;
	globals::edges = true;
	
    fMinNumCritters = 15;
    fInitNumCritters = 15;
	fNumberToSeed = 0;
	fProbabilityOfMutatingSeeds = 0.0;
    fMinFoodCount = 15;
    fMaxFoodCount = 50;
    fMaxFoodGrown = 25;
    fInitialFoodCount = 25;
    fFoodRate = 0.1;
    fMiscCritters = 150;
	fPositionSeed = 42;
    fGenomeSeed = 42;
    fCrittersRfood = true;
    fFitness1Frequency = 100;
    fFitness2Frequency = 2;
	fEat2Consume = 20.0;
    fNumberFit = 10;
	fNumberRecentFit = 10;
    fEatFitnessParameter = 10.0;
    fMateFitnessParameter = 10.0;
    fMoveFitnessParameter = 1.0;
    fEnergyFitnessParameter = 1.0;
    fAgeFitnessParameter = 1.0;
 	fTotalFitness = fEatFitnessParameter
					+ fMateFitnessParameter
					+ fMoveFitnessParameter
					+ fEnergyFitnessParameter
					+ fAgeFitnessParameter;
    fMinMateFraction = 0.05;
	fMateWait = 25;
	fEatMateSpan = 0;
    fPower2Energy = 2.5;
	fEatThreshold = 0.2;
    fMateThreshold = 0.5;
    fFightThreshold = 0.2;
  	fGroundColor.r = 0.1;
    fGroundColor.g = 0.15;
    fGroundColor.b = 0.05;
    fCameraColor.r = 1.0;
    fCameraColor.g = 1.0;
    fCameraColor.b = 1.0;
    fCameraRadius = 0.6;
    fCameraHeight = 0.35;
    fCameraRotationRate = 0.09;
    fRotateWorld = (fCameraRotationRate != 0.0);	//Boolean for enabling or disabling world roation (CMB 3/19/06)
    fCameraAngleStart = 0.0;
    fCameraFOV = 90.0;
	fMonitorCritterRank = 1;
	fMonitorCritterRankOld = 0;
	fMonitorCritter = NULL;
	fMonitorGeneSeparation = false;
    fRecordGeneSeparation = false;

    fNumberCreated = 0;
    fNumberCreatedRandom = 0;
    fNumberCreated1Fit = 0;
    fNumberCreated2Fit = 0;
    fNumberBorn = 0;
    fNumberDied = 0;
    fNumberDiedAge = 0;
    fNumberDiedEnergy = 0;
    fNumberDiedFight = 0;
    fNumberDiedEdge = 0;
	fNumberDiedSmite = 0;
    fNumberFights = 0;
    fMaxFitness = 0.;
    fAverageFitness = 0.;
	fBirthDenials = 0;
    fMiscDenials = 0;
    fLastCreated = 0;
    fMaxGapCreate = 0;
    fMinGeneSeparation = 1.e+10;
    fMaxGeneSeparation = 0.0;
    fAverageGeneSeparation = 5.e+9;
    fNumBornSinceCreated = 0;
    fChartGeneSeparation = false; // GeneSeparation (if true, genesepmon must be true)
    fDeathProbability = 0.001;
	fSmiteMode = 'L';
    fSmiteFrac = 0.10;
	fSmiteAgeFrac = 0.25;
    fShowVision = true;
	fRecordMovie = false;
	fMovieFile = NULL;

    fFitI = 0;
    fFitJ = 1;
		
	fStep = 0;
	globals::worldsize = 100.0;

    brain::gMinWin = 22;
    brain::gDecayRate = 0.9;
    brain::gLogisticsSlope = 0.5;
    brain::gMaxWeight = 1.0;
    brain::gInitMaxWeight = 0.5;
	brain::gNumPrebirthCycles = 25;
 	brain::gNeuralValues.mininternalneurgroups = 1;
    brain::gNeuralValues.maxinternalneurgroups = 5;
    brain::gNeuralValues.mineneurpergroup = 1;
    brain::gNeuralValues.maxeneurpergroup = 8;
    brain::gNeuralValues.minineurpergroup = 1;
    brain::gNeuralValues.maxineurpergroup = 8;
	brain::gNeuralValues.numoutneurgroups = 7;
    brain::gNeuralValues.numinputneurgroups = 5;
    brain::gNeuralValues.maxbias = 1.0;
    brain::gNeuralValues.minbiaslrate = 0.0;
    brain::gNeuralValues.maxbiaslrate = 0.2;
    brain::gNeuralValues.minconnectiondensity = 0.0;
    brain::gNeuralValues.maxconnectiondensity = 1.0;
    brain::gNeuralValues.mintopologicaldistortion = 0.0;
	brain::gNeuralValues.maxtopologicaldistortion = 1.0;
    brain::gNeuralValues.maxsynapse2energy = 0.01;
    brain::gNeuralValues.maxneuron2energy = 0.1;
	
    genome::gGrayCoding = true;
    genome::gMinvispixels = 1;
    genome::gMaxvispixels = 8;
    genome::gMinMutationRate = 0.01;
    genome::gMaxMutationRate = 0.1;
    genome::gMinNumCpts = 1;
    genome::gMaxNumCpts = 5;
    genome::gMinLifeSpan = 1000;
    genome::gMaxLifeSpan = 2000;
	genome::gMinStrength = 0.5;
	genome::gMaxStrength = 2.0;
    genome::gMinmateenergy = 0.2;
    genome::gMaxmateenergy = 0.8;
    genome::gMinmaxspeed = 0.5;
    genome::gMaxmaxspeed = 1.5;
    genome::gMinlrate = 0.0;
    genome::gMaxlrate = 0.1;
    genome::gMiscBias = 1.0;
    genome::gMiscInvisSlope = 2.0;
    genome::gMinBitProb = 0.1;
    genome::gMaxBitProb = 0.9;

	fGraphics = true;
    critter::gVision = true;
    critter::gMaxVelocity = 1.0;
    fMaxCritters = 50;
    critter::gInitMateWait = 25;
    critter::gMinCritterSize = 1.0;
    critter::gMinCritterSize = 4.0;
    critter::gMinMaxEnergy = 500.0;
    critter::gMaxMaxEnergy = 1000.0;
    critter::gSpeed2DPosition = 1.0;
    critter::gYaw2DYaw = 1.0;
    critter::gMinFocus = 20.0;
    critter::gMaxFocus = 140.0;
    critter::gCritterFOV = 10.0;
    critter::gMaxSizeAdvantage = 2.5;
    critter::gEat2Energy = 0.01;
    critter::gMate2Energy = 0.1;
    critter::gFight2Energy = 1.0;
    critter::gMaxSizePenalty = 10.0;
    critter::gSpeed2Energy = 0.1;
    critter::gYaw2Energy = 0.1;
    critter::gLight2Energy = 0.01;
    critter::gFocus2Energy = 0.001;
    critter::gFixedEnergyDrain = 0.1;
    critter::gCritterHeight = 0.2;

	food::gMinFoodEnergy = 20.0;
	fMinFoodEnergyAtDeath = food::gMinFoodEnergy;
    food::gMaxFoodEnergy = 300.0;
    food::gSize2Energy = 100.0;
    food::gFoodHeight = 0.6;
    food::gFoodColor.r = 0.2;
    food::gFoodColor.g = 0.6;
    food::gFoodColor.b = 0.2;

    barrier::gBarrierHeight = 5.0;
    barrier::gBarrierColor.r = 0.35;
    barrier::gBarrierColor.g = 0.25;
    barrier::gBarrierColor.b = 0.15;

	brick::gBrickHeight = 0.5;
	brick::gBrickColor.r = 0.6;
	brick::gBrickColor.g = 0.2;
	brick::gBrickColor.b = 0.2;  
}


//---------------------------------------------------------------------------
// TSimulation::InitMonitoringWindows
//
// Create and display monitoring windows
//---------------------------------------------------------------------------

void TSimulation::InitMonitoringWindows()
{
	// Critter birthrate
	fBirthrateWindow = new TChartWindow( "born / (born + created)", "Birthrate" );
//	fBirthrateWindow->setWindowTitle( QString( "born / (born + created)" ) );
	sprintf( fBirthrateWindow->title, "born / (born + created)" );
	fBirthrateWindow->SetRange(0.0, 1.0);
	fBirthrateWindow->setFixedSize(TChartWindow::kMaxWidth, TChartWindow::kMaxHeight);

	// Critter fitness		
	fFitnessWindow = new TChartWindow( "maxfit, curmaxfit, avgfit", "Fitness", 3);
//	fFitnessWindow->setWindowTitle( QString( "maxfit, curmaxfit, avgfit" ) );
	sprintf( fFitnessWindow->title, "maxfit, curmaxfit, avgfit" );
	fFitnessWindow->SetRange(0, 0.0, 1.0);
	fFitnessWindow->SetRange(1, 0.0, 1.0);
	fFitnessWindow->SetRange(2, 0.0, 1.0);
	fFitnessWindow->SetColor(0, 1.0, 1.0, 1.0);
	fFitnessWindow->SetColor(1, 1.0, 0.3, 0.0);
	fFitnessWindow->SetColor(2, 0.0, 1.0, 1.0);
	fFitnessWindow->setFixedSize(TChartWindow::kMaxWidth, TChartWindow::kMaxHeight);	
	
	// Food energy
	fFoodEnergyWindow = new TChartWindow( "energy in, total, avg", "Energy", 3);
//	fFoodEnergyWindow->setWindowTitle( QString( "energy in, total, avg" ) );
	sprintf( fFoodEnergyWindow->title, "energy in, total, avg" );
	fFoodEnergyWindow->SetRange(0, -1.0, 1.0);
	fFoodEnergyWindow->SetRange(1, -1.0, 1.0);
	fFoodEnergyWindow->SetRange(2, -1.0, 1.0);
	fFoodEnergyWindow->setFixedSize(TChartWindow::kMaxWidth, TChartWindow::kMaxHeight);	
	
	// Population
	const Color popColor[7] =
	{
		{ 1.0, 0.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0, 0.0 },
		{ 0.0, 0.0, 1.0, 0.0 },
		{ 0.0, 1.0, 1.0, 0.0 },
		{ 1.0, 0.0, 1.0, 0.0 },
		{ 1.0, 1.0, 0.0, 0.0 },
		{ 1.0, 1.0, 1.0, 1.0 }
	};
	
	const short numpop = (fNumDomains < 2) ? 1 : (fNumDomains + 1);
    fPopulationWindow = new TChartWindow( "Population", "Population", numpop);
//	fPopulationWindow->setWindowTitle( "population size" );
	sprintf( fPopulationWindow->title, "Population" );
	for (int i = 0; i < numpop; i++)
	{
		int colorIndex = i % 7;
		fPopulationWindow->SetRange(short(i), 0, fMaxCritters);
		fPopulationWindow->SetColor(i, popColor[colorIndex]);
	}
	fPopulationWindow->setFixedSize(TChartWindow::kMaxWidth, TChartWindow::kMaxHeight);

	// Brain monitor
	fBrainMonitorWindow = new TBrainMonitorWindow();
//	fBrainMonitorWindow->setWindowTitle( QString( "Brain Monitor" ) );
	
	// Critter POV
	fCritterPOVWindow = new TCritterPOVWindow( fMaxCritters, this );
	
	// Status window
	fTextStatusWindow = new TTextStatusWindow( this );
	
	//Overhead window
	fOverheadWindow = new TOverheadView(this);			

#if 0
	// Gene separation
	fGeneSeparationWindow = new TBinChartWindow( "gene separation", "GeneSeparation" );
//	fGeneSeparationWindow->setWindowTitle( "gene separation" );
	fGeneSeparationWindow->SetRange(0.0, 1.0);
	fGeneSeparationWindow->SetExponent(0.5);
#endif
		
	// Tile and show them along the left edge of the screen
//	const int titleHeight = fBirthrateWindow->frameGeometry().height() - TChartWindow::kMaxHeight;
	const int titleHeight = fBirthrateWindow->frameGeometry().height() - fBirthrateWindow->geometry().height();

	int topY = titleHeight + kMenuBarHeight;

	if (fBirthrateWindow != NULL)
	{
		fBirthrateWindow->RestoreFromPrefs(0, topY);
		topY = fBirthrateWindow->frameGeometry().bottom() + titleHeight + 1;
	}

	if (fFitnessWindow != NULL)
	{	
		fFitnessWindow->RestoreFromPrefs(0, topY);
		topY = fFitnessWindow->frameGeometry().bottom() + titleHeight + 1;
	}
	
	if (fFoodEnergyWindow != NULL)
	{		
		fFoodEnergyWindow->RestoreFromPrefs(0, topY);
		topY = fFoodEnergyWindow->frameGeometry().bottom() + titleHeight + 1;
	}
	
	if (fPopulationWindow != NULL)
	{			
		fPopulationWindow->RestoreFromPrefs(0, topY);
		topY = fPopulationWindow->frameGeometry().bottom() + titleHeight + 1;
	}
	
	if (fGeneSeparationWindow != NULL)
	{
		fGeneSeparationWindow->RestoreFromPrefs(0, topY);
		topY = fGeneSeparationWindow->frameGeometry().bottom() + titleHeight + 1;
	}
	
	const int mainWindowTitleHeight = 22;

	if (fCritterPOVWindow != NULL)
		fCritterPOVWindow->RestoreFromPrefs( fBirthrateWindow->width() + 1, kMenuBarHeight + mainWindowTitleHeight + titleHeight );
	
	if (fBrainMonitorWindow != NULL)
		fBrainMonitorWindow->RestoreFromPrefs( fBirthrateWindow->width() + 1, kMenuBarHeight + mainWindowTitleHeight + titleHeight + titleHeight );

	if (fTextStatusWindow != NULL)
	{
		QDesktopWidget* desktop = QApplication::desktop();
		const QRect& screenSize = desktop->screenGeometry( desktop->primaryScreen() );
		fTextStatusWindow->RestoreFromPrefs( screenSize.width() - TTextStatusWindow::kDefaultWidth, kMenuBarHeight /*+ mainWindowTitleHeight + titleHeight*/ );
	}
 	
	//Open overhead window CMB 3/17/06
	if (fOverheadWindow != NULL)
		fOverheadWindow->RestoreFromPrefs( fBirthrateWindow->width() + 1, kMenuBarHeight + mainWindowTitleHeight + titleHeight + titleHeight );
                //(screenleft,screenleft+.75*xscreensize, screenbottom,screenbottom+(5./6.)*yscreensize);
}

//---------------------------------------------------------------------------
// TSimulation::Interact
//---------------------------------------------------------------------------
void TSimulation::Interact()
{
    critter* c = NULL;
    critter* d = NULL;
	critter* newCritter = NULL;
//	critter* oldCritter = NULL;
	food* f = NULL;
	//cxsortedlist newCritters;
	int newlifes = 0;
	long i;
	long j;
    short id = 0;
	short jd;
	short kd;
	bool cDied;
//	bool foodMarked = 0;
	bool ateBackwardFood;
	float cpower;
	float dpower;

	fNewDeaths = 0;

	// first x-sort all the objects
	objectxsortedlist::gXSortedObjects.sort();

#if DebugShowSort
	if( fStep == 1 )
		printf( "food::gMaxFoodRadius = %g\n", food::gMaxFoodRadius );
	if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) )
	{
		objectxsortedlist::gXSortedObjects.reset();
		printf( "********** critters at step %ld **********\n", fStep );
		while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &c ) )
		{
			printf( "  # %ld edge=%g at (%g,%g) rad=%g\n", c->Number(), c->x() - c->radius(), c->x(), c->z(), c->radius() );
			fflush( stdout );
		}
		objectxsortedlist::gXSortedObjects.reset();
		printf( "********** food at step %ld **********\n", fStep );
		while( objectxsortedlist::gXSortedObjects.nextObj( FOODTYPE, (gobject**) &f ) )
		{
			printf( "  edge=%g at (%g,%g) rad=%g\n", f->x() - f->radius(), f->x(), f->z(), f->radius() );
			fflush( stdout );
		}
	}
#endif

	fCurrentFittestCount = 0;
	smPrint( "setting fCurrentFittestCount to 0\n" );
	float prevAvgFitness = fAverageFitness; // used in smite code, to limit critters that are smitable
    fAverageFitness = 0.0;
	fNumAverageFitness = 0;	// need this because we'll only count critters that have lived at least a modest portion of their lifespan (fSmiteAgeFrac)
//	fNumLeastFit = 0;
//	fNumSmited = 0;
	for( i = 0; i < fNumDomains; i++ )
	{
		fDomains[i].fNumLeastFit = 0;
		fDomains[i].fNumSmited = 0;
	}

	// If we're saving gene stats, zero them out them here
	if( fRecordGeneStats )
	{
		for( i = 0; i < genome::gNumBytes; i++ )
		{
			fGeneSum[i] = 0;
			fGeneSum2[i] = 0;
		}
	}
	
	// reset all the FoodPatch critter counts to 0
	if( fRecordFoodPatchStats )
	{
		fNumCrittersNotInOrNearAnyFoodPatch = 0;

		for( int domainNumber = 0; domainNumber < fNumDomains; domainNumber++ )
		{
			for( int foodPatchNumber = 0; foodPatchNumber < fDomains[domainNumber].numFoodPatches; foodPatchNumber++ )
			{
				fDomains[domainNumber].fFoodPatches[foodPatchNumber].resetCritterCounts();
			}
		}
	}

	// Take care of deaths first, plus least-fit determinations
	// Also use this as a convenient place to compute some stats
	
	if( fLockStepWithBirthsDeathsLog )
	{
		// if we are running in Lockstep with a LOCKSTEP-BirthDeaths.log, we kill our critters here.
		if( fLockstepTimestep == fStep )
		{
			lsPrint( "t%d: Triggering %d random deaths...\n", fStep, fLockstepNumDeathsAtTimestep );
			
			for( int count = 0; count < fLockstepNumDeathsAtTimestep; count++ )
			{
				int i = 0;
				int numcritters = objectxsortedlist::gXSortedObjects.getCount( CRITTERTYPE );
				critter* testCritter;
				critter* randCritter = NULL;
	//				int randomIndex = int( floor( drand48() * fDomains[kd].numcritters ) );	// pick from this domain
				int randomIndex = int( floor( drand48() * numcritters ) );
				gdlink<gobject*> *saveCurr = objectxsortedlist::gXSortedObjects.getcurr();	// save the state of the x-sorted list

				// As written, randCritter may not actually be the randomIndex-th critter in the domain, but it will be close,
				// and as long as there's a single legitimate critter for killing, we will find and kill it
				objectxsortedlist::gXSortedObjects.reset();
				while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &testCritter ) )
				{
					// no qualifications for this critter.  It doesn't even need to be old enough to smite.
					randCritter = testCritter;	// as long as there's a single legitimate critter for killing, randCriter will be non-NULL

					i++;
					
					if( i > randomIndex )	// don't need to test for non-NULL randCritter, as it is always non-NULL by the time we reach here
						break;
				}

				objectxsortedlist::gXSortedObjects.setcurr( saveCurr );	// restore the state of the x-sorted list  V???
				
				assert( randCritter != NULL );		// In we're in LOCKSTEP mode, we should *always* have a critter to kill.  If we don't kill a critter, then we are no longer in sync in the LOCKSTEP-BirthsDeaths.log
				
				Death( randCritter );
				
				lsPrint( "- Killed critter %d, randomIndex = %d\n", randCritter->Number(), randomIndex );						
			}	// end of for loop
		}	// end of if( fLockstepTimestep == fStep )
	}

	fCurrentNeuronGroupCountStats.reset();
	fCurrentNeuronCountStats.reset();
	objectxsortedlist::gXSortedObjects.reset();
    while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &c ) )
    {

		fCurrentNeuronGroupCountStats.add( c->Brain()->NumNeuronGroups() );
		fCurrentNeuronCountStats.add( c->Brain()->GetNumNeurons() );

        id = c->Domain();						// Determine the domain in which the critter currently is located
	
		if( ! fLockStepWithBirthsDeathsLog )
		{
			// If we're not running in LockStep mode, allow natural deaths
			// If we're not using the LowPopulationAdvantage to prevent the population getting to low,
			// or there are enough agents that we can still afford to lose one (globally & in critter's domain)...
			if( !fApplyLowPopulationAdvantage ||
				((objectxsortedlist::gXSortedObjects.getCount( CRITTERTYPE ) > fMinNumCritters) && (fDomains[c->Domain()].numCritters > fDomains[c->Domain()].minNumCritters)) )
			{
				if ( (c->Energy() <= 0.0)		||
					 (c->Age() >= c->MaxAge())  ||
					 ((!globals::edges) && ((c->x() < 0.0) || (c->x() >  globals::worldsize) ||
											(c->z() > 0.0) || (c->z() < -globals::worldsize))) )
				{
					if (c->Age() >= c->MaxAge())
						fNumberDiedAge++;
					else if (c->Energy() <= 0.0)
						fNumberDiedEnergy++;
					else
						fNumberDiedEdge++;
					Death(c);
					continue; // nothing else to do for this poor schmo
				}
			}
		}

	#ifdef OF1
        if ( (id == 0) && (drand48() < fDeathProb) )
        {
            Death(c);
            continue;
        }
	#endif


	#ifdef DEBUGCHECK
        debugcheck("after a death in interact");
	#endif DEBUGCHECK
	
		// If we're saving gene stats, compute them here
		if( fRecordGeneStats )
		{
			for( i = 0; i < genome::gNumBytes; i++ )
			{
				fGeneSum[i] += c->Genes()->GeneUIntValue(i);
				fGeneSum2[i] += c->Genes()->GeneUIntValue(i) * c->Genes()->GeneUIntValue(i);
			}
		}
		
		// If we're saving critter-occupancy-of-food-band stats, compute them here
		if( fRecordFoodPatchStats )
		{
			// Count critters inside FoodPatches
			// Also: Count critters outside FoodPatches, but within fFoodPatchOuterRange
			for (int domainNumber = 0; domainNumber < fNumDomains; domainNumber++){
				for( int foodPatchNumber = 0; foodPatchNumber < fDomains[domainNumber].numFoodPatches; foodPatchNumber++ )
				{
					// if critter is inside, then update FoodPatch's critterInsideCount
					fDomains[domainNumber].fFoodPatches[foodPatchNumber].checkIfCritterIsInside(c->x(), c->z());  

					// if critter is inside the outerrange, then update FoodPatch's critterOuterRangeCount
					fDomains[domainNumber].fFoodPatches[foodPatchNumber].checkIfCritterIsInsideNeighborhood(c->x(), c->z());  
				}
			}
		}	
		
				
		// Figure out who is least fit, if we're doing smiting to make room for births
		
		// Do the bookkeeping for the specific domain, if we're using domains
		// Note: I think we must have at least one domain these days - lsy 6/1/05
		
		// The test against average fitness is an attempt to keep fit organisms from being smited, in general,
		// but it also helps protect against the situation when there are so few potential low-fitness candidates,
		// due to the age constraint and/or population size, that critters can end up on both the highest fitness
		// and the lowest fitness lists, which can actually cause a crash (or at least used to).
//		printf( "%ld id=%ld, domain=%d, maxLeast=%d, numLeast=%d, numCrit=%d, maxCrit-
		if( (fNumDomains > 0) && (fDomains[id].fMaxNumLeastFit > 0) )
		{
			if( ((fDomains[id].numCritters > (fDomains[id].maxNumCritters - fDomains[id].fMaxNumLeastFit))) &&	// if there are getting to be too many critters, and
				(c->Age() >= (fSmiteAgeFrac * c->MaxAge())) &&													// the current critter is old enough to consider for smiting, and
				(c->Fitness() < prevAvgFitness) &&																// the current critter has worse than average fitness,
				( (fDomains[id].fNumLeastFit < fDomains[id].fMaxNumLeastFit)	||								// (we haven't filled our quota yet, or
				  (c->Fitness() < fDomains[id].fLeastFit[fDomains[id].fNumLeastFit-1]->Fitness()) ) )			// the critter is bad enough to displace one already in the queue)
			{
				if( fDomains[id].fNumLeastFit == 0 )
				{
					// It's the first one, so just store it
					fDomains[id].fLeastFit[0] = c;
					fDomains[id].fNumLeastFit++;
					smPrint( "critter %ld added to least fit list for domain %d at position 0 with fitness %g\n", c->Number(), id, c->Fitness() );
				}
				else
				{
					int i;
					
					// Find the position to be replaced
					for( i = 0; i < fDomains[id].fNumLeastFit; i++ )
						if( c->Fitness() < fDomains[id].fLeastFit[i]->Fitness() )	// worse than the one in this slot
							break;
					
					if( i < fDomains[id].fNumLeastFit )
					{
						// We need to move some of the items in the list down
						
						// If there's room left, add a slot
						if( fDomains[id].fNumLeastFit < fDomains[id].fMaxNumLeastFit )
							fDomains[id].fNumLeastFit++;

						// move everything up one, from i to end
						for( int j = fDomains[id].fNumLeastFit-1; j > i; j-- )
							fDomains[id].fLeastFit[j] = fDomains[id].fLeastFit[j-1];

					}
					else
						fDomains[id].fNumLeastFit++;	// we're adding to the end of the list, so increment the count
					
					// Store the new i-th worst
					fDomains[id].fLeastFit[i] = c;
					smPrint( "critter %ld added to least fit list for domain %d at position %d with fitness %g\n", c->Number(), id, i, c->Fitness() );
				}
			}
		}
	}

// Following debug output is accurate only when there is a single domain
//	if( fDomains[0].fNumLeastFit > 0 )
//		printf( "%ld numSmitable = %d out of %d, from %ld critters out of %ld\n", fStep, fDomains[0].fNumLeastFit, fDomains[0].fMaxNumLeastFit, fDomains[0].numCritters, fDomains[0].maxNumCritters );

	// If we're saving gene stats, record them here
	if( fRecordGeneStats )
	{
		fprintf( fGeneStatsFile, "%ld", fStep );
		for( i = 0; i < genome::gNumBytes; i++ )
		{
			float mean, stddev;
			
			mean = (float) fGeneSum[i] / (float) objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE);
			stddev = sqrt( (float) fGeneSum2[i] / (float) objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE)  -  mean * mean );
			fprintf( fGeneStatsFile, " %.1f,%.1f", mean, stddev );
		}
		fprintf( fGeneStatsFile, "\n" );
	}
	
#if DebugSmite
	if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) )
	{
		for( id = 0; id < fNumDomains; id++ )
		{
			printf( "At age %ld in domain %d (c,n,c->fit) =", fStep, id );
			for( i = 0; i < fDomains[id].fNumLeastFit; i++ )
				printf( " (%08lx,%ld,%5.2f)", (ulong) fDomains[id].fLeastFit[i], fDomains[id].fLeastFit[i]->Number(), fDomains[id].fLeastFit[i]->Fitness() );
			printf( "\n" );
		}
	}
#endif

	//cout << "after deaths1 "; critter::gXSortedCritters.list();	//dbg

	// Now go through the list, and use the influence radius to determine
	// all possible interactions

#if DebugMaxFitness
	if( (fStep >= MinDebugStep) && (fStep <= MaxDebugStep) )
	{
		objectxsortedlist::gXSortedObjects.reset();
		objectxsortedlist::gXSortedObjects.nextObj(CRITTERTYPE, c);
		critter* lastCritter;
		objectxsortedlist::gXSortedObjects.lastObj(CRITTERTYPE, (gobject**) &lastCritter );
		printf( "%s: at age %ld about to process %ld critters, %ld pieces of food, starting with critter %08lx (%4ld), ending with critter %08lx (%4ld)\n", __FUNCTION__, fStep, objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE), objectxsortedlist::gXSortedObjects.getCount(FOODTYPE), (ulong) c, c->Number(), (ulong) lastCritter, lastCritter->Number() );
	}
#endif

	// Do Critter Healing
	if( fHealing )	// if healing is turned on...
	{
		objectxsortedlist::gXSortedObjects.reset();
    	while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &c) )	// for every critter...
			c->Heal( fCritterHealingRate, 0.0 );										// heal it if FoodEnergy > 2ndParam.
	}

	if( fLockStepWithBirthsDeathsLog )
	{
		if( fLockstepTimestep == fStep )
		{
			lsPrint( "t%d: Triggering %d random births...\n", fStep, fLockstepNumBirthsAtTimestep );

			critter* c = NULL;		// mommy
			critter* d = NULL;		// daddy
			
			
			for( int count = 0; count < fLockstepNumBirthsAtTimestep; count++ ) 
			{
				/* select mommy. */

				int i = 0;
				
				int numCritters = objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE);
				assert( numCritters < fMaxCritters );			// Since we've already done all the deaths that occurred at this timestep, we should always have enough room to process the births that happened at this timestep.
				
				critter* testCritter = NULL;
				//int randomIndex = int( round( drand48() * fDomains[kd].numCritters ) );	// pick from this domain V???
				int randomIndex = int( round( drand48() * numCritters ) );
								
				// As written, randCritter may not actually be the randomIndex-th critter in the domain, but it will be close,
				// and as long as there's a single legitimate critter for mating (right domain, long enough since last mating,
				// and long enough since birth) we will find and use it
				objectxsortedlist::gXSortedObjects.reset();
				while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &testCritter ) )
				{
					// Make sure it's old enough (anything except just birthed), and it has been long enough since it mated
					if( (testCritter->Age() > 0) && ((testCritter->Age() - testCritter->LastMate()) >= fMateWait) )
						c = testCritter;	// as long as there's a single legitimate critter for mating, Mommy will be non-NULL
					
					i++;
					
					if( (i > randomIndex) && (c != NULL) )
						break;
				}

				/* select daddy. */

				i = 0;
				randomIndex = int( round( drand48() * numCritters ) );
								
				// As written, randCritter may not actually be the randomIndex-th critter in the domain, but it will be close,
				// and as long as there's a single legitimate critter for mating (right domain, long enough since last mating, and
				// has enough energy, plus not the same as the mommy), we will find and use it
				objectxsortedlist::gXSortedObjects.reset();
				while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &testCritter ) )
				{
					// If it was not just birthed, it has been long enough since it mated, and it's not the same as mommy, it'll do for daddy.
					if( (testCritter->Age() > 0) && ((testCritter->Age() - testCritter->LastMate()) >= fMateWait) && (testCritter->Number() != c->Number()) )
						d = testCritter;	// as long as there's another single legitimate critter for mating, Daddy will be non-NULL

					i++;
						
					if( (i > randomIndex) && (d != NULL) )
						break;
				}

				assert( c != NULL && d != NULL );				// If for some reason we can't select parents, then we'll become out of sync with LOCKSTEP-BirthDeaths.log
												
				lsPrint( "* I have selected Mommy(%ld) and Daddy(%ld). Population size = %d\n", c->Number(), d->Number(), numCritters );
				
				/* === We've selected our parents, now time to birth our critter. */
					
				ttPrint( "age %ld: critters # %ld & %ld are mating randomly\n", fStep, c->Number(), d->Number() );
						
				critter* e = critter::getfreecritter( this, &fStage );
				Q_CHECK_PTR(e);

				e->Genes()->Crossover(c->Genes(), d->Genes(), true);
				e->grow( RecordBrainAnatomy( e->Number() ), RecordBrainFunction( e->Number() ) );
				float eenergy = c->mating(fMateFitnessParameter, fMateWait) + d->mating(fMateFitnessParameter, fMateWait);
				float minenergy = fMinMateFraction * ( c->MaxEnergy() + d->MaxEnergy() ) * 0.5;	// just a modest, reasonable amount; this doesn't really matter in lockstep mode
				if( eenergy < minenergy )
					eenergy = minenergy;
				e->Energy(eenergy);
				e->FoodEnergy(eenergy);
				float x =  0.01 + drand48() * (globals::worldsize - 0.02);
				float z = -0.01 - drand48() * (globals::worldsize - 0.02);
				float y = 0.5 * critter::gCritterHeight;
				e->settranslation(x, y, z);
				float yaw =  360.0 * drand48();
				c->setyaw(yaw);
				kd = WhichDomain(x, z, 0);
				e->Domain(kd);
				fStage.AddObject(e);
				objectxsortedlist::gXSortedObjects.add(e); // Add the new critter directly to the list of objects (no new critter list); the e->listLink that gets auto stored here should be valid immediately
							
				newlifes++;
				fDomains[kd].numCritters++;
				fNumberBorn++;
				fDomains[kd].numborn++;
				fNumBornSinceCreated++;
				fDomains[kd].numbornsincecreated++;
				fNeuronGroupCountStats.add( e->Brain()->NumNeuronGroups() );
				ttPrint( "age %ld: critter # %ld is born\n", fStep, e->Number() );
				birthPrint( "step %ld: critter # %ld born to %ld & %ld, at (%g,%g,%g), yaw=%g, energy=%g, domain %d (%d & %d), neurgroups=%d\n",
					fStep, e->Number(), c->Number(), d->Number(), e->x(), e->y(), e->z(), e->yaw(), e->Energy(), kd, id, jd, e->Brain()->NumNeuronGroups() );


#if SPIKING_MODEL
				if (drand48() >= .5)
					e->Brain()->scale_latest_spikes = c->Brain()->scale_latest_spikes;
				else
					e->Brain()->scale_latest_spikes = d->Brain()->scale_latest_spikes;
				printf("%f\n", e->Brain()->scale_latest_spikes);
#endif

				if( fRecordBirthsDeaths )		// If we're recording birth and death events, record the birth of our newborn.
				{
					FILE * File;
					
					if( (File = fopen("run/BirthsDeaths.log", "a")) == NULL )
					{
						cerr << "could not open run/BirthsDeaths.log for writing [2]. Exiting." << endl;
						exit(1);
					}
							
					fprintf(File, "%ld BIRTH %ld %ld %ld\n", fStep, e->Number(), c->Number(), d->Number() );
					fclose(File);
				}
											
			}	// end of loop 'for( int count=0; count<fLockstepNumBirthsAtTimestep; count++ )'
							
			SetNextLockstepEvent();		// set the timestep, NumBirths, and NumDeaths for the next Lockstep events.
			
		}	// end of 'if( fLockstepTimestep == fStep )'
					
	}	// end of 'if( fLockStepWithBirthsDeathsLog )'


	objectxsortedlist::gXSortedObjects.reset();
    while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &c ) )
    {
		// Check for new critter.  If totally new (never updated), skip this critter.
		// This is because newly born critters get added directly to the main list,
		// and we might try to process them immediately after being born, but we
		// don't want to do that until the next time step.
		if( c->Age() <= 0 )
			continue;

		// determine the domain in which the critter currently is located
        id = c->Domain();

		// now see if there's an overlap with any other critters

		objectxsortedlist::gXSortedObjects.setMark( CRITTERTYPE ); // so can point back to this critter later
		
        cDied = FALSE;
        jd = id;
        kd = id;

        while( objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &d ) ) // to end of list or...
        {
			if( d == c )
			{
				printf( "d == c\n" );
				continue;
			}
			
            if( (d->x() - d->radius()) >= (c->x() + c->radius()) )
                break;  // this guy (& everybody else in list) is too far away

            // so if we get here, then c & d are close enough in x to interact

            if( fabs(d->z() - c->z()) < (d->radius() + c->radius()) )
            {
                // and if we get here then they are also close enough in z,
                // so must actually worry about their interaction

				ttPrint( "age %ld: critters # %ld & %ld are close\n", fStep, c->Number(), d->Number() );

                jd = d->Domain();

				// now take care of mating

			#ifdef OF1
                if ( (c->Mate() > fMateThreshold) &&
                     (d->Mate() > fMateThreshold) &&          // it takes two!
                     ((c->Age() - c->LastMate()) >= fMateWait) &&
                     ((d->Age() - d->LastMate()) >= fMateWait) &&  // and some time
                     (c->Energy() > fMinMateFraction * c->MaxEnergy()) &&
                     (d->Energy() > fMinMateFraction * d->MaxEnergy()) && // and energy
					 ((fEatMateSpan == 0) || ((c->LastEat()-fStep < fEatMateSpan) && (d->LastEat()-fStep < fEatMateSpan))) &&	// and they've eaten recently enough (if we're enforcing that)
                     (kd == 1) && (jd == 1) ) // in the safe domain
			#else
                if( !fLockStepWithBirthsDeathsLog &&
					(c->Mate() > fMateThreshold) &&
					(d->Mate() > fMateThreshold) &&          // it takes two!
					((c->Age() - c->LastMate()) >= fMateWait) &&
					((d->Age() - d->LastMate()) >= fMateWait) &&  // and some time
					(c->Energy() > fMinMateFraction * c->MaxEnergy()) &&
					(d->Energy() > fMinMateFraction * d->MaxEnergy()) &&	// and energy
					((fEatMateSpan == 0) || ((fStep-c->LastEat() < fEatMateSpan) && (fStep-d->LastEat() < fEatMateSpan))) )	// and they've eaten recently enough (if we're enforcing that)
			#endif
                {
                    kd = WhichDomain(0.5*(c->x()+d->x()),
                                     0.5*(c->z()+d->z()),
                                     kd);

					bool smited = false;

					if( fSmiteMode == 'L' )		// smite the least fit
					{
						if( (fDomains[kd].numCritters >= fDomains[kd].maxNumCritters) &&	// too many critters to reproduce withing a bit of smiting
							(fDomains[kd].fNumLeastFit > fDomains[kd].fNumSmited) )			// we've still got some left that are suitable for smiting
						{
							while( (fDomains[kd].fNumSmited < fDomains[kd].fNumLeastFit) &&		// there are any left to smite
								  ((fDomains[kd].fLeastFit[fDomains[kd].fNumSmited] == c) ||	// trying to smite mommy
								   (fDomains[kd].fLeastFit[fDomains[kd].fNumSmited] == d) ||	// trying to smite daddy
								   ((fCurrentFittestCount > 0) && (fDomains[kd].fLeastFit[fDomains[kd].fNumSmited]->Fitness() >= fCurrentMaxFitness[fCurrentFittestCount-1]))) )	// trying to smite one of the fittest
							{
								// We would have smited one of our mating pair, or one of the fittest, which wouldn't be prudent,
								// so just step over them and see if there's someone else to smite
								fDomains[kd].fNumSmited++;
							}
							if( fDomains[kd].fNumSmited < fDomains[kd].fNumLeastFit )	// we've still got someone to smite, so do it
							{
								smPrint( "About to smite least-fit critter #%d in domain %d\n", fDomains[kd].fLeastFit[fDomains[kd].fNumSmited]->Number(), kd );
								Death( fDomains[kd].fLeastFit[fDomains[kd].fNumSmited] );
								fDomains[kd].fNumSmited++;
								fNumberDiedSmite++;
								smited = true;
								//cout << "********************* SMITE *******************" nlf;	//dbg
							}
						}
					}
					else if( fSmiteMode == 'R' )				/// RANDOM SMITE
					{
						// If necessary, smite a random critter in this domain
						if( fDomains[kd].numCritters >= fDomains[kd].maxNumCritters )
						{
							int i = 0;
							critter* testCritter;
							critter* randCritter = NULL;
							int randomIndex = int( floor( drand48() * fDomains[kd].numCritters ) );	// pick from this domain

							gdlink<gobject*> *saveCurr = objectxsortedlist::gXSortedObjects.getcurr();	// save the state of the x-sorted list

							// As written, randCritter may not actually be the randomIndex-th critter in the domain, but it will be close,
							// and as long as there's a single legitimate critter for smiting (right domain, old enough, and not one of the
							// parents), we will find and smite it
							objectxsortedlist::gXSortedObjects.reset();
							while( (i <= randomIndex) && objectxsortedlist::gXSortedObjects.nextObj( CRITTERTYPE, (gobject**) &testCritter ) )
							{
								// If it's from the right domain, it's old enough, and it's not one of the parents, allow it
								if( testCritter->Domain() == kd )
								{
									i++;	// if it's in the right domain, increment even if we're not allowed to smite it
									
									if( (testCritter->Age() > fSmiteAgeFrac*testCritter->MaxAge()) && (testCritter->Number() != c->Number()) && (testCritter->Number() != d->Number()) )
										randCritter = testCritter;	// as long as there's a single legitimate critter for smiting in this domain, randCriter will be non-NULL
								}
								
								if( (i > randomIndex) && (randCritter != NULL) )
									break;
							}
																
							objectxsortedlist::gXSortedObjects.setcurr( saveCurr );	// restore the state of the x-sorted list
							
							if( randCritter )	// if we found any legitimately smitable critter...
							{
								fDomains[kd].fNumSmited++;
								fNumberDiedSmite++;
								smited = true;
								Death( randCritter );
							}
							
						}
						
					}

					
                    if ( (fDomains[kd].numCritters < fDomains[kd].maxNumCritters) &&
                         (objectxsortedlist::gXSortedObjects.getCount( CRITTERTYPE ) < fMaxCritters) )
                    {
						// Still got room for more
                        if( (fMiscCritters < 0) ||									// miscegenation function not being used
							(fDomains[kd].numbornsincecreated < fMiscCritters) ||	// miscegenation function not in use yet
							(drand48() < c->MateProbability(d)) )					// miscegenation function allows the birth
                        {
							ttPrint( "age %ld: critters # %ld & %ld are mating\n", fStep, c->Number(), d->Number() );
                            fNumBornSinceCreated++;
                            fDomains[kd].numbornsincecreated++;
							
                            critter* e = critter::getfreecritter(this, &fStage);
                            Q_CHECK_PTR(e);

                            e->Genes()->Crossover(c->Genes(), d->Genes(), true);
                            e->grow( RecordBrainAnatomy( e->Number() ), RecordBrainFunction( e->Number() ) );
                            float eenergy = c->mating(fMateFitnessParameter, fMateWait) + d->mating(fMateFitnessParameter, fMateWait);
                            e->Energy(eenergy);
                            e->FoodEnergy(eenergy);
                            e->settranslation(0.5*(c->x() + d->x()),
                                              0.5*(c->y() + d->y()),
                                              0.5*(c->z() + d->z()));
                            e->setyaw( AverageAngles(c->yaw(), d->yaw()) );	// wrong: 0.5*(c->yaw() + d->yaw()));   // was (360.0*drand48());
                            e->Domain(kd);
                            fStage.AddObject(e);
							gdlink<gobject*> *saveCurr = objectxsortedlist::gXSortedObjects.getcurr();
							objectxsortedlist::gXSortedObjects.add(e); // Add the new critter directly to the list of objects (no new critter list); the e->listLink that gets auto stored here should be valid immediately
							objectxsortedlist::gXSortedObjects.setcurr( saveCurr );
							
							newlifes++;
							//newCritters.add(e); // add it to the full list later; the e->listLink that gets auto stored here must be replaced with one from full list below
                            fDomains[kd].numCritters++;
                            fNumberBorn++;
                            fDomains[kd].numborn++;
							fNeuronGroupCountStats.add( e->Brain()->NumNeuronGroups() );
							ttPrint( "age %ld: critter # %ld is born\n", fStep, e->Number() );
							birthPrint( "step %ld: critter # %ld born to %ld & %ld, at (%g,%g,%g), yaw=%g, energy=%g, domain %d (%d & %d), neurgroups=%d\n",
										fStep, e->Number(), c->Number(), d->Number(), e->x(), e->y(), e->z(), e->yaw(), e->Energy(), kd, id, jd, e->Brain()->NumNeuronGroups() );
							//if( fStep > 50 )
							//	exit( 0 );
#if SPIKING_MODEL
						if (drand48() >= .5)
							e->Brain()->scale_latest_spikes = c->Brain()->scale_latest_spikes;
						else
							e->Brain()->scale_latest_spikes = d->Brain()->scale_latest_spikes;
						printf("%f\n", e->Brain()->scale_latest_spikes);
#endif							
							if( fRecordBirthsDeaths )
							{
								FILE * File;
							
								if( (File = fopen("run/BirthsDeaths.log", "a")) == NULL )
								{
									cerr << "could not open run/BirthsDeaths.log for writing [1]. Exiting." << endl;
									exit(1);
								}
								
								fprintf(File, "%ld BIRTH %ld %ld %ld\n", fStep, e->Number(), c->Number(), d->Number() );
								
								fclose(File);
							}
                        }
                        else	// miscegenation function denied this birth
						{
							fBirthDenials++;
                            fMiscDenials++;
						}
                    }
					else	// Too many critters
						fBirthDenials++;
                }
			
			

			#ifdef DEBUGCHECK
                debugcheck("after a birth in interact");
			#endif DEBUGCHECK

				// now take care of fighting

                if (fPower2Energy > 0.0)
                {
                    if ( (c->Fight() > fFightThreshold) )
                        cpower = c->Strength() * c->SizeAdvantage() * c->Fight() * (c->Energy()/c->MaxEnergy());
                    else
                        cpower = 0.0;

                    if ( (d->Fight() > fFightThreshold) )
                        dpower = d->Strength() * d->SizeAdvantage() * d->Fight() * (d->Energy()/d->MaxEnergy());
                    else
                        dpower = 0.0;

                    if ( (cpower > 0.0) || (dpower > 0.0) )
                    {
						ttPrint( "age %ld: critters # %ld & %ld are fighting\n", fStep, c->Number(), d->Number() );
                        // somebody wants to fight
                        fNumberFights++;
                        c->damage(dpower * fPower2Energy);
                        d->damage(cpower * fPower2Energy);
						if( !fLockStepWithBirthsDeathsLog )
						{
							// If we're not running in LockStep mode, allow natural deaths
							if (d->Energy() <= 0.0)
							{
								//cout << "before deaths2 "; critter::gXSortedCritters.list();	//dbg
								Death(d);
								fNumberDiedFight++;
								//cout << "after deaths2 "; critter::gXSortedCritters.list();	//dbg
							}
							if (c->Energy() <= 0.0)
							{
								objectxsortedlist::gXSortedObjects.toMark( CRITTERTYPE ); // point back to c
								Death(c);
								fNumberDiedFight++;

								// note: this leaves list pointing to item before c, and markedCritter set to previous critter
								//objectxsortedlist::gXSortedObjects.setMarkPrevious( CRITTERTYPE );	// if previous object was a critter, this would step one too far back, I think - lsy
								//cout << "after deaths3 "; critter::gXSortedCritters.list();	//dbg
								cDied = true;
								break;
							}
						}
                    }
                }

			#ifdef DEBUGCHECK
                debugcheck("after fighting in interact");
			#endif DEBUGCHECK

            }  // if close enough
        }  // while (critter::gXSortedCritters.next(d))

	#ifdef DEBUGCHECK
        debugcheck("after all critter interactions in interact");
	#endif DEBUGCHECK

        if( cDied )
			continue; // nothing else to do with c, it's gone!
	
		// they finally get to eat (couldn't earlier to keep from conferring
		// a special advantage on critters early in the sorted list)

		// Just to be slightly more like the old multi-x-sorted list version of the code, look backwards first

		// set the list back to the critter mark, so we can look backward from that point
		objectxsortedlist::gXSortedObjects.toMark( CRITTERTYPE ); // point list back to c
	
		// look for food in the -x direction
		ateBackwardFood = false;
	#if CompatibilityMode
		// go backwards in the list until we reach a place where even the largest possible piece of food
		// would entirely precede our critter, and no smaller piece of food sorting after it, but failing
		// to reach the critter can prematurely terminate the scan back (hence the factor of 2.0),
		// so we can then search forward from there
		while( objectxsortedlist::gXSortedObjects.prevObj( FOODTYPE, (gobject**) &f ) )
			if( (f->x() + 2.0*food::gMaxFoodRadius) < (c->x() - c->radius()) )
				break;
	#else CompatibilityMode
        while( objectxsortedlist::gXSortedObjects.prevObj( FOODTYPE, (gobject**) &f ) )
        {
			if( (f->x() + f->radius()) < (c->x() - c->radius()) )
			{
				// end of food comes before beginning of critter, so there is no overlap
				// if we've gone so far back that the largest possible piece of food could not overlap us,
				// then we can stop searching for this critter's possible foods in the backward direction
				if( (f->x() + 2.0*food::gMaxFoodRadius) < (c->x() - c->radius()) )
					break;  // so get out of the backward food while loop
			}
			else
			{
				// beginning of food comes before end of critter, so there is overlap in x
				// time to check for overlap in z
				if( fabs( f->z() - c->z() ) < ( f->radius() + c->radius() ) )
				{
					// also overlap in z, so they really interact
					ttPrint( "step %ld: critter # %ld is eating\n", fStep, c->Number() );
					float foodEaten = c->eat( f, fEatFitnessParameter, fEat2Consume, fEatThreshold, fStep );
					fFoodEnergyOut += foodEaten;
					
					eatPrint( "at step %ld, critter %ld at (%g,%g) with rad=%g wasted %g units of food at (%g,%g) with rad=%g\n", fStep, c->Number(), c->x(), c->z(), c->radius(), foodEaten, f->x(), f->z(), f->radius() );

					if (f->energy() <= 0.0)  // all gone
					{
						// A food was used up so remove it from the patch count and domain count
						(f->getPatch())->foodCount--;
						fDomains[f->domain()].foodCount--;

						objectxsortedlist::gXSortedObjects.removeCurrentObject();   // get it out of the list
						fStage.RemoveObject( f );  // get it out of the world
						delete f;				// get it out of memory
					}

					// but this guy only gets to eat from one food source
					ateBackwardFood = true;
					break;  // so get out of the backward food while loop
				}
			}
		}	// backward while loop on food
	#endif CompatibilityMode
	
		if( !ateBackwardFood )
		{
		#if ! CompatibilityMode
			// set the list back to the critter mark, so we can look forward from that point
			objectxsortedlist::gXSortedObjects.toMark( CRITTERTYPE ); // point list back to c
		#endif
		
			// look for food in the +x direction
			while( objectxsortedlist::gXSortedObjects.nextObj( FOODTYPE, (gobject**) &f ) )
			{
				if( (f->x() - f->radius()) > (c->x() + c->radius()) )
				{
					// beginning of food comes after end of critter, so there is no overlap,
					// and we can stop searching for this critter's possible foods in the forward direction
					break;  // so get out of the forward food while loop
				}
				else
				{
		#if CompatibilityMode
					if( ((f->x() + f->radius()) > (c->x() - c->radius()))  &&		// end of food comes after beginning of critter, and
						(fabs( f->z() - c->z() ) < (f->radius() + c->radius())) )	// there is overlap in z
		#else
					// beginning of food comes before end of critter, so there is overlap in x
					// time to check for overlap in z
					if( fabs( f->z() - c->z() ) < (f->radius() + c->radius()) )
		#endif
					{
						// also overlap in z, so they really interact
						ttPrint( "step %ld: critter # %ld is eating\n", fStep, c->Number() );
						float foodEaten = c->eat( f, fEatFitnessParameter, fEat2Consume, fEatThreshold, fStep );
						fFoodEnergyOut += foodEaten;
						
						eatPrint( "at step %ld, critter %ld at (%g,%g) with rad=%g wasted %g units of food at (%g,%g) with rad=%g\n", fStep, c->Number(), c->x(), c->z(), c->radius(), foodEaten, f->x(), f->z(), f->radius() );

						if (f->energy() <= 0.0)  // all gone
						{
							// A food was used up so remove it from the patch count and domain count
							FoodPatch* fp = f->getPatch();
							if( fp )
								fp->foodCount--;
							fDomains[f->domain()].foodCount--;

							objectxsortedlist::gXSortedObjects.removeCurrentObject();   // get it out of the list
							fStage.RemoveObject( f );  // get it out of the world
							delete f;				// get it out of memory
						}

						// but this guy only gets to eat from one food source
						break;  // so get out of the forward food while loop
					}
				}
			} // forward while loop on food

		}
	
		objectxsortedlist::gXSortedObjects.toMark( CRITTERTYPE ); // point list back to c
		
	#ifdef DEBUGCHECK
        debugcheck( "after eating in interact" );
	#endif DEBUGCHECK

		// keep tabs of current and average fitness for surviving organisms

		if( c->Age() >= (fSmiteAgeFrac * c->MaxAge()) )
		{
			fAverageFitness += c->Fitness();
			fNumAverageFitness++;
		}
        if( (fCurrentFittestCount < MAXFITNESSITEMS) || (c->Fitness() > fCurrentMaxFitness[fCurrentFittestCount-1]) )
        {
			if( (fCurrentFittestCount == 0) || ((c->Fitness() <= fCurrentMaxFitness[fCurrentFittestCount-1]) && (fCurrentFittestCount < MAXFITNESSITEMS)) )	// just append
			{
				fCurrentMaxFitness[fCurrentFittestCount] = c->Fitness();
				fCurrentFittestCritter[fCurrentFittestCount] = c;
				fCurrentFittestCount++;
			#if DebugMaxFitness
				printf( "appended critter %08lx (%4ld) to fittest list at position %d with fitness %g, count = %d\n", (ulong) c, c->Number(), fCurrentFittestCount-1, c->Fitness(), fCurrentFittestCount );
			#endif
			}
			else	// must insert
			{
				for( i = 0; i <  fCurrentFittestCount ; i++ )
				{
					if( c->Fitness() > fCurrentMaxFitness[i] )
						break;
				}
				
				for( j = min( fCurrentFittestCount, MAXFITNESSITEMS-1 ); j > i; j-- )
				{
					fCurrentMaxFitness[j] = fCurrentMaxFitness[j-1];
					fCurrentFittestCritter[j] = fCurrentFittestCritter[j-1];
				}
				
				fCurrentMaxFitness[i] = c->Fitness();
				fCurrentFittestCritter[i] = c;
				if( fCurrentFittestCount < MAXFITNESSITEMS )
					fCurrentFittestCount++;
			#if DebugMaxFitness
				printf( "inserted critter %08lx (%4ld) into fittest list at position %ld with fitness %g, count = %d\n", (ulong) c, c->Number(), i, c->Fitness(), fCurrentFittestCount );
			#endif
			}
        }

    } // while loop on critters

//	fAverageFitness /= critter::gXSortedCritters.count();
	if( fNumAverageFitness > 0 )
		fAverageFitness /= fNumAverageFitness;

#if DebugMaxFitness
	printf( "At age %ld (c,n,fit,c->fit) =", fStep );
	for( i = 0; i < fCurrentFittestCount; i++ )
		printf( " (%08lx,%ld,%5.2f,%5.2f)", (ulong) fCurrentFittestCritter[i], fCurrentFittestCritter[i]->Number(), fCurrentMaxFitness[i], fCurrentFittestCritter[i]->Fitness() );
	printf( "\n" );
#endif

    if (fMonitorGeneSeparation && (fNewDeaths > 0))
        CalculateGeneSeparationAll();

	// now for a little spontaneous generation!
	
    if (((long)(objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE))) < fMaxCritters)
    {
        // provided there are less than the maximum number of critters already

		// first deal with the individual domains
        for (id = 0; id < fNumDomains; id++)
        {
            while (fDomains[id].numCritters < fDomains[id].minNumCritters)
            {
                fNumberCreated++;
                fDomains[id].numcreated++;
                fNumBornSinceCreated = 0;
                fDomains[id].numbornsincecreated = 0;
                fLastCreated = fStep;
                fDomains[id].lastcreate = fStep;
                critter* newCritter = critter::getfreecritter(this, &fStage);
                Q_CHECK_PTR(newCritter);

                if ( fNumberFit && (fDomains[id].numdied >= fNumberFit) )
                {
                    // the list exists and is full
                    if (fFitness1Frequency
                    	&& ((fDomains[id].numcreated / fFitness1Frequency) * fFitness1Frequency == fDomains[id].numcreated) )
                    {
                        // revive 1 fittest
                        newCritter->Genes()->CopyGenes(fDomains[id].fittest[0]->genes);
                        fNumberCreated1Fit++;
						gaPrint( "%5ld: domain %d creation from one fittest (%4lu) %4ld\n", fStep, id, fDomains[id].fittest[0]->critterID, fNumberCreated1Fit );
                    }
                    else if (fFitness2Frequency
                    		 && ((fDomains[id].numcreated / fFitness2Frequency) * fFitness2Frequency == fDomains[id].numcreated) )
                    {
                        // mate 2 from array of fittest
                        newCritter->Genes()->Crossover(fDomains[id].fittest[fDomains[id].ifit]->genes,
                            				  		   fDomains[id].fittest[fDomains[id].jfit]->genes,
                            				  		   true);
                        fNumberCreated2Fit++;
						gaPrint( "%5ld: domain %d creation from two (%d, %d) fittest (%4lu, %4lu) %4ld\n", fStep, id, fDomains[id].ifit, fDomains[id].jfit, fDomains[id].fittest[fDomains[id].ifit]->critterID, fDomains[id].fittest[fDomains[id].jfit]->critterID, fNumberCreated2Fit );
                        ijfitinc(&(fDomains[id].ifit), &(fDomains[id].jfit));
						
                    }
                    else
                    {
                        // otherwise, just generate a random, hopeful monster
                        newCritter->Genes()->Randomize();
                        fNumberCreatedRandom++;
						gaPrint( "%5ld: domain %d creation random (%4ld)\n", fStep, id, fNumberCreatedRandom );
                    }
                }
                else
                {
                    // otherwise, just generate a random, hopeful monster
                    newCritter->Genes()->Randomize();
                    fNumberCreatedRandom++;
					gaPrint( "%5ld: domain %d creation random early (%4ld)\n", fStep, id, fNumberCreatedRandom );
                }

                newCritter->grow( RecordBrainAnatomy( newCritter->Number() ), RecordBrainFunction( newCritter->Number() ) );
                fFoodEnergyIn += newCritter->FoodEnergy();
				float x = drand48() * (fDomains[id].absoluteSizeX - 0.02) + fDomains[id].startX + 0.01;
				float z = drand48() * (fDomains[id].absoluteSizeZ - 0.02) + fDomains[id].startZ + 0.01;
				float y = 0.5 * critter::gCritterHeight;
				float yaw = drand48() * 360.0;
			#if TestWorld
				x = fDomains[id].xleft  +  0.666 * fDomains[id].xsize;
				z = - globals::worldsize * ((float) (i+1) / (fDomains[id].initNumCritters + 1));
				yaw = 95.0;
			#endif
                newCritter->settranslation( x, y, z );
                newCritter->setyaw( yaw );
                newCritter->Domain(id);
                fStage.AddObject(newCritter);
                fDomains[id].numCritters++;
				objectxsortedlist::gXSortedObjects.add(newCritter);
				newlifes++;
                //newCritters.add(newCritter); // add it to the full list later; the e->listLink that gets auto stored here must be replaced with one from full list below
				fNeuronGroupCountStats.add( newCritter->Brain()->NumNeuronGroups() );
				
				if( fRecordBirthsDeaths )
				{
					FILE * File;
					if( (File = fopen("run/BirthsDeaths.log", "a")) == NULL )
					{
						cerr << "could not open run/genome/AdamiComplexity-summary.txt for writing [1]. Exiting." << endl;
						exit(1);
					}
					fprintf( File, "%ld CREATION %ld\n", fStep, newCritter->Number() );
					fclose( File );
				}
            }
        }

	#ifdef DEBUGCHECK
        debugcheck("after domain creations in interact");
	#endif DEBUGCHECK
		
		// then deal with global creation if necessary

        while (((long)(objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE))) < fMinNumCritters)
        {
            fNumberCreated++;
            numglobalcreated++;

            if ((numglobalcreated == 1) && (fNumDomains > 1))
                errorflash(0, "Possible global influence on domains due to minNumCritters");

            fNumBornSinceCreated = 0;
            fLastCreated = fStep;

            newCritter = critter::getfreecritter(this, &fStage);
            Q_CHECK_PTR(newCritter);

            if( fNumberFit && (fNumberDied >= fNumberFit) )
            {
                // the list exists and is full
                if( fFitness1Frequency
                	&& ((numglobalcreated / fFitness1Frequency) * fFitness1Frequency == numglobalcreated) )
                {
                    // revive 1 fittest
                    newCritter->Genes()->CopyGenes( fFittest[0]->genes );
                    fNumberCreated1Fit++;
					gaPrint( "%5ld: global creation from one fittest (%4lu) %4ld\n", fStep, fFittest[0]->critterID, fNumberCreated1Fit );
                }
                else if( fFitness2Frequency && ((numglobalcreated / fFitness2Frequency) * fFitness2Frequency == numglobalcreated) )
                {
                    // mate 2 from array of fittest
                    newCritter->Genes()->Crossover( fFittest[fFitI]->genes, fFittest[fFitJ]->genes, true );
                    fNumberCreated2Fit++;
					gaPrint( "%5ld: global creation from two (%d, %d) fittest (%4lu, %4lu) %4ld\n", fStep, fFitI, fFitJ, fFittest[fFitI]->critterID, fFittest[fFitJ]->critterID, fNumberCreated2Fit );
                    ijfitinc( &fFitI, &fFitJ );
                }
                else
                {
                    // otherwise, just generate a random, hopeful monster
                    newCritter->Genes()->Randomize();
                    fNumberCreatedRandom++;
					gaPrint( "%5ld: global creation random (%4ld)\n", fStep, fNumberCreatedRandom );
                }
            }
            else
            {
                // otherwise, just generate a random, hopeful monster
                newCritter->Genes()->Randomize();
                fNumberCreatedRandom++;
				gaPrint( "%5ld: global creation random early (%4ld)\n", fStep, fNumberCreatedRandom );
            }

            newCritter->grow( RecordBrainAnatomy( newCritter->Number() ), RecordBrainFunction( newCritter->Number() ) );
            fFoodEnergyIn += newCritter->FoodEnergy();
            newCritter->settranslation(drand48() * globals::worldsize, 0.5 * critter::gCritterHeight, drand48() * -globals::worldsize);
            newCritter->setyaw(drand48() * 360.0);
            id = WhichDomain(newCritter->x(), newCritter->z(), 0);
            newCritter->Domain(id);
            fDomains[id].numcreated++;
            fDomains[id].lastcreate = fStep;
            fDomains[id].numCritters++;
            fStage.AddObject(newCritter);
	    	objectxsortedlist::gXSortedObjects.add(newCritter); // add new critter to list of all objejcts; the newCritter->listLink that gets auto stored here should be valid immediately
	    	newlifes++;
            //newCritters.add(newCritter); // add it to the full list later; the e->listLink that gets auto stored here must be replaced with one from full list below
			fNeuronGroupCountStats.add( newCritter->Brain()->NumNeuronGroups() );

			if( fRecordBirthsDeaths )
			{
				FILE * File;
				if( (File = fopen("run/BirthsDeaths.log", "a")) == NULL )
				{
					cerr << "could not open run/genome/AdamiComplexity-summary.txt for writing [1]. Exiting." << endl;
					exit(1);
				}
				fprintf( File, "%ld CREATION %ld\n", fStep, newCritter->Number() );
				fclose( File );
			}
						
        }

	#ifdef DEBUGCHECK
        debugcheck("after global creations in interact");
	#endif DEBUGCHECK
    }

#ifdef DEBUGCHECK
    debugcheck("after newcritters added to critter::gXSortedCritters in interact");
#endif DEBUGCHECK

    if ((newlifes || fNewDeaths) && fMonitorGeneSeparation)
    {
        if (fRecordGeneSeparation)
            RecordGeneSeparation();

		if (fChartGeneSeparation && fGeneSeparationWindow != NULL)
			fGeneSeparationWindow->AddPoint(fGeneSepVals, fNumGeneSepVals);
    }

	// finally, keep the world's food supply going...
	// Go through each of the food patches and bring them up to minFoodCount size
	// and create a new piece based on the foodRate probability
	if( (long)objectxsortedlist::gXSortedObjects.getCount(FOODTYPE) < fMaxFoodCount ) 
	{
		for( int domainNumber = 0; domainNumber < fNumDomains; domainNumber++ )
		{
			// If there are any dynamic food patches that have not yet had their initial growth,
			// see if they're ready to grow now
			if( fDomains[domainNumber].numFoodPatchesGrown < fDomains[domainNumber].numFoodPatches )
			{
				// If there are dynamic food patches that have not yet had their initial growth,
				// and they are now active, perform the initial growth now
				for( int patchNumber = 0; patchNumber < fDomains[domainNumber].numFoodPatches; patchNumber++ )
				{
					if( !fDomains[domainNumber].fFoodPatches[patchNumber].initFoodGrown() && fDomains[domainNumber].fFoodPatches[patchNumber].on( fStep ) )
					{
						for( int j = 0; j < fDomains[domainNumber].fFoodPatches[patchNumber].initFoodCount; j++ )
						{
							if( fDomains[domainNumber].foodCount < fDomains[domainNumber].maxFoodCount )
							{
								fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[patchNumber].addFood();
								fDomains[domainNumber].foodCount++;
							}
						}
						fDomains[domainNumber].fFoodPatches[patchNumber].initFoodGrown( true );
						fDomains[domainNumber].numFoodPatchesGrown++;
					}
				}
			}

			if( fUseProbabilisticFoodPatches )
			{
				if( fDomains[domainNumber].foodCount < fDomains[domainNumber].maxFoodGrownCount )
				{
					float probAdd = (fDomains[domainNumber].maxFoodGrownCount - fDomains[domainNumber].foodCount) * fDomains[domainNumber].foodRate;
					if( drand48() < probAdd )
					{
						// Add food to a patch in the domain (chosen based on the patch's fraction)
						int patchNumber = getRandomPatch( domainNumber );
						if( patchNumber >= 0 )
						{
							fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[patchNumber].addFood();
							fDomains[domainNumber].foodCount++;
						}
					}

					// Grow by the integer part of the foodRate of the domain
					int foodToGrow = (int)fDomains[domainNumber].foodRate; 
					for( int i = 0; i < foodToGrow; i++ )
					{
						int patchNumber = getRandomPatch( domainNumber );
						if( patchNumber >= 0 )
						{
							fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[patchNumber].addFood();
							fDomains[domainNumber].foodCount++;
						}
						else
							break;	// no active patches in this domain, so give up
					}
					
					int newFood = fDomains[domainNumber].minFoodCount - fDomains[domainNumber].foodCount;
					for( int i = 0; i < newFood; i++ )
					{
						int patchNumber = getRandomPatch(domainNumber);
						if( patchNumber >= 0 )
						{
							fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[patchNumber].addFood();
							fDomains[domainNumber].foodCount++;
						}
						else
							break;	// no active patches in this domain, so give up
					}
				}
			}
			else 
			{
				for( int patchNumber = 0; patchNumber < fDomains[domainNumber].numFoodPatches; patchNumber++ )
				{
					if( fDomains[domainNumber].fFoodPatches[patchNumber].on( fStep ) )
					{
						if( fDomains[domainNumber].fFoodPatches[patchNumber].foodCount < fDomains[domainNumber].fFoodPatches[patchNumber].maxFoodGrownCount )
						{

							float probAdd = (fDomains[domainNumber].fFoodPatches[patchNumber].maxFoodGrownCount - fDomains[domainNumber].fFoodPatches[patchNumber].foodCount) * fDomains[domainNumber].fFoodPatches[patchNumber].growthRate;

							if( drand48() < probAdd )
							{
								fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[patchNumber].addFood();
								fDomains[domainNumber].foodCount++;
							}

							// Grow by the integer part of the growthRate
							int foodToGrow = (int) fDomains[domainNumber].fFoodPatches[patchNumber].growthRate; 
							for( int i = 0; i < foodToGrow; i++ )
							{
								fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[patchNumber].addFood();
								fDomains[domainNumber].foodCount++;
							}

							int newFood = fDomains[domainNumber].fFoodPatches[patchNumber].minFoodCount - fDomains[domainNumber].fFoodPatches[patchNumber].foodCount;
							for( int i = 0; i < newFood; i++ )
							{
								fFoodEnergyIn += fDomains[domainNumber].fFoodPatches[patchNumber].addFood();
								fDomains[domainNumber].foodCount++;
							}
						}
					}
				}
			}
		}
	}

	// If any dynamic food patches destroy their food when turned off, take care of it here
	if( fFoodRemovalNeeded )
	{
		// Get a list of patches needing food removal currently
		int numPatchesNeedingRemoval = 0;
		for( int domain = 0; domain < fNumDomains; domain++ )
		{
			for( int patch = 0; patch < fDomains[domain].numFoodPatches; patch++ )
			{
				// printf( "%2ld: d=%d, p=%d, on=%d, on1=%d, r=%d\n", fStep, domain, patch, fDomains[domain].fFoodPatches[patch].on( fStep ), fDomains[domain].fFoodPatches[patch].on( fStep+1 ), fDomains[domain].fFoodPatches[patch].removeFood );
				// If the patch is on now, but off in the next time step, and it is supposed to remove its food when it is off...
				if( fDomains[domain].fFoodPatches[patch].on( fStep ) && !fDomains[domain].fFoodPatches[patch].on( fStep+1 ) && fDomains[domain].fFoodPatches[patch].removeFood )
				{
					fFoodPatchesNeedingRemoval[numPatchesNeedingRemoval++] = &(fDomains[domain].fFoodPatches[patch]);
				}
			}
		}
		
		if( numPatchesNeedingRemoval > 0 )
		{
			food* f;
			
			// There are patches currently needing removal, so do it
			objectxsortedlist::gXSortedObjects.reset();
			while( objectxsortedlist::gXSortedObjects.nextObj( FOODTYPE, (gobject**) &f ) )
			{
				for( int i = 0; i < numPatchesNeedingRemoval; i++ )
				{
					if( f->getPatch() == fFoodPatchesNeedingRemoval[i] )
					{
						// This piece of food is in a patch performing food removal, so get rid of it
						(f->getPatch())->foodCount--;
						fDomains[f->domain()].foodCount--;
						fFoodEnergyOut += f->energy();
						
						objectxsortedlist::gXSortedObjects.removeCurrentObject();   // get it out of the list
						fStage.RemoveObject( f );  // get it out of the world
						delete f;				// get it out of memory
						
						break;	// found patch and deleted food, so get out of patch loop
					}
				}
			}
		}
	}

	if( fFoodPatchStatsFile )
	{
		fprintf( fFoodPatchStatsFile, "%ld: foodPatchCounts =", fStep );
		for( int domainNumber = 0; domainNumber < fNumDomains; domainNumber++ )
		{
			for( int patchNumber = 0; patchNumber < fDomains[domainNumber].numFoodPatches; patchNumber++ )
			{
				fprintf( fFoodPatchStatsFile, " %d", fDomains[domainNumber].fFoodPatches[patchNumber].foodCount );
			}
		}
		fprintf( fFoodPatchStatsFile, "\n");
	}
}


//---------------------------------------------------------------------------
// TSimulation::RecordGeneSeparation
//---------------------------------------------------------------------------
void TSimulation::RecordGeneSeparation()
{
	fprintf(fGeneSeparationFile, "%ld %g %g %g\n",
			fStep,
			fMaxGeneSeparation,
			fMinGeneSeparation,
			fAverageGeneSeparation);
}


//---------------------------------------------------------------------------
// TSimulation::CalculateGeneSeparation
//---------------------------------------------------------------------------
void TSimulation::CalculateGeneSeparation(critter* ci)
{
	// TODO add assert to validate statement below
	
	// NOTE: This version assumes ci is *not* currently in gSortedCritters.
    // It also marks the current position in the list on entry and returns
    // there before exit so it can be invoked during the insertion of the
    // newcritters list into the existing gSortedCritters list.
	
    //objectxsortedlist::gXSortedObjects.mark();
    objectxsortedlist::gXSortedObjects.setMark( CRITTERTYPE );
	
    critter* cj = NULL;
    float genesep;
    float genesepsum = 0.0;
    long numgsvalsold = fNumGeneSepVals;

	objectxsortedlist::gXSortedObjects.reset();
	while (objectxsortedlist::gXSortedObjects.nextObj(CRITTERTYPE, (gobject**)&cj))
    {
		genesep = ci->Genes()->CalcSeparation(cj->Genes());
        fMaxGeneSeparation = fmax(genesep, fMaxGeneSeparation);
        fMinGeneSeparation = fmin(genesep, fMinGeneSeparation);
        genesepsum += genesep;
        fGeneSepVals[fNumGeneSepVals++] = genesep;
    }

    long n = objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE);
    if (numgsvalsold != (n * (n - 1) / 2))
    {
		char tempstring[256];
        sprintf(tempstring,"%s %s %ld %s %ld %s %ld",
				"genesepcalc: numgsvalsold not equal to n * (n - 1) / 2.\n",
				"  numgsvals, n, n * (n - 1) / 2 = ",
				numgsvalsold,", ",n,", ", n * (n - 1) / 2);
		error(2, tempstring);
    }

    if (fNumGeneSepVals != (n * (n + 1) / 2))
    {
		char tempstring[256];
        sprintf(tempstring,"%s %s %ld %s %ld %s %ld",
			 	"genesepcalc: numgsvals not equal to n * (n + 1) / 2.\n",
				"  numgsvals, n, n * (n + 1) / 2 = ",
				fNumGeneSepVals,", ",n,", ",n*(n+1)/2);
        error(2,tempstring);
    }

    fAverageGeneSeparation = (genesepsum + fAverageGeneSeparation * numgsvalsold) / fNumGeneSepVals;
	
    //objectxsortedlist::gXSortedObjects.tomark();
    objectxsortedlist::gXSortedObjects.toMark(CRITTERTYPE);
}



//---------------------------------------------------------------------------
// TSimulation::CalculateGeneSeparationAll
//---------------------------------------------------------------------------
void TSimulation::CalculateGeneSeparationAll()
{
    critter* ci = NULL;
    critter* cj = NULL;

    float genesep;
    float genesepsum = 0.0;
    fMinGeneSeparation = 1.e+10;
    fMaxGeneSeparation = 0.0;
    fNumGeneSepVals = 0;

    objectxsortedlist::gXSortedObjects.reset();
    while (objectxsortedlist::gXSortedObjects.nextObj(CRITTERTYPE, (gobject**)&ci))
    {
		objectxsortedlist::gXSortedObjects.setMark(CRITTERTYPE);

		while (objectxsortedlist::gXSortedObjects.nextObj(CRITTERTYPE, (gobject**)&cj))
		{	
            genesep = ci->Genes()->CalcSeparation(cj->Genes());
            fMaxGeneSeparation = max(genesep, fMaxGeneSeparation);
            fMinGeneSeparation = min(genesep, fMinGeneSeparation);
            genesepsum += genesep;
            fGeneSepVals[fNumGeneSepVals++] = genesep;
        }
		//objectxsortedlist::gXSortedObjects.tomark();
		objectxsortedlist::gXSortedObjects.toMark(CRITTERTYPE);
    }

    // n * (n - 1) / 2 is how many calculations were made
	long n = objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE);
    if (fNumGeneSepVals != (n * (n - 1) / 2))
    {
		char tempstring[256];
        sprintf(tempstring, "%s %s %ld %s %ld %s %ld",
            "genesepcalcall: numgsvals not equal to n * (n - 1) / 2.\n",
            "  numgsvals, n, n * (n - 1) / 2 = ",
            fNumGeneSepVals,", ",n,", ",n * (n - 1) / 2);
        error(2, tempstring);
    }

    fAverageGeneSeparation = genesepsum / fNumGeneSepVals;
}


//---------------------------------------------------------------------------
// TSimulation::SmiteOne
//---------------------------------------------------------------------------
void TSimulation::SmiteOne(short /*id*/, short /*smite*/)
{
/*
    if (id < 0)  // smite one anywhere in the world
    {
        fSortedCritters.next(c);
        smitten = c;
        while (fSortedCritters.next(c))
        {
            if (c->Age() > minsmiteage)
            {
                case (smite)
                1:  // fitness rate
            }
        }
    }
    else  // smite one in domain id
    {
    }
*/
}


//---------------------------------------------------------------------------
// TSimulation::ijfitinc
//---------------------------------------------------------------------------
void TSimulation::ijfitinc(short* i, short* j)
{
    (*j)++;

    if ((*j) == (*i))
        (*j)++;

    if ((*j) > fNumberFit - 1)
    {
        (*j) = 0;
        (*i)++;

        if ((*i) > fNumberFit - 1)
		{
            (*i) = 0;	// min(1, fNumberFit - 1);
			(*j) = 1;
		}
    }
}


//---------------------------------------------------------------------------
// TSimulation::Death
//
// Perform all actions needed to critter before list removal and deletion
//---------------------------------------------------------------------------
void TSimulation::Death(critter* c)
{
	Q_CHECK_PTR(c);
	unsigned long loserIDBestSoFar = 0;	// the way this is used depends on fCritterNumber being 1-based in critter.cp
	unsigned long loserIDBestRecent = 0;	// the way this is used depends on fCritterNumber being 1-based in critter.cp
	bool oneOfTheBestSoFar = false;
	bool oneOfTheBestRecent = false;
	
	const short id = c->Domain();
	
	ttPrint( "age %ld: critter # %ld has died\n", fStep, c->Number() );
	
    fNewDeaths++;
    fNumberDied++;
    fDomains[id].numdied++;
    fDomains[id].numCritters--;
	
	fLifeSpanStats.add( c->Age() );
	fLifeSpanRecentStats.add( c->Age() );
	fLifeFractionRecentStats.add( c->Age() / c->MaxAge() );
	
	// Make any final contributions to the critter's overall, lifetime fitness
    c->lastrewards(fEnergyFitnessParameter, fAgeFitnessParameter); // if any

	FoodPatch* fp;
	
	// If critter's carcass is to become food, make it so here
    if (fCrittersRfood
    	&& ((long)objectxsortedlist::gXSortedObjects.getCount(FOODTYPE) < fMaxFoodCount)
		&& (fDomains[id].foodCount < fDomains[id].maxFoodCount)	// ??? Matt had commented this out; why?
		&& ((fp = fDomains[id].whichFoodPatch( c->x(), c->z() )) && (fp->foodCount < fp->maxFoodCount))	// ??? Matt had nothing like this here; why?
    	&& (globals::edges || ((c->x() >= 0.0) && (c->x() <=  globals::worldsize) &&
    	                       (c->z() <= 0.0) && (c->z() >= -globals::worldsize))) )
    {
        float foodenergy = c->FoodEnergy();
		
        if (foodenergy < fMinFoodEnergyAtDeath)	// it will be bumped up to fMinFoodEnergyAtDeath
        {
            if (fMinFoodEnergyAtDeath >= food::gMinFoodEnergy)			// it will get turned into food
                fFoodEnergyIn += fMinFoodEnergyAtDeath - foodenergy;	// so account for the increase due to the bump (original amount already accounted for)
            else														// else it'll just be disposed of
                fFoodEnergyOut += foodenergy;							// so account for the lost energy

            foodenergy = fMinFoodEnergyAtDeath;
        }
		else	// it will retain its original value (no bump due to the min)
		{
			if( foodenergy < food::gMinFoodEnergy )	// it's going to be disposed of
				fFoodEnergyOut += foodenergy;		// so account for the lost energy
		}

        if (foodenergy >= food::gMinFoodEnergy)
        {
            food* f = new food( foodenergy, c->x(), c->z() );
            Q_CHECK_PTR( f );
			gdlink<gobject*> *saveCurr = objectxsortedlist::gXSortedObjects.getcurr();
			objectxsortedlist::gXSortedObjects.add( f );	// dead critter becomes food
			objectxsortedlist::gXSortedObjects.setcurr( saveCurr );
			fStage.AddObject( f );			// put replacement food into the world
			if( fp )
			{
				fp->foodCount++;
				f->setPatch( fp );
			}
			else
				fprintf( stderr, "food created with no affiliated FoodPatch\n" );
			f->domain( id );
			fDomains[id].foodCount++;
        }
    }
    else
    {
        fFoodEnergyOut += c->FoodEnergy();
    }
	
	// Must call Die() for the critter before any of the uses of Fitness() below, so we get the final, true, post-death fitness
	c->Die();

#define HackForProcessingUnitComplexity 1

#if HackForProcessingUnitComplexity
	if(  fBrainAnatomyRecordAll					||
		 fBestSoFarBrainAnatomyRecordFrequency	||
		 fBestRecentBrainAnatomyRecordFrequency	||
		(fBrainAnatomyRecordSeeds && (c->Number() <= fInitNumCritters))
	  )
		c->Brain()->dumpAnatomical( "run/brain/anatomy", "death", c->Number(), c->Fitness() );
#endif

	if( RecordBrainFunction( c->Number() ) )
	{
		char s[256];
		char t[256];
		sprintf( s, "run/brain/function/incomplete_brainFunction_%ld.txt", c->Number() );
		sprintf( t, "run/brain/function/brainFunction_%ld.txt", c->Number() );
		rename( s, t );

		// Virgil
		if ( fUseComplexityAsFitnessFunc != 'O' )		// Are we using Complexity as a Fitness Function?  If so, set fitness = Complexity here
		{
			c->SetUnusedFitness( c->Fitness() );		// We might want the heuristic fitness later so lets store it somewhere.
		#if UseMaxSpeedAsFitness
			c->SetFitness( 0.01 / (c->MaxSpeed() + 0.01) );
		#else
			if( fUseComplexityAsFitnessFunc == 'D' )	// special case the difference of complexities case
			{
				float pComplexity = CalcComplexity( t, 'P', 0 );
				float iComplexity = CalcComplexity( t, 'I', 0 );
				c->SetFitness( pComplexity - iComplexity );
			}
			else	// otherwise, fUseComplexityAsFitnessFunc has the right letter in it
				c->SetFitness( CalcComplexity( t, fUseComplexityAsFitnessFunc, 0 ) );
		#endif
		}
	}
	
	// Maintain the current-fittest list
	if( (fCurrentFittestCount > 0) && (c->Fitness() >= fCurrentMaxFitness[fCurrentFittestCount-1]) )	// a current-fittest critter is dying
	{
		int haveFitCritter = 0;
		for( int i = 0; i < fCurrentFittestCount; i++ )
		{
			if( fCurrentFittestCritter[i] != c )
				fCurrentFittestCritter[ (i-haveFitCritter) ] = fCurrentFittestCritter[i];
			else
				haveFitCritter = 1;
		}
	
		if( haveFitCritter == 1 )		// this should usually be true, but lets make sure.
		{
			fCurrentFittestCritter[ (fCurrentFittestCount-1) ] = NULL;	// Null out the last critter in the list, just to be polite
			fCurrentFittestCount--;		// decrement the number of critters in the list now that we've removed the recently deceased critter (c) from it.
		}
	}
	
	// Maintain a list of the fittest critters ever, for use in the online/steady-state GA
	// First on a domain-by-domain basis...
    long newfit = 0;
    FitStruct* saveFit;
    if( c->Fitness() > fDomains[id].fittest[fNumberFit-1]->fitness )
    {
        for( short i = 0; i < fNumberFit; i++ )
        {
            if( c->Fitness() > fDomains[id].fittest[i]->fitness )
			{
				newfit = i;
				break;
			}
		}
		
        saveFit = fDomains[id].fittest[fNumberFit-1];				// this is to save the data structure, not its contents
        for( short i = fNumberFit - 1; i > newfit; i-- )
            fDomains[id].fittest[i] = fDomains[id].fittest[i-1];
        fDomains[id].fittest[newfit] = saveFit;						// reuse the old data structure, but replace its contents...
        fDomains[id].fittest[newfit]->fitness = c->Fitness();
        fDomains[id].fittest[newfit]->genes->CopyGenes( c->Genes() );
		fDomains[id].fittest[newfit]->critterID = c->Number();
		gaPrint( "%5ld: new domain %d fittest[%ld] id=%4ld fitness=%7.3f\n", fStep, id, newfit, c->Number(), c->Fitness() );
    }

	// Then on a whole-world basis...
    if( c->Fitness() > fFittest[fNumberFit-1]->fitness )
    {
		oneOfTheBestSoFar = true;
		
        for( short i = 0; i < fNumberFit; i++ )
        {
            if( c->Fitness() > fFittest[i]->fitness )
			{
				newfit = i;
				break;
			}
		}
				
		loserIDBestSoFar = fFittest[fNumberFit - 1]->critterID;	// this is the ID of the critter that is being booted from the bestSoFar (fFittest[]) list
        saveFit = fFittest[fNumberFit - 1];		// this is to save the data structure, not its contents
        for( short i = fNumberFit - 1; i > newfit; i-- )
            fFittest[i] = fFittest[i - 1];
        fFittest[newfit] = saveFit;				// reuse the old data structure, but replace its contents...
        fFittest[newfit]->fitness = c->Fitness();
        fFittest[newfit]->genes->CopyGenes( c->Genes() );
		fFittest[newfit]->critterID = c->Number();
		fFittest[newfit]->Complexity = 0.0;		// must zero out the Complexity so it is recalculated for the new critter
		gaPrint( "%5ld: new global   fittest[%ld] id=%4ld fitness=%7.3f\n", fStep, newfit, c->Number(), c->Fitness() );
    }

    if (c->Fitness() > fMaxFitness)
        fMaxFitness = c->Fitness();
	
	// Keep a separate list of the recent fittest, purely for data-gathering purposes
	// "Recent" means since the last archival recording of recent best, as determined by fBestRecentBrainAnatomyRecordFrequency
	// (Don't bother, if we're not gathering that kind of data)
    if( fBestRecentBrainAnatomyRecordFrequency && (c->Fitness() > fRecentFittest[fNumberRecentFit-1]->fitness) )
    {
		oneOfTheBestRecent = true;
		
        for( short i = 0; i < fNumberRecentFit; i++ )
        {
			// If the critter booted off of the bestSoFar list happens to remain on the bestRecent list,
			// then we don't want to let it be unlinked below, so clear loserIDBestSoFar
			// This loop tests the first part of the fRecentFittest[] list, and the last part is tested
			// below, so we don't need a separate loop to make this determination
			if( loserIDBestSoFar == fRecentFittest[i]->critterID )
				loserIDBestSoFar = 0;
			
            if( c->Fitness() > fRecentFittest[i]->fitness )
			{
				newfit = i;
				break;
			}
		}
				
		loserIDBestRecent = fRecentFittest[fNumberRecentFit - 1]->critterID;	// this is the ID of the critter that is being booted from the bestRecent (fRecentFittest[]) list
        saveFit = fRecentFittest[fNumberRecentFit - 1];		// this is to save the data structure, not its contents
        for( short i = fNumberRecentFit - 1; i > newfit; i-- )
		{
			// If the critter booted off of the bestSoFar list happens to remain on the bestRecent list,
			// then we don't want to let it be unlinked below, so clear loserIDBestSoFar
			// This loop tests the last part of the fRecentFittest[] list, and the first part was tested
			// above, so we don't need a separate loop to make this determination
			if( loserIDBestSoFar == fRecentFittest[i]->critterID )
				loserIDBestSoFar = 0;
			
            fRecentFittest[i] = fRecentFittest[i - 1];
		}
        fRecentFittest[newfit] = saveFit;				// reuse the old data structure, but replace its contents...
        fRecentFittest[newfit]->fitness = c->Fitness();
//		fRecentFittest[newfit]->genes->CopyGenes( c->Genes() );	// we don't save the genes in the bestRecent list
		fRecentFittest[newfit]->critterID = c->Number();
    }

	// Must also update the leastFit data structures, now that they
	// are used on-demand in the main mate/fight/eat loop in Interact()
	// Update the domain-specific leastFit list (only one, as we know which domain it's in)
	for( int i = 0; i < fDomains[id].fNumLeastFit; i++ )
	{
		if( fDomains[id].fLeastFit[i] != c )	// not one of our least fit, so just loop again to keep searching
			continue;

		// one of our least-fit critters died, so pull in the list over it
		smPrint( "removing critter %ld from the least fit list for domain %d at position %d with fitness %g (because it died)\n", c->Number(), id, i, c->Fitness() );
		for( int j = i; j < fDomains[id].fNumLeastFit-1; j++ )
			fDomains[id].fLeastFit[j] = fDomains[id].fLeastFit[j+1];
		fDomains[id].fNumLeastFit--;
		break;	// c can only appear once in the list, so we're done
	}
	
	// Remove critter from world
    fStage.RemoveObject( c );

    // Check and see if we are monitoring this guy
	if (c == fMonitorCritter)
	{
		fMonitorCritter = NULL;

		// Stop monitoring critter brain
		if (fBrainMonitorWindow != NULL)
			fBrainMonitorWindow->StopMonitoring();
	}
	
	// If we're recording all anatomies or recording best anatomies and this was one of the fittest critters,
	// then dump the anatomy to the appropriate location on disk
	if(  fBrainAnatomyRecordAll ||
		(fBestSoFarBrainAnatomyRecordFrequency && oneOfTheBestSoFar) ||
		(fBestRecentBrainAnatomyRecordFrequency && oneOfTheBestRecent) ||
		(fBrainAnatomyRecordSeeds && (c->Number() <= fInitNumCritters))
	  )
	{
	#if ! HackForProcessingUnitComplexity
		c->Brain()->dumpAnatomical( "run/brain/anatomy", "death", c->Number(), c->Fitness() );
	#endif
	}
	else	// don't want brain anatomies for this critter, so must eliminate the "incept" and "birth" anatomies that were already recorded
	{
		char s[256];
	
		sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_incept.txt", c->Number() );
		if( unlink( s ) )
			eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
		sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_birth.txt", c->Number() );
		if( unlink( s ) )
			eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );

	#if HackForProcessingUnitComplexity
		// Turns out we didn't want the "death" anatomy either, but we saved it above for the complexity measure
		if( (fBestSoFarBrainAnatomyRecordFrequency  && !oneOfTheBestSoFar  && (!fBestRecentBrainAnatomyRecordFrequency || !oneOfTheBestRecent))	||
			(fBestRecentBrainAnatomyRecordFrequency	&& !oneOfTheBestRecent && (!fBestSoFarBrainAnatomyRecordFrequency  || !oneOfTheBestSoFar ))
		  )
		{
			sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_death.txt", c->Number() );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
		}
	#endif
	}
		
	// If this was one of the seed critters and we're recording their anatomies, then save the data in the appropriate directory
	if( fBrainAnatomyRecordSeeds && (c->Number() <= fInitNumCritters) )
	{
		char s[256];	// source
		char t[256];	// target
		// Determine whether the original needs to stay around or not.  If so, create a hard link for the
		// copy in "seeds"; if not, just rename (mv) the file into seeds, thus removing it from the original location
		bool keep = ( fBrainAnatomyRecordAll || (fBestSoFarBrainAnatomyRecordFrequency && oneOfTheBestSoFar) || (fBestRecentBrainAnatomyRecordFrequency && oneOfTheBestRecent) );
		
		sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_incept.txt", c->Number() );
		sprintf( t, "run/brain/seeds/anatomy/brainAnatomy_%ld_incept.txt", c->Number() );
		if( keep )
		{
			if( link( s, t ) )
				eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
		}
		else
		{
			if( rename( s, t ) )
				eprintf( "Error (%d) renaming from \"%s\" to \"%s\"\n", errno, s, t );
		}
		sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_birth.txt", c->Number() );
		sprintf( t, "run/brain/seeds/anatomy/brainAnatomy_%ld_birth.txt", c->Number() );
		if( keep )
		{
			if( link( s, t ) )
				eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
		}
		else
		{
			if( rename( s, t ) )
				eprintf( "Error (%d) renaming from \"%s\" to \"%s\"\n", errno, s, t );
		}
		sprintf( s, "run/brain/anatomy/brainAnatomy_%ld_death.txt", c->Number() );
		sprintf( t, "run/brain/seeds/anatomy/brainAnatomy_%ld_death.txt", c->Number() );
		if( keep )
		{
			if( link( s, t ) )
				eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
		}
		else
		{
			if( rename( s, t ) )
				eprintf( "Error (%d) renaming from \"%s\" to \"%s\"\n", errno, s, t );
		}
	}
	
	// If this critter was so good it displaced another critter from the bestSoFar (fFittest[]) list,
	// then nuke the booted critter's brain anatomy & function recordings
	// Note: A critter may be booted from the bestSoFar list, yet remain on the bestRecent list;
	// if this happens, loserIDBestSoFar will be reset to zero above, so we won't execute this block
	// of code and delete files that are needed for the bestRecent data logging
	// In the rare case that a critter is simultaneously booted from both best lists,
	// don't delete it here (so we don't try to delete it more than once)
	if( loserIDBestSoFar && (loserIDBestSoFar != loserIDBestRecent) )	//  depends on fCritterNumber being 1-based in critter.cp
	{
		char s[256];
		if( !fBrainAnatomyRecordAll )
		{
			sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_incept.txt", loserIDBestSoFar );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
			sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_birth.txt", loserIDBestSoFar );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
			sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_death.txt", loserIDBestSoFar );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
		}
		if( !fBrainFunctionRecordAll )
		{
			sprintf( s, "run/brain/function/brainFunction_%lu.txt", loserIDBestSoFar );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
		}
	}

	// If this critter was so good it displaced another critter from the bestRecent (fRecentFittest[]) list,
	// then nuke the booted critter's brain anatomy & function recordings
	// In the rare case that a critter is simultaneously booted from both best lists,
	// we will only delete it here
	if( loserIDBestRecent )	//  depends on fCritterNumber being 1-based in critter.cp
	{
		char s[256];
		if( !fBrainAnatomyRecordAll )
		{
			sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_incept.txt", loserIDBestRecent );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
			sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_birth.txt", loserIDBestRecent );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
			sprintf( s, "run/brain/anatomy/brainAnatomy_%lu_death.txt", loserIDBestRecent );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
		}
		if( !fBrainFunctionRecordAll )
		{
			sprintf( s, "run/brain/function/brainFunction_%lu.txt", loserIDBestRecent );
			if( unlink( s ) )
				eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
		}
	}

	// If this was one of the seed critters and we're recording their functioning, then save the data in the appropriate directory
	// NOTE:  Must be done after c->Die(), as that is where the brainFunction file is closed
	if( fBrainFunctionRecordSeeds && (c->Number() <= fInitNumCritters) )
	{
		char s[256];	// source
		char t[256];	// target
		
		sprintf( s, "run/brain/function/brainFunction_%ld.txt", c->Number() );
		sprintf( t, "run/brain/seeds/function/brainFunction_%ld.txt", c->Number() );
		if( link( s, t ) )
			eprintf( "Error (%d) linking from \"%s\" to \"%s\"\n", errno, s, t );
	}
	
	// If we're only archiving the best, and this isn't one of them, then nuke its brainFunction recording
	if( (fBestSoFarBrainFunctionRecordFrequency || fBestRecentBrainFunctionRecordFrequency) &&
		(!oneOfTheBestSoFar && !oneOfTheBestRecent) &&
		!fBrainFunctionRecordAll )
	{
		char s[256];
		sprintf( s, "run/brain/function/brainFunction_%lu.txt", c->Number() );
		if( unlink( s ) )
			eprintf( "Error (%d) unlinking \"%s\"\n", errno, s );
	}

	if( fRecordBirthsDeaths )		// are we recording births, deaths, and creations?  If so, this critter will be included.
	{
		FILE * File;
		
		if( (File = fopen("run/BirthsDeaths.log", "a")) == NULL )
		{
			cerr << "could not open run/genome/run/BirthsDeaths.log for writing [1].  Exiting." << endl;
			exit(1);
		}
		
		fprintf( File, "%ld DEATH %ld\n", fStep, c->Number() );
		fclose( File );
	}
	
	// following assumes (requires!) list to be currently pointing to c,
    // and will leave the list pointing to the previous critter
	// critter::gXSortedCritters.remove(); // get critter out of the list
	// objectxsortedlist::gXSortedObjects.removeCurrentObject(); // get critter out of the list
	
	// Following assumes (requires!) the critter to have stored c->listLink correctly
	//critter::gXSortedCritters.removeFastUnsafe( c->GetListLink() );
	objectxsortedlist::gXSortedObjects.removeObjectWithLink( (gobject*) c );

	
	// Note: For the sake of computational efficiency, I used to never delete a critter,
	// but "reinit" and reuse them as new critters were born or created.  But Gene made
	// critters be allocated afresh on birth or creation, so we now need to delete the
	// old ones here when they die.  Remove this if I ever get a chance to go back to the
	// more efficient reinit and reuse technique.
	delete c;
}


//-------------------------------------------------------------------------------------------
// TSimulation::ReadWorldFile
//-------------------------------------------------------------------------------------------
void TSimulation::ReadWorldFile(const char* filename)
{
    filebuf fb;
    if (fb.open(filename, ios_base::in) == 0)
    {
        error(0,"unable to open world file \"",filename,"\"");
        return;
    }
	
	istream in(&fb);
    short version;
    char label[128];

    in >> version; in >> label;
	cout << "version" ses version nl;
	
    if( (version < 1) || (version > CurrentWorldfileVersion) )
    {
		if( version <  1 )
		{
			eprintf( "Invalid worldfile version (%d) < 1\n", version );
		}
		else
		{
			eprintf( "Invalid worldfile version (%d) > CurrentWorldfileVersion (%d)\n", version, CurrentWorldfileVersion );
		}
		cout nlf;
     	fb.close();
        exit( 1 );
	}
	
	if( version >= 25 )
	{
		in >> fLockStepWithBirthsDeathsLog; in >> label;
		cout << "LockStepWithBirthsDeathsLog" ses fLockStepWithBirthsDeathsLog nl;
	}
	else
	{
		fLockStepWithBirthsDeathsLog = false;
		cout << "+LockStepWithBirthsDeathsLog" ses fLockStepWithBirthsDeathsLog nl;
	}
	
	if( version >= 18 )
	{
		in >> fMaxSteps; in >> label;
		cout << "maxSteps" ses fMaxSteps nl;
	}
	else
		fMaxSteps = 0;  // don't terminate automatically
	
	bool ignoreBool;
    in >> ignoreBool; in >> label;
    cout << "fDoCPUWork (ignored)" ses ignoreBool nl;

    in >> fDumpFrequency; in >> label;
    cout << "fDumpFrequency" ses fDumpFrequency nl;
    in >> fStatusFrequency; in >> label;
    cout << "fStatusFrequency" ses fStatusFrequency nl;
    in >> globals::edges; in >> label;
    cout << "edges" ses globals::edges nl;

	// TODO figure out how to config critter using this
    in >> globals::wraparound; in >> label;
    cout << "wraparound" ses globals::wraparound nl;

    in >> critter::gVision; in >> label;
    if (!fGraphics)
        critter::gVision = false;
    cout << "vision" ses critter::gVision nl;
    in >> fShowVision; in >> label;
    if (!fGraphics)
        fShowVision = false;
    cout << "showvision" ses fShowVision nl;
    in >> brain::gMinWin; in >> label;
    cout << "minwin" ses brain::gMinWin nl;
    in >> critter::gMaxVelocity; in >> label;
    cout << "maxvel" ses critter::gMaxVelocity nl;
    in >> fMinNumCritters; in >> label;
    cout << "minNumCritters" ses fMinNumCritters nl;
    in >> fMaxCritters; in >> label;
    cout << "maxNumCritters" ses fMaxCritters nl;
    in >> fInitNumCritters; in >> label;
    cout << "initNumCritters" ses fInitNumCritters nl;
	if( version >= 9 )
	{
		in >> fNumberToSeed; in >> label;
		cout << "numberToSeed" ses fNumberToSeed nl;
		in >> fProbabilityOfMutatingSeeds; in >> label;
		cout << "probabilityOfMutatingSeeds" ses fProbabilityOfMutatingSeeds nl;
	}
	else
	{
		fNumberToSeed = 0;
		fProbabilityOfMutatingSeeds = 0.0;
	}
    in >> fMiscCritters; in >> label;
    cout << "misccritters" ses fMiscCritters nl;
    in >> fPositionSeed; in >> label;
    cout << "fPositionSeed" ses fPositionSeed nl;
    in >> fGenomeSeed; in >> label;
    cout << "genomeseed" ses fGenomeSeed nl;
    in >> fMinFoodCount; in >> label;
    cout << "minFoodCount" ses fMinFoodCount nl;
    in >> fMaxFoodCount; in >> label;
    cout << "maxFoodCount" ses fMaxFoodCount nl;
    in >> fMaxFoodGrown; in >> label;
    cout << "maxFoodGrown" ses fMaxFoodGrown nl;
    in >> fInitialFoodCount; in >> label;
    cout << "initFoodCount" ses fInitialFoodCount nl;
    in >> fFoodRate; in >> label;
    cout << "foodrate" ses fFoodRate nl;
    in >> fCrittersRfood; in >> label;
    cout << "crittersRfood" ses fCrittersRfood nl;
    in >> fFitness1Frequency; in >> label;
    cout << "fit1freq" ses fFitness1Frequency nl;
    in >> fFitness2Frequency; in >> label;
    cout << "fit2freq" ses fFitness2Frequency nl;
    in >> fNumberFit; in >> label;
    cout << "numfit" ses fNumberFit nl;
	
	if( version >= 14 )
	{
		in >> fNumberRecentFit; in >> label;
		cout << "numRecentFit" ses fNumberRecentFit nl;
	}
	
    in >> fEatFitnessParameter; in >> label;
    cout << "eatfitparam" ses fEatFitnessParameter nl;
    in >> fMateFitnessParameter; in >> label;
    cout << "matefitparam" ses fMateFitnessParameter nl;
    in >> fMoveFitnessParameter; in >> label;
    cout << "movefitparam" ses fMoveFitnessParameter nl;
    in >> fEnergyFitnessParameter; in >> label;
    cout << "energyfitparam" ses fEnergyFitnessParameter nl;
    in >> fAgeFitnessParameter; in >> label;
    cout << "agefitparam" ses fAgeFitnessParameter nl;
  	fTotalFitness = fEatFitnessParameter + fMateFitnessParameter + fMoveFitnessParameter + fEnergyFitnessParameter + fAgeFitnessParameter;
    in >> food::gMinFoodEnergy; in >> label;
    cout << "minfoodenergy" ses food::gMinFoodEnergy nl;
    in >> food::gMaxFoodEnergy; in >> label;
    cout << "maxfoodenergy" ses food::gMaxFoodEnergy nl;
    in >> food::gSize2Energy; in >> label;
    cout << "size2energy" ses food::gSize2Energy nl;
    in >> fEat2Consume; in >> label;
    cout << "eat2consume" ses fEat2Consume nl;

    int ignoreShort1, ignoreShort2, ignoreShort3, ignoreShort4;
    in >> ignoreShort1; in >> label;
    cout << "minintneurons" ses ignoreShort1 nl;
    in >> ignoreShort2; in >> label;
    cout << "maxintneurons" ses ignoreShort2 nl;
    // note: minintneurons & maxintneurons are no longer used
    cout << "note: minintneurons & maxintneurons are no longer used" nl;

    in >> genome::gMinvispixels; in >> label;
    cout << "minvispixels" ses genome::gMinvispixels nl;
    in >> genome::gMaxvispixels; in >> label;
    cout << "maxvispixels" ses genome::gMaxvispixels nl;
    in >> genome::gMinMutationRate; in >> label;
    cout << "minmrate" ses genome::gMinMutationRate nl;
    in >> genome::gMaxMutationRate; in >> label;
    cout << "maxmrate" ses genome::gMaxMutationRate nl;
    in >> genome::gMinNumCpts; in >> label;
    cout << "minNumCpts" ses genome::gMinNumCpts nl;
    in >> genome::gMaxNumCpts; in >> label;
    cout << "maxNumCpts" ses genome::gMaxNumCpts nl;
    in >> genome::gMinLifeSpan; in >> label;
    cout << "minlifespan" ses genome::gMinLifeSpan nl;
    in >> genome::gMaxLifeSpan; in >> label;
    cout << "maxlifespan" ses genome::gMaxLifeSpan nl;
    in >> fMateWait; in >> label;
    cout << "matewait" ses fMateWait nl;
    in >> critter::gInitMateWait; in >> label;
    cout << "initmatewait" ses critter::gInitMateWait nl;
	if( version >= 29 )
	{
		in >> fEatMateSpan; in >> label;
		cout << "EatMateSpan" ses fEatMateSpan nl;
	}
	else
	{
		fEatMateSpan = 0;
		cout << "+EatMateSpan" ses fEatMateSpan nl;
	}
    in >> genome::gMinStrength; in >> label;
    cout << "minstrength" ses genome::gMinStrength nl;
    in >> genome::gMaxStrength; in >> label;
    cout << "maxstrength" ses genome::gMaxStrength nl;
    in >> critter::gMinCritterSize; in >> label;
    cout << "mincsize" ses critter::gMinCritterSize nl;
    in >> critter::gMaxCritterSize; in >> label;
    cout << "maxcsize" ses critter::gMaxCritterSize nl;
    in >> critter::gMinMaxEnergy; in >> label;
    cout << "minmaxenergy" ses critter::gMinMaxEnergy nl;
    in >> critter::gMaxMaxEnergy; in >> label;
    cout << "maxmaxenergy" ses critter::gMaxMaxEnergy nl;
    in >> genome::gMinmateenergy; in >> label;
    cout << "minmateenergy" ses genome::gMinmateenergy nl;
    in >> genome::gMaxmateenergy; in >> label;
    cout << "maxmateenergy" ses genome::gMaxmateenergy nl;
    in >> fMinMateFraction; in >> label;
    cout << "minmatefrac" ses fMinMateFraction nl;
    in >> genome::gMinmaxspeed; in >> label;
    cout << "minmaxspeed" ses genome::gMinmaxspeed nl;
    in >> genome::gMaxmaxspeed; in >> label;
    cout << "maxmaxspeed" ses genome::gMaxmaxspeed nl;
    in >> critter::gSpeed2DPosition; in >> label;
    cout << "speed2dpos" ses critter::gSpeed2DPosition nl;
    in >> critter::gYaw2DYaw; in >> label;
    cout << "yaw2dyaw" ses critter::gYaw2DYaw nl;
    in >> genome::gMinlrate; in >> label;
    cout << "minlrate" ses genome::gMinlrate nl;
    in >> genome::gMaxlrate; in >> label;
    cout << "maxlrate" ses genome::gMaxlrate nl;
    in >> critter::gMinFocus; in >> label;
    cout << "minfocus" ses critter::gMinFocus nl;
    in >> critter::gMaxFocus; in >> label;
    cout << "maxfocus" ses critter::gMaxFocus nl;
    in >> critter::gCritterFOV; in >> label;
    cout << "critterfovy" ses critter::gCritterFOV nl;
    in >> critter::gMaxSizeAdvantage; in >> label;
    cout << "maxsizeadvantage" ses critter::gMaxSizeAdvantage nl;
    in >> fPower2Energy; in >> label;
    cout << "power2energy" ses fPower2Energy nl;
    in >> critter::gEat2Energy; in >> label;
    cout << "eat2energy" ses critter::gEat2Energy nl;
    in >> critter::gMate2Energy; in >> label;
    cout << "mate2energy" ses critter::gMate2Energy nl;
    in >> critter::gFight2Energy; in >> label;
    cout << "fight2energy" ses critter::gFight2Energy nl;
    in >> critter::gMaxSizePenalty; in >> label;
    cout << "maxsizepenalty" ses critter::gMaxSizePenalty nl;
    in >> critter::gSpeed2Energy; in >> label;
    cout << "speed2energy" ses critter::gSpeed2Energy nl;
    in >> critter::gYaw2Energy; in >> label;
    cout << "yaw2energy" ses critter::gYaw2Energy nl;
    in >> critter::gLight2Energy; in >> label;
    cout << "light2energy" ses critter::gLight2Energy nl;
    in >> critter::gFocus2Energy; in >> label;
    cout << "focus2energy" ses critter::gFocus2Energy nl;
    in >> brain::gNeuralValues.maxsynapse2energy; in >> label;
    cout << "maxsynapse2energy" ses brain::gNeuralValues.maxsynapse2energy nl;
    in >> critter::gFixedEnergyDrain; in >> label;
    cout << "fixedenergydrain" ses critter::gFixedEnergyDrain nl;
    in >> brain::gDecayRate; in >> label;
    cout << "decayrate" ses brain::gDecayRate nl;

	if( version >= 18 )
	{
		in >> fCritterHealingRate; in >> label;
		
		if( fCritterHealingRate > 0.0 )
			fHealing = 1;					// a bool flag to check to see if healing is turned on is faster than always comparing a float.
		cout << "CritterHealingRate" ses fCritterHealingRate nl;
	}
	
    in >> fEatThreshold; in >> label;
    cout << "eatthreshold" ses fEatThreshold nl;
    in >> fMateThreshold; in >> label;
    cout << "matethreshold" ses fMateThreshold nl;
    in >> fFightThreshold; in >> label;
    cout << "fightthreshold" ses fFightThreshold nl;
    in >> genome::gMiscBias; in >> label;
    cout << "miscbias" ses genome::gMiscBias nl;
    in >> genome::gMiscInvisSlope; in >> label;
    cout << "miscinvslope" ses genome::gMiscInvisSlope nl;
    in >> brain::gLogisticsSlope; in >> label;
    cout << "logisticslope" ses brain::gLogisticsSlope nl;
    in >> brain::gMaxWeight; in >> label;
    cout << "maxweight" ses brain::gMaxWeight nl;
    in >> brain::gInitMaxWeight; in >> label;
    cout << "initmaxweight" ses brain::gInitMaxWeight nl;
    in >> genome::gMinBitProb; in >> label;
    cout << "minbitprob" ses genome::gMinBitProb nl;
    in >> genome::gMaxBitProb; in >> label;
    cout << "maxbitprob" ses genome::gMaxBitProb nl;
    in >> critter::gCritterHeight; in >> label;
    cout << "critterheight" ses critter::gCritterHeight nl;
    in >> food::gFoodHeight; in >> label;
    cout << "foodheight" ses food::gFoodHeight nl;
    in >> food::gFoodColor.r >> food::gFoodColor.g >> food::gFoodColor.b >> label;
    cout << "foodcolor = (" << food::gFoodColor.r cms
                               food::gFoodColor.g cms
                               food::gFoodColor.b pnl;
    in >> barrier::gBarrierHeight; in >> label;
    cout << "barrierheight" ses barrier::gBarrierHeight nl;
    in >> barrier::gBarrierColor.r >> barrier::gBarrierColor.g >> barrier::gBarrierColor.b >> label;
    cout << "barriercolor = (" << barrier::gBarrierColor.r cms
                                  barrier::gBarrierColor.g cms
                                  barrier::gBarrierColor.b pnl;
    in >> fGroundColor.r >> fGroundColor.g >> fGroundColor.b >> label;
    cout << "groundcolor = (" << fGroundColor.r cms
                                 fGroundColor.g cms
                                 fGroundColor.b pnl;
    in >> fGroundClearance; in >> label;
    cout << "fGroundClearance" ses fGroundClearance nl;
    in >> fCameraColor.r >> fCameraColor.g >> fCameraColor.b >> label;
    cout << "cameracolor = (" << fCameraColor.r cms
                                 fCameraColor.g cms
                                 fCameraColor.b pnl;
    in >> fCameraRadius; in >> label;
    cout << "camradius" ses fCameraRadius nl;
    in >> fCameraHeight; in >> label;
    cout << "camheight" ses fCameraHeight nl;
    in >> fCameraRotationRate; in >> label;
    cout << "camrotationrate" ses fCameraRotationRate nl;
    fRotateWorld = (fCameraRotationRate != 0.0);	//Boolean for enabling or disabling world roation (CMB 3/19/06)
    in >> fCameraAngleStart; in >> label;
    cout << "camanglestart" ses fCameraAngleStart nl;
    in >> fCameraFOV; in >> label;
    cout << "camfov" ses fCameraFOV nl;
    in >> fMonitorCritterRank; in >> label;
    if (!fGraphics)
        fMonitorCritterRank = 0; // cannot monitor critter brain without graphics
    cout << "fMonitorCritterRank" ses fMonitorCritterRank nl;

    in >> ignoreShort3; in >> label;
    cout << "fMonitorCritWinWidth (ignored)" ses ignoreShort3 nl;
    in >> ignoreShort4; in >> label;
    cout << "fMonitorCritWinHeight (ignored)" ses ignoreShort4 nl;

    in >> fBrainMonitorStride; in >> label;
    cout << "fBrainMonitorStride" ses fBrainMonitorStride nl;
    in >> globals::worldsize; in >> label;
    cout << "worldsize" ses globals::worldsize nl;
    long numbarriers;
    float x1, z1, x2, z2;
    in >> numbarriers; in >> label;
    cout << "numbarriers = " << numbarriers nl;
	if( version >= 27 )
	{
		for( int i = 0; i < numbarriers; i++ )
		{
			int numKeyFrames;
			in >> numKeyFrames >> label;
			cout << "barrier[" << i << "].numKeyFrames" ses numKeyFrames nl;
			barrier* b = new barrier( numKeyFrames );
			for( int j = 0; j < numKeyFrames; j++ )
			{
				long t;
				in >> t >> x1 >> z1 >> x2 >> z2 >> label;
				cout << "barrier[" << i << "].keyFrame[" << j << "]" ses t sp x1 sp z1 sp x2 sp z2 nl;
				b->setKeyFrame( t, x1, z1, x2, z2 );
			}
			barrier::gXSortedBarriers.add( b );
		}
	}
	else
	{
		for( int i = 0; i < numbarriers; i++ )
		{
			in >> x1 >> z1 >> x2 >> z2 >> label;
			cout << "barrier #" << i << " position = ("
				 << x1 cms z1 << ") to (" << x2 cms z2 pnl;
			barrier* b = new barrier( 1 );
			b->setKeyFrame( 0, x1, z1, x2, z2 );
			barrier::gXSortedBarriers.add( b );
		}
	}
	
    if (version < 2)
    {
		cout nlf;
     	fb.close();
        return;
	}

    in >> fMonitorGeneSeparation; in >> label;
    cout << "genesepmon" ses fMonitorGeneSeparation nl;
    in >> fRecordGeneSeparation; in >> label;
    cout << "geneseprec" ses fRecordGeneSeparation nl;

    if (version < 3)
    {
		cout nlf;
     	fb.close();
        return;
	}

    in >> fChartBorn; in >> label;
    cout << "fChartBorn" ses fChartBorn nl;
    in >> fChartFitness; in >> label;
    cout << "fChartFitness" ses fChartFitness nl;
    in >> fChartFoodEnergy; in >> label;
    cout << "fChartFoodEnergy" ses fChartFoodEnergy nl;
    in >> fChartPopulation; in >> label;
    cout << "fChartPopulation" ses fChartPopulation nl;

    if (version < 4)
    {
		cout nlf;
     	fb.close();
        return;
	}

    in >> fNumDomains; in >> label;
    cout << "numdomains" ses fNumDomains nl;
    if (fNumDomains < 1)
    {
		char tempstring[256];
        sprintf(tempstring,"%s %ld %s", "Number of fitness domains must always be > 0 (not ", fNumDomains,").\nIt will be reset to 1.");
        error(1, tempstring);
        fNumDomains = 1;
    }
    if (fNumDomains > MAXDOMAINS)
    {
		char tempstring[256];
        sprintf(tempstring, "Too many fitness domains requested (%ld > %d)", fNumDomains, MAXDOMAINS);
        error(2,tempstring);
    }

    short id;
	long totmaxnumcritters = 0;
	long totminnumcritters = 0;
	int	numFoodPatchesNeedingRemoval = 0;
	
	for (id = 0; id < fNumDomains; id++)
	{
		in >> fDomains[id].minNumCritters; in >> label;
		cout << "minNumCritters in domains[" << id << "]" ses fDomains[id].minNumCritters nl;
		in >> fDomains[id].maxNumCritters; in >> label;
		cout << "maxNumCritters in domains[" << id << "]" ses fDomains[id].maxNumCritters nl;
		in >> fDomains[id].initNumCritters; in >> label;
		cout << "initNumCritters in domains[" << id << "]" ses fDomains[id].initNumCritters nl;
		if( version >= 9 )
		{
			in >> fDomains[id].numberToSeed; in >> label;
			cout << "numberToSeed in domains[" << id << "]" ses fDomains[id].numberToSeed nl;
			in >> fDomains[id].probabilityOfMutatingSeeds; in >> label;
			cout << "probabilityOfMutatingSeeds in domains [" << id << "]" ses fDomains[id].probabilityOfMutatingSeeds nl;
		}
		else
		{
			fDomains[id].numberToSeed = 0;
			fDomains[id].probabilityOfMutatingSeeds = 0.0;
		}
		if (version >= 19)
		{
			in >> fDomains[id].centerX; in >> label;
			cout << "centerX of domains[" << id << "]" ses fDomains[id].centerX nl;
			in >> fDomains[id].centerZ; in >> label;
			cout << "centerZ of domains[" << id << "]" ses fDomains[id].centerZ nl;
			in >> fDomains[id].sizeX; in >> label;
			cout << "sizeX of domains[" << id << "]" ses fDomains[id].sizeX nl;
			in >> fDomains[id].sizeZ; in >> label;
			cout << "sizeZ of domains[" << id << "]" ses fDomains[id].sizeZ nl;

			fDomains[id].absoluteSizeX = globals::worldsize * fDomains[id].sizeX;
			fDomains[id].absoluteSizeZ = globals::worldsize * fDomains[id].sizeZ;

			fDomains[id].startX = fDomains[id].centerX * globals::worldsize - (fDomains[id].absoluteSizeX / 2.0);
			fDomains[id].startZ = - fDomains[id].centerZ * globals::worldsize - (fDomains[id].absoluteSizeZ / 2.0);

			fDomains[id].endX = fDomains[id].centerX * globals::worldsize + (fDomains[id].absoluteSizeX / 2.0);
			fDomains[id].endZ = - fDomains[id].centerZ * globals::worldsize + (fDomains[id].absoluteSizeZ / 2.0);
			
			// clean up for floating point precision a little
			if( fDomains[id].startX < 0.0006 )
				fDomains[id].startX = 0.0;
			if( fDomains[id].startZ > -0.0006 )
				fDomains[id].startZ = 0.0;
			if( fDomains[id].endX > globals::worldsize * 0.9994 )
				fDomains[id].endX = globals::worldsize;
			if( fDomains[id].endZ < -globals::worldsize * 0.9994 )
				fDomains[id].endZ = -globals::worldsize;

			printf("NEW DOMAIN\n");
			printf("\tstartX: %.2f\n", fDomains[id].startX);
			printf("\tstartZ: %.2f\n", fDomains[id].startZ);
			printf("\tendX: %.2f\n", fDomains[id].endX);
			printf("\tendZ: %.2f\n", fDomains[id].endZ);
			printf("\tabsoluteX: %.2f\n", fDomains[id].absoluteSizeX);
			printf("\tabsoluteZ: %.2f\n", fDomains[id].absoluteSizeZ);

			in >> fDomains[id].numFoodPatches; in >> label;
			cout << "numFoodPatches of domains[" << id << "]" ses fDomains[id].numFoodPatches nl;
			in >> fDomains[id].numBrickPatches; in >> label;
			cout << "numBrickPatches of domains[" << id << "]" ses fDomains[id].numBrickPatches nl;
			in >> fDomains[id].foodRate; in >> label;
			cout << "foodRate of domains[" << id << "]" ses fDomains[id].foodRate nl;
			in >> fDomains[id].initFoodCount; in >> label;
			cout << "initFoodCount of domains[" << id << "]" ses fDomains[id].initFoodCount nl;
            in >> fDomains[id].minFoodCount; in >> label;
			cout << "minFoodCount of domains[" << id << "]" ses fDomains[id].minFoodCount nl;
            in >> fDomains[id].maxFoodCount; in >> label;
			cout << "maxFoodCount of domains[" << id << "]" ses fDomains[id].maxFoodCount nl;
			in >> fDomains[id].maxFoodGrownCount; in >> label;
			cout << "maxFoodGrownCount of domains[" << id << "]" ses fDomains[id].maxFoodGrownCount nl;
			
			// Create an array of FoodPatches
			fDomains[id].fFoodPatches = new FoodPatch[fDomains[id].numFoodPatches];
			float patchFractionSpecified = 0.0;

			for (int i = 0; i < fDomains[id].numFoodPatches; i++)
			{
				float tmpFraction, tmpRate;
				float centerX, centerZ;  // should be from 0.0 -> 1.0
				float sizeX, sizeZ;  // should be from 0.0 -> 1.0
				float nhsize;
				int initFood, minFood, maxFood, maxFoodGrown;
				int shape, distribution;
				int period;
				float onFraction, phase;
				bool removeFood;

				in >> tmpFraction; in >> label;
				cout << "tmpFraction" ses tmpFraction nl;
				in >> tmpRate; in >> label;
				cout << "tmpRate" ses tmpRate nl;
				in >> initFood; in >> label;
				cout << "initFood" ses initFood nl;
				in >> minFood; in >> label;
				cout << "minFood" ses minFood nl;
				in >> maxFood; in >> label;
				cout << "maxFood" ses maxFood nl;
				in >> maxFoodGrown; in >> label;
				cout << "maxFoodGrown" ses maxFoodGrown nl;
				in >> centerX; in >> label;
				cout << "centerX" ses centerX nl;
				in >> centerZ; in >> label;
				cout << "centerZ" ses centerZ nl;
				in >> sizeX; in >> label;
				cout << "sizeX" ses sizeX nl;
				in >> sizeZ; in >> label;
				cout << "sizeZ" ses sizeZ nl;
				in >> shape; in >> label;
				cout << "shape" ses shape nl;
				in >> distribution; in >> label;
				cout << "distribution" ses distribution nl;
				in >> nhsize; in >> label;
				cout << "neighborhoodSize" ses nhsize nl;
				if( version >= 26 )
				{
					in >> period; in >> label;
					cout << "period" ses period nl;
					in >> onFraction; in >> label;
					cout << "onFraction" ses onFraction nl;
					in >> phase; in >> label;
					cout << "phase" ses phase nl;
					in >> removeFood; in >> label;
					cout << "removeFood" ses removeFood nl;
					if( removeFood )
						numFoodPatchesNeedingRemoval++;
				}
				else
				{
					period = 0;
					cout << "+period" ses period nl;
					onFraction = 1.0;
					cout << "+onFraction" ses onFraction nl;
					phase = 0.0;
					cout << "+phase" ses phase nl;
					removeFood = false;
					cout << "+removeFood" ses removeFood nl;
				}
				
				// If we have a legitimate fraction specified, see if we need to set the food limits
				if( tmpFraction > 0.0 )
				{
					// Check for negative values...if found then use global food counts and this patch's fraction
					if (initFood < 0) initFood = (int)(tmpFraction * fDomains[id].initFoodCount);
					if (minFood < 0) minFood = (int)(tmpFraction * fDomains[id].minFoodCount);
					if (maxFood < 0) maxFood = (int)(tmpFraction * fDomains[id].maxFoodCount);
					if (maxFoodGrown < 0) maxFoodGrown = (int)(tmpFraction * fDomains[id].maxFoodGrownCount);
				}

				// Check if rate is 0.0...if so, then set this patch's rate to the domain's rate
				if (tmpRate == 0.0)
					tmpRate = fDomains[id].foodRate;

				fDomains[id].fFoodPatches[i].init(centerX, centerZ, sizeX, sizeZ, tmpRate, initFood, minFood, maxFood, maxFoodGrown, tmpFraction, shape, distribution, nhsize, period, onFraction, phase, removeFood, &fStage, &(fDomains[id]), id);

				patchFractionSpecified += tmpFraction;

			}

			// If all the fractions are 0.0, then set fractions based on patch area
			if (patchFractionSpecified == 0.0)
			{
				float totalArea = 0.0;
				// First calculate the total area from all the patches
				for (int i=0; i<fDomains[id].numFoodPatches; i++)
				{
					totalArea += fDomains[id].fFoodPatches[i].getArea();
				}
				// Then calculate the fraction for each patch
				for (int i=0; i<fDomains[id].numFoodPatches; i++)
				{
					float newFraction = fDomains[id].fFoodPatches[i].getArea() / totalArea;
					fDomains[id].fFoodPatches[i].setInitCounts((int)(newFraction * fDomains[id].initFoodCount), (int)(newFraction * fDomains[id].minFoodCount), (int)(newFraction * fDomains[id].maxFoodCount), (int)(newFraction * fDomains[id].maxFoodGrownCount), newFraction);
					patchFractionSpecified += newFraction;
				}
			}
			
			// Make sure fractions add up to 1.0 (allowing a little slop for floating point precision)
			if( (patchFractionSpecified < 0.99999) || (patchFractionSpecified > 1.00001) )
			{
				printf( "Patch Fractions sum to %f\n", patchFractionSpecified );
				printf( "FoodPatch fractions do not sum to 1!\n" );
				exit( 1 );
			}

			// Create an array of BrickPatches
			fDomains[id].fBrickPatches = new BrickPatch[fDomains[id].numBrickPatches];
			for (int i = 0; i < fDomains[id].numBrickPatches; i++)
			{
				float centerX, centerZ;  // should be from 0.0 -> 1.0
				float sizeX, sizeZ;  // should be from 0.0 -> 1.0
				float nhsize;
				int shape, distribution, brickCount;

				in >> centerX; in >> label;
				cout << "centerX" ses centerX nl;
				in >> centerZ; in >> label;
				cout << "centerZ" ses centerZ nl;
				in >> sizeX; in >> label;
				cout << "sizeX" ses sizeX nl;
				in >> sizeZ; in >> label;
				cout << "sizeZ" ses sizeZ nl;
				in >> brickCount; in >> label;
				cout << "brickCount" ses brickCount nl;
				in >> shape; in >> label;
				cout << "shape" ses shape nl;
				in >> distribution; in >> label;
				cout << "distribution" ses distribution nl;
				in >> nhsize; in >> label;
				cout << "neighborhoodSize" ses nhsize nl;

				fDomains[id].fBrickPatches[i].init(centerX, centerZ, sizeX, sizeZ, brickCount, shape, distribution, nhsize, &fStage, &(fDomains[id]), id);
			}

		}

		totmaxnumcritters += fDomains[id].maxNumCritters;
		totminnumcritters += fDomains[id].minNumCritters;
	}


	if (totmaxnumcritters > fMaxCritters)
	{
		char tempstring[256];
		sprintf(tempstring,"%s %ld %s %ld %s",
			"The maximum number of organisms in the world (",
			fMaxCritters,
			") is < the maximum summed over domains (",
			totmaxnumcritters,
			"), so there may still be some indirect global influences.");
		errorflash(0, tempstring);
	}
	if (totminnumcritters < fMinNumCritters)
	{
		char tempstring[256];
		sprintf(tempstring,"%s %ld %s %ld %s",
			"The minimum number of organisms in the world (",
			fMinNumCritters,
			") is > the minimum summed over domains (",
			totminnumcritters,
			"), so there may still be some indirect global influences.");
		errorflash(0,tempstring);
	}

	if( numFoodPatchesNeedingRemoval > 0 )
	{
		// Allocate a maximally sized array to keep a list of food patches needing their food removed
		fFoodPatchesNeedingRemoval = new FoodPatch*[numFoodPatchesNeedingRemoval];
		fFoodRemovalNeeded = true;
	}
	else
		fFoodRemovalNeeded = false;
	
    for (id = 0; id < fNumDomains; id++)
    {
        fDomains[id].numCritters = 0;
        fDomains[id].numcreated = 0;
        fDomains[id].numborn = 0;
        fDomains[id].numbornsincecreated = 0;
        fDomains[id].numdied = 0;
        fDomains[id].lastcreate = 0;
        fDomains[id].maxgapcreate = 0;
        fDomains[id].ifit = 0;
        fDomains[id].jfit = 1;
        fDomains[id].fittest = NULL;
	}
	
	if( version >= 17 )
	{
		in >> fUseProbabilisticFoodPatches; in >> label;
		cout << "useProbabilisticFoodPatches" ses fUseProbabilisticFoodPatches nl;
	}
	else
		fUseProbabilisticFoodPatches = false;

    if (version < 5)
    {
		cout nlf;
     	fb.close();
        return;
	}

    in >> fChartGeneSeparation; in >> label;
    cout << "fChartGeneSeparation" ses fChartGeneSeparation nl;
    if (fChartGeneSeparation)
        fMonitorGeneSeparation = true;

    if (version < 6)
	{
		cout nlf;
     	fb.close();
        return;
	}

    in >> brain::gNeuralValues.mininternalneurgroups; in >> label;
    cout << "mininternalneurgroups" ses brain::gNeuralValues.mininternalneurgroups nl;
    in >> brain::gNeuralValues.maxinternalneurgroups; in >> label;
    cout << "maxinternalneurgroups" ses brain::gNeuralValues.maxinternalneurgroups nl;
    in >> brain::gNeuralValues.mineneurpergroup; in >> label;
    cout << "mineneurpergroup" ses brain::gNeuralValues.mineneurpergroup nl;
    in >> brain::gNeuralValues.maxeneurpergroup; in >> label;
    cout << "maxeneurpergroup" ses brain::gNeuralValues.maxeneurpergroup nl;
    in >> brain::gNeuralValues.minineurpergroup; in >> label;
    cout << "minineurpergroup" ses brain::gNeuralValues.minineurpergroup nl;
    in >> brain::gNeuralValues.maxineurpergroup; in >> label;
    cout << "maxineurpergroup" ses brain::gNeuralValues.maxineurpergroup nl;
	float unusedFloat;
    in >> unusedFloat; in >> label;
    cout << "minbias (not used)" ses unusedFloat nl;
    in >> brain::gNeuralValues.maxbias; in >> label;
    cout << "maxbias" ses brain::gNeuralValues.maxbias nl;
    in >> brain::gNeuralValues.minbiaslrate; in >> label;
    cout << "minbiaslrate" ses brain::gNeuralValues.minbiaslrate nl;
    in >> brain::gNeuralValues.maxbiaslrate; in >> label;
    cout << "maxbiaslrate" ses brain::gNeuralValues.maxbiaslrate nl;
    in >> brain::gNeuralValues.minconnectiondensity; in >> label;
    cout << "minconnectiondensity" ses brain::gNeuralValues.minconnectiondensity nl;
    in >> brain::gNeuralValues.maxconnectiondensity; in >> label;
    cout << "maxconnectiondensity" ses brain::gNeuralValues.maxconnectiondensity nl;
    in >> brain::gNeuralValues.mintopologicaldistortion; in >> label;
    cout << "mintopologicaldistortion" ses brain::gNeuralValues.mintopologicaldistortion nl;
    in >> brain::gNeuralValues.maxtopologicaldistortion; in >> label;
    cout << "maxtopologicaldistortion" ses brain::gNeuralValues.maxtopologicaldistortion nl;
    in >> brain::gNeuralValues.maxneuron2energy; in >> label;
    cout << "maxneuron2energy" ses brain::gNeuralValues.maxneuron2energy nl;

    in >> brain::gNumPrebirthCycles; in >> label;
    cout << "numprebirthcycles" ses brain::gNumPrebirthCycles nl;

    InitNeuralValues();

    cout << "maxneurgroups" ses brain::gNeuralValues.maxneurgroups nl;
    cout << "maxneurons" ses brain::gNeuralValues.maxneurons nl;
    cout << "maxsynapses" ses brain::gNeuralValues.maxsynapses nl;

    in >> fOverHeadRank; in >> label;
    cout << "fOverHeadRank" ses fOverHeadRank nl;
    in >> fCritterTracking; in >> label;
    cout << "fCritterTracking" ses fCritterTracking nl;
    in >> fMinFoodEnergyAtDeath; in >> label;
    cout << "minfoodenergyatdeath" ses fMinFoodEnergyAtDeath nl;

    in >> genome::gGrayCoding; in >> label;
    cout << "graycoding" ses genome::gGrayCoding nl;

    if (version < 7)
    {
		cout nlf;
		fb.close();
		return;
	}
	if( version >= 21 )
	{
		in >> fSmiteMode; in >> label;
		cout << "smiteMode" ses fSmiteMode nl;
		assert( fSmiteMode == 'L' || fSmiteMode == 'R' || fSmiteMode == 'O' );		// fSmiteMode must be 'L', 'R', or 'O'
	}
    in >> fSmiteFrac; in >> label;
	
    cout << "smiteFrac" ses fSmiteFrac nl;
    in >> fSmiteAgeFrac; in >> label;
    cout << "smiteAgeFrac" ses fSmiteAgeFrac nl;
	
	// critter::gNumDepletionSteps and critter:gMaxPopulationFraction must both be initialized to zero
	// for backward compatibility, and we depend on that fact here, so don't change it
	if( version >= 23 )
	{
		in >> critter::gNumDepletionSteps; in >> label;
		cout << "NumDepletionSteps" ses critter::gNumDepletionSteps nl;
		if( critter::gNumDepletionSteps )
			critter::gMaxPopulationPenaltyFraction = 1.0 / (float) critter::gNumDepletionSteps;
		cout << ".MaxPopulationPenaltyFraction" ses critter::gMaxPopulationPenaltyFraction nl;
	}
	else
	{
		cout << "+NumDepletionSteps" ses critter::gNumDepletionSteps nl;
		cout << ".MaxPopulationPenaltyFraction" ses critter::gMaxPopulationPenaltyFraction nl;
	}
	
	if( version >= 24 )
	{
		in >> fApplyLowPopulationAdvantage; in >> label;
		cout << "ApplyLowPopulationAdvantage" ses fApplyLowPopulationAdvantage nl;
	}
	else
	{
		fApplyLowPopulationAdvantage = false;
		cout << "+ApplyLowPopulationAdvantage" ses fApplyLowPopulationAdvantage nl;
	}

	if( version < 8 )
	{
		cout nlf;
		fb.close();
		return;
	}
	if( version >= 22 )
	{
		in >> fRecordBirthsDeaths; in >> label;
		cout << "RecordBirthsDeaths" ses fRecordBirthsDeaths nl;
	}
	
	if( version >= 11 )
	{
		in >> fBrainAnatomyRecordAll; in >> label;
		cout << "brainAnatomyRecordAll" ses fBrainAnatomyRecordAll nl;
		in >> fBrainFunctionRecordAll; in >> label;
		cout << "brainFunctionRecordAll" ses fBrainFunctionRecordAll nl;
		
		if( version >= 12 )
		{
			in >> fBrainAnatomyRecordSeeds; in >> label;
			cout << "brainAnatomyRecordSeeds" ses fBrainAnatomyRecordSeeds nl;
			in >> fBrainFunctionRecordSeeds; in >> label;
			cout << "brainFunctionRecordSeeds" ses fBrainFunctionRecordSeeds nl;
		}
		
		in >> fBestSoFarBrainAnatomyRecordFrequency; in >> label;
		cout << "bestSoFarBrainAnatomyRecordFrequency" ses fBestSoFarBrainAnatomyRecordFrequency nl;
		in >> fBestSoFarBrainFunctionRecordFrequency; in >> label;
		cout << "bestSoFarBrainFunctionRecordFrequency" ses fBestSoFarBrainFunctionRecordFrequency nl;
		
		if( version >= 14 )
		{
			in >> fBestRecentBrainAnatomyRecordFrequency; in >> label;
			cout << "bestRecentBrainAnatomyRecordFrequency" ses fBestRecentBrainAnatomyRecordFrequency nl;
			in >> fBestRecentBrainFunctionRecordFrequency; in >> label;
			cout << "bestRecentBrainFunctionRecordFrequency" ses fBestRecentBrainFunctionRecordFrequency nl;
		}
	}
	
	if( version >= 12 )
	{
		in >> fRecordGeneStats; in >> label;
		cout << "recordGeneStats" ses fRecordGeneStats nl;
	}

	if( version >= 15 )
	{
		in >> fRecordFoodPatchStats; in >> label;
		cout << "recordFoodPatchStats" ses fRecordFoodPatchStats nl;
	}


	if( version >= 18 )
	{
	

		in >> fRecordComplexity; in >> label;
		cout << "recordComplexity" ses fRecordComplexity nl;
	
		if( fRecordComplexity )
		{
			if( ! fBestSoFarBrainFunctionRecordFrequency )
			{
				if( fBestRecentBrainFunctionRecordFrequency )
					fBestSoFarBrainFunctionRecordFrequency = fBestRecentBrainFunctionRecordFrequency;
				else
					fBestSoFarBrainFunctionRecordFrequency = 1000;
				cerr << "Warning: Attempted to record Complexity without recording \"best so far\" brain function.  Setting BestSoFarBrainFunctionRecordFrequency to " << fBestSoFarBrainFunctionRecordFrequency nl;
			}
			
			if( ! fBestSoFarBrainAnatomyRecordFrequency )
			{
				if( fBestRecentBrainAnatomyRecordFrequency )
					fBestSoFarBrainAnatomyRecordFrequency = fBestRecentBrainAnatomyRecordFrequency;
				else
					fBestSoFarBrainAnatomyRecordFrequency = 1000;				
				cerr << "Warning: Attempted to record Complexity without recording \"best so far\" brain anatomy.  Setting BestSoFarBrainAnatomyRecordFrequency to " << fBestSoFarBrainAnatomyRecordFrequency nl;
			}

			if( ! fBestRecentBrainFunctionRecordFrequency )
			{
				if( fBestSoFarBrainAnatomyRecordFrequency )
					fBestRecentBrainFunctionRecordFrequency = fBestSoFarBrainAnatomyRecordFrequency;
				else
					fBestRecentBrainFunctionRecordFrequency = 1000;
				cerr << "Warning: Attempted to record Complexity without recording \"best recent\" brain function.  Setting BestRecentBrainFunctionRecordFrequency to " << fBestRecentBrainFunctionRecordFrequency nl;
			}
			
			if( ! fBestRecentBrainAnatomyRecordFrequency )
			{
				if( fBestSoFarBrainAnatomyRecordFrequency )
					fBestRecentBrainAnatomyRecordFrequency = fBestSoFarBrainAnatomyRecordFrequency;
				else
					fBestRecentBrainAnatomyRecordFrequency = 1000;				
				cerr << "Warning: Attempted to record Complexity without recording \"best recent\" brain anatomy.  Setting BestRecentBrainAnatomyRecordFrequency to " << fBestRecentBrainAnatomyRecordFrequency nl;
			}
		}
		
		in >> fUseComplexityAsFitnessFunc; in >> label;
		if( version < 28 )
		{
			if( fUseComplexityAsFitnessFunc == '0' )	// zero used to mean off
				fUseComplexityAsFitnessFunc = 'O';
			else
				fUseComplexityAsFitnessFunc = 'P';	// on used to assume processing complexity
		}
		cout << "UseComplexityAsFitnessFunction" ses fUseComplexityAsFitnessFunc nl;

		if( fUseComplexityAsFitnessFunc != 'O' )
		{
			if( ! fRecordComplexity )		//Not Recording Complexity?
			{
				cerr << "Warning: Attempted to use Complexity as fitness func without recording Complexity.  Turning on RecordComplexity." nl;
				fRecordComplexity = true;
			}
		
			if( ! fBrainFunctionRecordAll )	//Not recording BrainFunction?
			{
				cerr << "Warning: Attempted to use Complexity as fitness func without recording brain function.  Turning on RecordBrainFunctionAll." nl;
				fBrainFunctionRecordAll = true;
			}

			if( ! fBrainAnatomyRecordAll )
			{
				cerr << "Warning: Attempted to use Complexity as fitness func without recording brain anatomy.  Turning on RecordBrainAnatomyAll." nl;
				fBrainAnatomyRecordAll = true;				
			}

		}

		if( version >= 20 )
		{
			in >> fRecordAdamiComplexity; in >> label;
			cout << "recordAdamiComplexity" ses fRecordAdamiComplexity nl;
			
			in >> fAdamiComplexityRecordFrequency; in >> label;
			cout << "AdamiComplexityRecordFrequency" ses fAdamiComplexityRecordFrequency nl;
		}
		
	}	

	if( version >= 18 )
	{
		in >> fFogFunction; in >> label;
		
		if( toupper(fFogFunction) == 'E' )			//Exponential
			fFogFunction = 'E';
		else if( toupper(fFogFunction) == 'L' )			//Linear
			fFogFunction = 'L';
		else
			fFogFunction = 'O';				// No Fog (OFF)

		assert( (fFogFunction == 'O' || fFogFunction == 'E' || fFogFunction == 'L') );
		cout << "FogFunction" ses fFogFunction nl;
		assert( glFogFunction() == fFogFunction );
			
	
		// This value only does something if Fog Function is exponential
		// Acceptable values are between 0 and 1 (inclusive)
		in >> fExpFogDensity; in >> label;
		cout << "ExpFogDensity" ses fExpFogDensity nl;

		// This value only does something if Fog Function is linear
		// It defines the maximum distance a critter can see.
		in >> fLinearFogEnd; in >> label;
		cout << "LinearFogEnd" ses fLinearFogEnd nl;

	}	
	
	
	in >> fRecordMovie; in >> label;
	cout << "recordMovie" ses fRecordMovie nl;
	
	// If this is a lockstep run, then we need to force certain parameter values (and warn the user)
	if( fLockStepWithBirthsDeathsLog )
	{
		genome::gMinLifeSpan = fMaxSteps;
		genome::gMaxLifeSpan = fMaxSteps;
		
		critter::gEat2Energy = 0.0;
		critter::gMate2Energy = 0.0;
		critter::gFight2Energy = 0.0;
		critter::gMaxSizePenalty = 0.0;
		critter::gSpeed2Energy = 0.0;
		critter::gYaw2Energy = 0.0;
		critter::gLight2Energy = 0.0;
		critter::gFocus2Energy = 0.0;
		critter::gFixedEnergyDrain = 0.0;

		critter::gNumDepletionSteps = 0;
		critter::gMaxPopulationPenaltyFraction = 0.0;
		
		fApplyLowPopulationAdvantage = false;
		
		cout << "Due to running in LockStepWithBirthsDeathsLog mode, the following parameter values have been forcibly reset as indicated:" nl;
		cout << "  MinLifeSpan" ses genome::gMinLifeSpan nl;
		cout << "  MaxLifeSpan" ses genome::gMaxLifeSpan nl;
		cout << "  Eat2Energy" ses critter::gEat2Energy nl;
		cout << "  Mate2Energy" ses critter::gMate2Energy nl;
		cout << "  Fight2Energy" ses critter::gFight2Energy nl;
		cout << "  MaxSizePenalty" ses critter::gMaxSizePenalty nl;
		cout << "  Speed2Energy" ses critter::gSpeed2Energy nl;
		cout << "  Yaw2Energy" ses critter::gYaw2Energy nl;
		cout << "  Light2Energy" ses critter::gLight2Energy nl;
		cout << "  Focus2Energy" ses critter::gFocus2Energy nl;
		cout << "  FixedEnergyDrain" ses critter::gFixedEnergyDrain nl;
		cout << "  NumDepletionSteps" ses critter::gNumDepletionSteps nl;
		cout << "  .MaxPopulationPenaltyFraction" ses critter::gMaxPopulationPenaltyFraction nl;
		cout << "  ApplyLowPopulationAdvantage" ses fApplyLowPopulationAdvantage nl;
	}
	
	// If this is a complexity-as-fitness run, then we need to force certain parameter values (and warn the user)
	if( fUseComplexityAsFitnessFunc != 'O' )
	{
		fNumberToSeed = lrint( fMaxCritters * (float) fNumberToSeed / fInitNumCritters );	// same proportion as originally specified (must calculate before changing fInitNumCritters)
		if( fNumberToSeed > fMaxCritters )	// just to be safe
			fNumberToSeed = fMaxCritters;
		fInitNumCritters = fMaxCritters;	// population starts at maximum
		fMinNumCritters = fMaxCritters;		// population stays at mximum
		fProbabilityOfMutatingSeeds = 1.0;	// so there is variation in the initial population
		fMateThreshold = 1.5;				// so they can't reproduce on their own

		for( int i = 0; i < fNumDomains; i++ )	// over all domains
		{
			fDomains[i].numberToSeed = lrint( fDomains[i].maxNumCritters * (float) fDomains[i].numberToSeed / fDomains[i].initNumCritters );	// same proportion as originally specified (must calculate before changing fInitNumCritters)
			if( fDomains[i].numberToSeed > fDomains[i].maxNumCritters )	// just to be safe
				fDomains[i].numberToSeed = fDomains[i].maxNumCritters;
			fDomains[i].initNumCritters = fDomains[i].maxNumCritters;	// population starts at maximum
			fDomains[i].minNumCritters  = fDomains[i].maxNumCritters;	// population stays at maximum
			fDomains[i].probabilityOfMutatingSeeds = 1.0;				// so there is variation in the initial population
		}

		critter::gNumDepletionSteps = 0;				// turn off the high-population penalty
		critter::gMaxPopulationPenaltyFraction = 0.0;	// ditto
		fApplyLowPopulationAdvantage = false;			// turn off the low-population advantage
		
		fRecordComplexity = true;						// record it, since we have to compute it

		cout << "Due to running with Complexity as the fitness function, the following parameter values have been forcibly reset as indicated:" nl;
		cout << "  InitNumCritters" ses fInitNumCritters nl;
		cout << "  MinNumCritters" ses fMinNumCritters nl;
		cout << "  NumberToSeed" ses fNumberToSeed nl;
		cout << "  ProbabilityOfMutatingSeeds" ses fProbabilityOfMutatingSeeds nl;
		cout << "  MateThreshold" ses fMateThreshold nl;
		for( int i = 0; i < fNumDomains; i++ )
		{
			cout << "  Domain " << i << ":" nl;
			cout << "    initNumCritters" ses fDomains[i].initNumCritters nl;
			cout << "    minNumCritters" ses fDomains[i].minNumCritters nl;
			cout << "    numberToSeed" ses fDomains[i].numberToSeed nl;
			cout << "    probabilityOfMutatingSeeds" ses fDomains[i].probabilityOfMutatingSeeds nl;
		}
		cout << "  NumDepletionSteps" ses critter::gNumDepletionSteps nl;
		cout << "  .MaxPopulationPenaltyFraction" ses critter::gMaxPopulationPenaltyFraction nl;
		cout << "  ApplyLowPopulationAdvantage" ses fApplyLowPopulationAdvantage nl;
		cout << "  RecordComplexity" ses fRecordComplexity nl;
	}

	cout nlf;
    fb.close();
	return;
}



//---------------------------------------------------------------------------
// TSimulation::Dump
//---------------------------------------------------------------------------
void TSimulation::Dump()
{
    filebuf fb;
    if (fb.open("pw.dump", ios_base::out) == 0)
    {
        error(1, "Unable to dump state to \"pw.dump\" file");
        return;
    }

    ostream out(&fb);
    char version[16];
    sprintf(version, "%s", "pw1");

    out << version nl;
    out << fStep nl;
    out << fCameraAngle nl;
    out << fNumberCreated nl;
    out << fNumberCreatedRandom nl;
    out << fNumberCreated1Fit nl;
    out << fNumberCreated2Fit nl;
    out << fNumberDied nl;
    out << fNumberDiedAge nl;
    out << fNumberDiedEnergy nl;
    out << fNumberDiedFight nl;
    out << fNumberDiedEdge nl;
	out << fNumberDiedSmite nl;
    out << fNumberBorn nl;
    out << fNumberFights nl;
    out << fMiscDenials nl;
    out << fLastCreated nl;
    out << fMaxGapCreate nl;
    out << fNumBornSinceCreated nl;

    out << fMonitorCritterRank nl;
    out << fMonitorCritterRankOld nl;
    out << fCritterTracking nl;
    out << fOverHeadRank nl;
    out << fOverHeadRankOld nl;
    out << fMaxGeneSeparation nl;
    out << fMinGeneSeparation nl;
    out << fAverageGeneSeparation nl;
    out << fMaxFitness nl;
    out << fAverageFitness nl;
    out << fAverageFoodEnergyIn nl;
    out << fAverageFoodEnergyOut nl;
    out << fTotalFoodEnergyIn nl;
    out << fTotalFoodEnergyOut nl;

    critter::critterdump(out);

    out << objectxsortedlist::gXSortedObjects.getCount(FOODTYPE) nl;
	food* f = NULL;
	objectxsortedlist::gXSortedObjects.reset();
	while (objectxsortedlist::gXSortedObjects.nextObj(FOODTYPE, (gobject**)&f))
		f->dump(out);
					
    out << fNumberFit nl;
    out << fFitI nl;
    out << fFitJ nl;
    for (int index = 0; index < fNumberFit; index++)
    {
		out << fFittest[index]->critterID nl;
        out << fFittest[index]->fitness nl;
        fFittest[index]->genes->Dump(out);
    }
	out << fNumberRecentFit nl;
    for (int index = 0; index < fNumberRecentFit; index++)
    {
		out << fRecentFittest[index]->critterID nl;
		out << fRecentFittest[index]->fitness nl;
//		fRecentFittest[index]->genes->Dump(out);	// we don't save the genes in the bestRecent list
    }

    out << fNumDomains nl;
    for (int id = 0; id < fNumDomains; id++)
    {
        out << fDomains[id].numCritters nl;
        out << fDomains[id].numcreated nl;
        out << fDomains[id].numborn nl;
        out << fDomains[id].numbornsincecreated nl;
        out << fDomains[id].numdied nl;
        out << fDomains[id].lastcreate nl;
        out << fDomains[id].maxgapcreate nl;
        out << fDomains[id].ifit nl;
        out << fDomains[id].jfit nl;

        int numfitid = 0;
        if (fDomains[id].fittest)
        {
            int i;
            for (i = 0; i < fNumberFit; i++)
            {
                if (fDomains[id].fittest[i])
                    numfitid++;
                else
                    break;
            }
            out << numfitid nl;

            for (i = 0; i < numfitid; i++)
            {
				out << fDomains[id].fittest[i]->critterID nl;
                out << fDomains[id].fittest[i]->fitness nl;
                fDomains[id].fittest[i]->genes->Dump(out);
            }
        }
        else
            out << numfitid nl;
    }
	
	// Dump monitor windows
	if (fBirthrateWindow != NULL)
		fBirthrateWindow->Dump(out);

	if (fFitnessWindow != NULL)
		fFitnessWindow->Dump(out);

	if (fFoodEnergyWindow != NULL)
		fFoodEnergyWindow->Dump(out);

	if (fPopulationWindow != NULL)
		fPopulationWindow->Dump(out);

	if (fGeneSeparationWindow != NULL)
		fGeneSeparationWindow->Dump(out);
		

    out.flush();
    fb.close();
}


//---------------------------------------------------------------------------
// TSimulation::WhichDomain
//---------------------------------------------------------------------------
short TSimulation::WhichDomain(float x, float z, short d)
{
	for (short i = 0; i < fNumDomains; i++)
	{
		if (((x >= fDomains[i].startX) && (x <= fDomains[i].endX)) && 
			((z >= fDomains[i].startZ) && (z <= fDomains[i].endZ)))
			return i;
	}

	// If we reach here, we failed to find a domain, so kvetch and quit
	
	char errorString[256];
	
	printf( "Domain not found in %ld domains, located at:\n", fNumDomains );
	for( int i = 0; i < fNumDomains; i++ )
		printf( "  %d: ranging over x = (%f -> %f) and z = (%f -> %f)\n",
				i, fDomains[i].startX, fDomains[i].endX, fDomains[i].startZ, fDomains[i].endZ );
	
	sprintf(errorString,"%s (%g, %g) %s %d, %ld",
			"WhichDomain failed to find any domain for point at (x, z) = ",
			x, z, " & d, nd = ", d, fNumDomains);
	error(2, errorString);
	
	return( -1 );	// not really returning, as error(2,...) will abort
}


//---------------------------------------------------------------------------
// TSimulation::SwitchDomain
//
//	No verification is done. Assume caller has verified domains.
//---------------------------------------------------------------------------
void TSimulation::SwitchDomain(short newDomain, short oldDomain)
{
	Q_ASSERT(newDomain != oldDomain);
		
	fDomains[newDomain].numCritters++;
	fDomains[oldDomain].numCritters--;
}


//---------------------------------------------------------------------------
// TSimulation::PopulateStatusList
//---------------------------------------------------------------------------
void TSimulation::PopulateStatusList(TStatusList& list)
{
	char t[256];
	char t2[256];
	short id;
	
	// TODO: If we're neither updating the window, nor writing to the stat file,
	// then we shouldn't sprintf all these strings, or put them in the list
	// (but for now, the window always draws anyway, so it's not a big deal)
	
	sprintf( t, "step = %ld", fStep );
	list.push_back( strdup( t ) );
	
	sprintf( t, "critters = %4d", objectxsortedlist::gXSortedObjects.getCount(CRITTERTYPE) );
	if (fNumDomains > 1)
	{
		sprintf(t2, " (%ld",fDomains[0].numCritters );
		strcat(t, t2 );
		for (id = 1; id < fNumDomains; id++)
		{
			sprintf(t2, ", %ld", fDomains[id].numCritters );
			strcat( t, t2 );
		}
		
		strcat(t,")" );		
	}
	list.push_back( strdup( t ) );

	sprintf( t, "food = %4d", objectxsortedlist::gXSortedObjects.getCount(FOODTYPE) );
	if (fNumDomains > 1)
	{
		sprintf(t2, " (%d",fDomains[0].foodCount );
		strcat(t, t2 );
		for (id = 1; id < fNumDomains; id++)
		{
			sprintf(t2, ", %d", fDomains[id].foodCount );
			strcat( t, t2 );
		}
		
		strcat(t,")" );		
	}
	list.push_back( strdup( t ) );

	sprintf( t, "created  = %4ld", fNumberCreated );
	if (fNumDomains > 1)
	{
		sprintf( t2, " (%ld",fDomains[0].numcreated );
		strcat( t, t2 );
		
		for (id = 1; id < fNumDomains; id++)
		{
			sprintf( t2, ",%ld",fDomains[id].numcreated );
			strcat( t, t2 );
		}
		strcat(t,")" );
	}
	list.push_back( strdup( t ) );

	sprintf( t, " -random = %4ld", fNumberCreatedRandom );
	list.push_back( strdup( t ) );

	sprintf( t, " -two    = %4ld", fNumberCreated2Fit );
	list.push_back( strdup( t ) );

	sprintf( t, " -one    = %4ld", fNumberCreated1Fit );
	list.push_back( strdup( t ) );

	sprintf( t, "born     = %4ld", fNumberBorn );
	if (fNumDomains > 1)
	{
		sprintf( t2, " (%ld",fDomains[0].numborn );
		strcat( t, t2 );
		for (id = 1; id < fNumDomains; id++)
		{
			sprintf( t2, ",%ld",fDomains[id].numborn );
			strcat( t, t2 );
		}
		strcat(t,")" );
	}
	list.push_back( strdup( t ) );

	sprintf( t, "died     = %4ld", fNumberDied );
	if (fNumDomains > 1)
	{
		sprintf( t2, " (%ld",fDomains[0].numdied );
		strcat( t, t2 );
		for (id = 1; id < fNumDomains; id++)
		{
			sprintf( t2, ",%ld",fDomains[id].numdied );
			strcat( t, t2 );
		}
		strcat(t,")" );
	}
	list.push_back( strdup( t ) );


	sprintf( t, " -age    = %4ld", fNumberDiedAge );
	list.push_back( strdup( t ) );
	
	sprintf( t, " -energy = %4ld", fNumberDiedEnergy );
	list.push_back( strdup( t ) );
	
	sprintf( t, " -fight  = %4ld", fNumberDiedFight );
	list.push_back( strdup( t ) );
	
	sprintf( t, " -edge   = %4ld", fNumberDiedEdge );
	list.push_back( strdup( t ) );

	sprintf( t, " -smite  = %4ld", fNumberDiedSmite );
	list.push_back( strdup( t ) );

    sprintf( t, "birthDenials = %ld", fBirthDenials );
	list.push_back( strdup( t ) );

    sprintf( t, "miscDenials = %ld", fMiscDenials );
	list.push_back( strdup( t ) );

    sprintf( t, "ageCreate = %ld", fLastCreated );
    if (fNumDomains > 1)
    {
        sprintf( t2, " (%ld",fDomains[0].lastcreate );
        strcat( t, t2 );
        for (id = 1; id < fNumDomains; id++)
        {
            sprintf( t2, ",%ld",fDomains[id].lastcreate );
            strcat( t, t2 );
        }
        strcat(t,")" );
    }
	list.push_back( strdup( t ) );

	sprintf( t, "maxGapCreate = %ld", fMaxGapCreate );
	if (fNumDomains > 1)
	{
		sprintf( t2, " (%ld",fDomains[0].maxgapcreate );
	   	strcat( t, t2 );
	   	for (id = 1; id < fNumDomains; id++)
	   	{
			sprintf( t2, ",%ld", fDomains[id].maxgapcreate );
	       	strcat(t, t2 );
	   	}
	   	strcat(t,")" );
	}
	list.push_back( strdup( t ) );

	sprintf( t, "born/total = %.2f", float(fNumberBorn) / float(fNumberCreated + fNumberBorn) );
	list.push_back( strdup( t ) );

	sprintf( t, "Fitness m=%.2f, c=%.2f, a=%.2f", fMaxFitness, fCurrentMaxFitness[0], fAverageFitness );
	list.push_back( strdup( t ) );
	
	sprintf( t, "NormFit m=%.2f, c=%.2f, a=%.2f", fMaxFitness / fTotalFitness, fCurrentMaxFitness[0] / fTotalFitness, fAverageFitness / fTotalFitness );
	list.push_back( strdup( t ) );
	
	sprintf( t, "Fittest =" );
	int fittestCount = min( 5, min( fNumberFit, (int) fNumberDied ));
	for( int i = 0; i < fittestCount; i++ )
	{
		sprintf( t2, " %lu", fFittest[i]->critterID );
		strcat( t, t2 );
	}
	list.push_back( strdup( t ) );
	
	if( fittestCount > 0 )
	{
		sprintf( t, " " );
		for( int i = 0; i < fittestCount; i++ )
		{
			sprintf( t2, "  %.2f", fFittest[i]->fitness );
			strcat( t, t2 );
		}
		list.push_back( strdup( t ) );
	}
	
	sprintf( t, "CurFit =" );
	for( int i = 0; i < fCurrentFittestCount; i++ )
	{
		sprintf( t2, " %lu", fCurrentFittestCritter[i]->Number() );
		strcat( t, t2 );
	}
	list.push_back( strdup( t ) );
	
	if( fCurrentFittestCount > 0 )
	{
		sprintf( t, " " );
		for( int i = 0; i < fCurrentFittestCount; i++ )
		{
			sprintf( t2, "  %.2f", fCurrentFittestCritter[i]->Fitness() );
			strcat( t, t2 );
		}
		list.push_back( strdup( t ) );
	}
	
	sprintf( t, "avgFoodEnergy = %.2f", (fAverageFoodEnergyIn - fAverageFoodEnergyOut) / (fAverageFoodEnergyIn + fAverageFoodEnergyOut) );
	list.push_back( strdup( t ) );
	
	sprintf( t, "totFoodEnergy = %.2f", (fTotalFoodEnergyIn - fTotalFoodEnergyOut) / (fTotalFoodEnergyIn + fTotalFoodEnergyOut) );
	list.push_back( strdup( t ) );

	if (fMonitorGeneSeparation)
	{
		sprintf( t, "GeneSep = %5.3f [%5.3f, %5.3f]", fAverageGeneSeparation, fMinGeneSeparation, fMaxGeneSeparation );
		list.push_back( strdup( t ) );
	}

	sprintf( t, "LifeSpan = %lu � %lu [%lu, %lu]", nint( fLifeSpanStats.mean() ), nint( fLifeSpanStats.stddev() ), (ulong) fLifeSpanStats.min(), (ulong) fLifeSpanStats.max() );
	list.push_back( strdup( t ) );

	sprintf( t, "RecLifeSpan = %lu � %lu [%lu, %lu]", nint( fLifeSpanRecentStats.mean() ), nint( fLifeSpanRecentStats.stddev() ), (ulong) fLifeSpanRecentStats.min(), (ulong) fLifeSpanRecentStats.max() );
	list.push_back( strdup( t ) );

	sprintf( t, "CurNeurons = %.1f � %.1f [%lu, %lu]", fCurrentNeuronCountStats.mean(), fCurrentNeuronCountStats.stddev(), (ulong) fCurrentNeuronCountStats.min(), (ulong) fCurrentNeuronCountStats.max() );
	list.push_back( strdup( t ) );

	sprintf( t, "NeurGroups = %.1f � %.1f [%lu, %lu]", fNeuronGroupCountStats.mean(), fNeuronGroupCountStats.stddev(), (ulong) fNeuronGroupCountStats.min(), (ulong) fNeuronGroupCountStats.max() );
	list.push_back( strdup( t ) );

	sprintf( t, "CurNeurGroups = %.1f � %.1f [%lu, %lu]", fCurrentNeuronGroupCountStats.mean(), fCurrentNeuronGroupCountStats.stddev(), (ulong) fCurrentNeuronGroupCountStats.min(), (ulong) fCurrentNeuronGroupCountStats.max() );
	list.push_back( strdup( t ) );

	sprintf( t, "Rate %2.1f (%2.1f) %2.1f (%2.1f) %2.1f (%2.1f)",
				fFramesPerSecondInstantaneous, fSecondsPerFrameInstantaneous,
				fFramesPerSecondRecent,        fSecondsPerFrameRecent,
				fFramesPerSecondOverall,       fSecondsPerFrameOverall  );
	list.push_back( strdup( t ) );
	
	if( fRecordFoodPatchStats )
	{
		int numCrittersInAnyFoodPatchInAnyDomain = 0;
		int numCrittersInOuterRangesInAnyDomain = 0;

		for( int domainNumber = 0; domainNumber < fNumDomains; domainNumber++ )
		{
			sprintf( t, "Domain %d", domainNumber);
			list.push_back( strdup( t ) );
			
			int numCrittersInAnyFoodPatch = 0;
			int numCrittersInOuterRanges = 0;

			for( int i = 0; i < fDomains[domainNumber].numFoodPatches; i++ )
			{
				numCrittersInAnyFoodPatch += fDomains[domainNumber].fFoodPatches[i].critterInsideCount;
				numCrittersInOuterRanges += fDomains[domainNumber].fFoodPatches[i].critterNeighborhoodCount;
			}
			
			float makePercent = 100.0 / fDomains[domainNumber].numCritters;
			float makePercentNorm = 100.0 / numCrittersInAnyFoodPatch;

			for( int i = 0; i < fDomains[domainNumber].numFoodPatches; i++ )
			{
				sprintf( t, "  FP%d %3d %3d  %4.1f %4.1f  %4.1f",
						 i,
						 fDomains[domainNumber].fFoodPatches[i].critterInsideCount,
						 fDomains[domainNumber].fFoodPatches[i].critterInsideCount + fDomains[domainNumber].fFoodPatches[i].critterNeighborhoodCount,
						 fDomains[domainNumber].fFoodPatches[i].critterInsideCount * makePercent,
						(fDomains[domainNumber].fFoodPatches[i].critterInsideCount + fDomains[domainNumber].fFoodPatches[i].critterNeighborhoodCount) * makePercent,
						 fDomains[domainNumber].fFoodPatches[i].critterInsideCount * makePercentNorm );
				list.push_back( strdup( t ) );
			}
			
			
			sprintf( t, "  FP* %3d %3d  %4.1f %4.1f 100.0",
					 numCrittersInAnyFoodPatch,
					 numCrittersInAnyFoodPatch + numCrittersInOuterRanges,
					 numCrittersInAnyFoodPatch * makePercent,
					(numCrittersInAnyFoodPatch + numCrittersInOuterRanges) * makePercent );
			list.push_back( strdup( t ) );

			numCrittersInAnyFoodPatchInAnyDomain += numCrittersInAnyFoodPatch;
			numCrittersInOuterRangesInAnyDomain += numCrittersInOuterRanges;
		}

		if( fNumDomains > 1 )
		{
			float makePercent = 100.0 / objectxsortedlist::gXSortedObjects.getCount( CRITTERTYPE );

			sprintf( t, "**FP* %3d %3d  %4.1f %4.1f 100.0",
					 numCrittersInAnyFoodPatchInAnyDomain,
					 numCrittersInAnyFoodPatchInAnyDomain + numCrittersInOuterRangesInAnyDomain,
					 numCrittersInAnyFoodPatchInAnyDomain * makePercent,
					(numCrittersInAnyFoodPatchInAnyDomain + numCrittersInOuterRangesInAnyDomain) * makePercent );
			list.push_back( strdup( t ) );
		}
	}
	
    if( !(fStep % fStatusFrequency) || (fStep == 1) )
    {
		char statusFileName[256];
		
		sprintf( statusFileName, "run/stats/stat.%ld", fStep );
        FILE *statusFile = fopen( statusFileName, "w" );
		Q_CHECK_PTR( statusFile );
		
		TStatusList::const_iterator iter = list.begin();
		for( ; iter != list.end(); ++iter )
			fprintf( statusFile, "%s\n", *iter );

        fclose( statusFile );
    }
}


//---------------------------------------------------------------------------
// TSimulation::Update
//
// Do not call this function as part of the simulation.  It is only used to
// refresh all windows after something external, like a screen saver,
// overwrote all the windows.
//---------------------------------------------------------------------------
void TSimulation::Update()
{
	// Redraw main simulation window
	fSceneView->updateGL();

	// Update critter POV window
	if (fShowVision && fCritterPOVWindow != NULL && !fCritterPOVWindow->isHidden())
		fCritterPOVWindow->updateGL();
		//QApplication::postEvent(fCritterPOVWindow, new QCustomEvent(kUpdateEventType));	
	
	// Update chart windows
	if (fChartBorn && fBirthrateWindow != NULL && fBirthrateWindow->isVisible())
		fBirthrateWindow->updateGL();
		//QApplication::postEvent(fBirthrateWindow, new QCustomEvent(kUpdateEventType));	
			
	if (fChartFitness && fFitnessWindow != NULL && fFitnessWindow->isVisible())
		fFitnessWindow->updateGL();
		//QApplication::postEvent(fFitnessWindow, new QCustomEvent(kUpdateEventType));	
			
	if (fChartFoodEnergy && fFoodEnergyWindow != NULL && fFoodEnergyWindow->isVisible())
		fFoodEnergyWindow->updateGL();
		//QApplication::postEvent(fFoodEnergyWindow, new QCustomEvent(kUpdateEventType));	
	
	if (fChartPopulation && fPopulationWindow != NULL && fPopulationWindow->isVisible())
		fPopulationWindow->updateGL();
		//QApplication::postEvent(fPopulationWindow, new QCustomEvent(kUpdateEventType));	

	if (fBrainMonitorWindow != NULL && fBrainMonitorWindow->isVisible())
		fBrainMonitorWindow->updateGL();
		//QApplication::postEvent(fBrainMonitorWindow, new QCustomEvent(kUpdateEventType));	

	if (fChartGeneSeparation && fGeneSeparationWindow != NULL)
		fGeneSeparationWindow->updateGL();
		//QApplication::postEvent(fGeneSeparationWindow, new QCustomEvent(kUpdateEventType));	
		
	if (fShowTextStatus && fTextStatusWindow != NULL)
		fTextStatusWindow->Draw();
		//QApplication::postEvent(fTextStatusWindow, new QCustomEvent(kUpdateEventType));	
}


int TSimulation::getRandomPatch( int domainNumber )
{
	int patch;
	float ranval;
	float maxFractions = 0.0;
	
	// Since not all patches may be "on", we need to calculate the maximum fraction
	// attainable by those patches that are on, and therefore allowed to grow food
	for( short i = 0; i < fDomains[domainNumber].numFoodPatches; i++ )
		if( fDomains[domainNumber].fFoodPatches[i].on( fStep ) )
			maxFractions += fDomains[domainNumber].fFoodPatches[i].fraction;
	
	if( maxFractions > 0.0 )	// there is an active patch in this domain
	{
		float sumFractions = 0.0;
		
		// Weight the random value by the maximum attainable fraction, so we always get
		// a valid patch selection (if possible--they could all be off)
		ranval = drand48() * maxFractions;

		for( short i = 0; i < fDomains[domainNumber].numFoodPatches; i++ )
		{
			if( fDomains[domainNumber].fFoodPatches[i].on( fStep ) )
			{
				sumFractions += fDomains[domainNumber].fFoodPatches[i].fraction;
				if( ranval <= sumFractions )
					return( i );    // this is the patch
			}
		}
	
		// Shouldn't get here
		patch = int( floor( ranval * fDomains[domainNumber].numFoodPatches ) );
		if( patch >= fDomains[domainNumber].numFoodPatches )
			patch  = fDomains[domainNumber].numFoodPatches - 1;
		fprintf( stderr, "%s: ranval of %g failed to end up in any food patch; assigning patch #%d\n", __FUNCTION__, ranval, patch );
	}
	else
		patch = -1;	// no patches are active in this domain
	
	return( patch );
}

void TSimulation::SetNextLockstepEvent()
{
	if( !fLockStepWithBirthsDeathsLog )
	{
		// if SetNextLockstepEvent() is called w/o fLockStepWithBirthsDeathsLog turned on, error and exit.
		cerr << "ERROR: You called SetNextLockstepEvent() and 'fLockStepWithBirthsDeathsLog' isn't set to true.  Though not fatal, it's certain that you didn't intend to do this.  Exiting." << endl;
		exit(1);
	}
	
	const char *delimiters = " ";		// a single space is the field delimiter
	char LockstepLine[512];				// making this big in case we add longer lines in the future.

	fLockstepNumDeathsAtTimestep = 0;
	fLockstepNumBirthsAtTimestep = 0;

	if( fgets( LockstepLine, sizeof(LockstepLine), fLockstepFile ) )			// get the next event, if the LOCKSTEP-BirthsDeaths.log still has entries in it.
	{
		fLockstepTimestep = atoi( strtok( LockstepLine, delimiters ) );		// token => timestep
		assert( fLockstepTimestep > 0 );										// if we get a >= zero timestep something is definitely wrong.

		int currentpos;
		int nexttimestep;
		char LockstepEvent;

		do
		{
			nexttimestep = 0;
			LockstepEvent = (strtok( NULL, delimiters ))[0];    // token => event

			//TODO: Add support for the 'GENERATION' event.  Note a GENERATION event cannot be identical to a BIRTH event.  They must be made from the Fittest list.
			if( LockstepEvent == 'B' )
				fLockstepNumBirthsAtTimestep++;
			else if( LockstepEvent == 'D' )
				fLockstepNumDeathsAtTimestep++;
			else if( LockstepEvent == 'C' )
			{
				fLockstepNumBirthsAtTimestep++;
				cerr << "t" << fLockstepTimestep << ": Warning: a CREATION event occured, but we're simply going to treat it as a random BIRTH." << endl;
			}
			else
			{
				cerr << "ERROR/SetNextLockstepEvent(): Currently only support events 'DEATH', 'BIRTH', and 'CREATION' events in the Lockstep file.  Exiting.";
				cerr << "Latest Event: '" << LockstepEvent << "'" << endl;
				exit(1);
			}
			
			currentpos = ftell( fLockstepFile );

			//=======================
						
			if( (fgets(LockstepLine, sizeof(LockstepLine), fLockstepFile)) != NULL )		// if LOCKSTEP-BirthsDeaths.log still has entries in it, nexttimestep is the timestep of the next line.
				nexttimestep = atoi( strtok( LockstepLine, delimiters ) );    // token => timestep
			
		} while( fLockstepTimestep == nexttimestep );
		
		// reset to the beginning of the next timestep
		fseek( fLockstepFile, currentpos, 0 );
		lsPrint( "SetNextLockstepEvent()/ Timestep: %d\tDeaths: %d\tBirths: %d\n", fLockstepTimestep, fLockstepNumDeathsAtTimestep, fLockstepNumBirthsAtTimestep );
	}
}

//GL Fog
char TSimulation::glFogFunction()
{
//	if( fFogFunction == 'O' || fFogFunction == 'L' || fFogFunction == 'E' )
		return fFogFunction;
//	else
//		return 'O';
}

float TSimulation::glExpFogDensity() { return fExpFogDensity; }
int TSimulation::glLinearFogEnd()    { return fLinearFogEnd;  }
