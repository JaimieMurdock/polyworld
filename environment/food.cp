/********************************************************************/
/* PolyWorld:  An Artificial Life Ecological Simulator              */
/* by Larry Yaeger                                                  */
/* Copyright Apple Computer 1990,1991,1992                          */
/********************************************************************/

// food.C - implementation of food classes

// Self
#include "food.h"

// System
#include <ostream>

// qt
#include <qapplication.h>

// Local
#include "critter.h"
#include "globals.h"
#include "graphics.h"


using namespace std;

// External globals
float food::gFoodHeight;
Color food::gFoodColor;
float food::gMinFoodEnergy;
float food::gMaxFoodEnergy;
float food::gSize2Energy;
float food::gMaxFoodRadius;


//===========================================================================
// food
//===========================================================================


//-------------------------------------------------------------------------------------------
// food::food
//-------------------------------------------------------------------------------------------
food::food()
{
	/* Set object type to be FOODTYPE */
	setType(FOODTYPE);

	initfood();
}


//-------------------------------------------------------------------------------------------
// food::food
//-------------------------------------------------------------------------------------------
food::food(float e)
{
	initfood(e);
}


//-------------------------------------------------------------------------------------------
// food::food
//-------------------------------------------------------------------------------------------
food::food(float e, float x, float z)
{
	initfood(e, x, z);
}


//-------------------------------------------------------------------------------------------
// food::~food
//-------------------------------------------------------------------------------------------
food::~food()
{
}


//-------------------------------------------------------------------------------------------
// food::dump
//-------------------------------------------------------------------------------------------
void food::dump(ostream& out)
{
    out << fEnergy nl;
    out << fPosition[0] sp fPosition[1] sp fPosition[2] nl;
}


//-------------------------------------------------------------------------------------------
// food::load
//-------------------------------------------------------------------------------------------
void food::load(istream& in)
{
    in >> fEnergy;
    in >> fPosition[0] >> fPosition[1] >> fPosition[2];

    initlen();
}


//-------------------------------------------------------------------------------------------
// food::eat
//-------------------------------------------------------------------------------------------
float food::eat(float e)
{
	float er = e < fEnergy ? e : fEnergy;
	fEnergy -= er;
	initlen();
	
	return er;
}


//-------------------------------------------------------------------------------------------
// food::initfood
//-------------------------------------------------------------------------------------------
void food::initfood()
{
	fEnergy = drand48() * (gMaxFoodEnergy - gMinFoodEnergy) + gMinFoodEnergy;
	initlen();
	initpos();
	initrest();
}


//-------------------------------------------------------------------------------------------
// food::initfood
//-------------------------------------------------------------------------------------------
void food::initfood(float e)
{
	fEnergy = e;
	initlen();
	initpos();
	initrest();
}


//-------------------------------------------------------------------------------------------
// food::initfood
//-------------------------------------------------------------------------------------------
void food::initfood(float e, float x, float z)
{
	fEnergy = e;
	initlen();
	fPosition[0] = x;
	fPosition[1] = 0.5 * fLength[1];
	fPosition[2] = z;
	initrest();
}
 

//-------------------------------------------------------------------------------------------
// food::initpos
//-------------------------------------------------------------------------------------------    
void food::initpos()
{
	fPosition[0] = drand48() * globals::worldsize;
	fPosition[1] = 0.5 * fLength[1];
	fPosition[2] = drand48() * globals::worldsize;
}


//-------------------------------------------------------------------------------------------
// food::initlen
//-------------------------------------------------------------------------------------------       
void food::initlen()
{
	float lxz = 0.75 * fEnergy / gSize2Energy;
	float ly = gFoodHeight;
	setlen(lxz,ly,lxz);
}


//-------------------------------------------------------------------------------------------
// food::initrest
//-------------------------------------------------------------------------------------------           
void food::initrest()
{
	setcolor(gFoodColor);
}


//-------------------------------------------------------------------------------------------
// food::setenergy
//-------------------------------------------------------------------------------------------           
void food::setenergy(float e)
{
	fEnergy = e;
	initlen();
}


//-------------------------------------------------------------------------------------------
// food::setradius
//-------------------------------------------------------------------------------------------           
void food::setradius()
{
	if( !fRadiusFixed )  //  only set radius anew if not set manually
		fRadius = sqrt( fLength[0]*fLength[0] + fLength[2]*fLength[2] ) * fRadiusScale * fScale * 0.5;
	srPrint( "food::%s(): r=%g%s\n", __FUNCTION__, fRadius, fRadiusFixed ? "(fixed)" : "" );
}

#if 0
//---------------------------------------------------------------------------
// food::FoodWorldZ
//---------------------------------------------------------------------------
float food::FoodWorldZ( float foodBandZ )
{
	float worldZ;
	int i;
	float dz;
	float zHigh;	// this, zLow, and foodBandZ are in the continuous coordinates of gFoodBandZTotal in which the random numbers are generated
	
	zHigh = 0.0;
	for( i = 0; i < gNumFoodBands; i++ )
	{
		dz = gFoodBand[i].zMax - gFoodBand[i].zMin;
		zHigh += dz;
		
		if( foodBandZ <= zHigh )
			break;
	}
	
	if( i >= gNumFoodBands )	// not found; shouldn't be possible
	{
		cerr << "food generated outside of any food band" nl;
		worldZ = - globals::worldsize * 0.5;	// stick it in the precise middle; if we get many of these it should be very noticeable
	}
	else
	{
		float zLow = zHigh - dz;
		
		worldZ = gFoodBand[i].zMin  +  (foodBandZ - zLow);	// this is the normalized value
		worldZ *= globals::worldsize;	// this is the final fully scaled value
	}
	
	return( worldZ );
}
#endif
