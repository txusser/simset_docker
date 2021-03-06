/*********************************************************************************
*                                                                                *
*                       Source code developed by the                             *
*           Imaging Research Laboratory - University of Washington               *
*               (C) Copyright 1992-2013 Department of Radiology                  *
*                           University of Washington                             *
*                              All Rights Reserved                               *
*                                                                                *
*********************************************************************************/
 
/*********************************************************************************
*
*			Module Name:		PhoHStat.c
*			Revision Number:	1.2
*			Date last revised:	23 July 2013
*			Programmer:			Steven Vannoy
*			Date Originated:	19 August 1992
*
*			Module Overview:	Photon History Statistics routines.
*
*			References:			'Photon History Stats Proceses' PHG design.
*
**********************************************************************************
*
*			Global functions defined:
*				PhoHStatIncDetectedPhotonStats
*				PhoHStatIncTotInvalPhoLocations
*				PhoHStatIncTotLowEnPhotons
*				PhoHStatIncTotAbsorbedPhotons
*				PhoHStatIncTotPrimOnlyScatter
*				PhoHStatInitPhgStatistics
*				PhoHStatUpdateForDetAttemptWeight
*				PhoHStatUpdateForDetHitWeight
*				PhoHStatUpdateRoulettedPhotons
*				PhoHStatUpdateStartedPhotons
*				PhoHStatUpdateSplitPhotons
*				PhoHStatUpdateStartingWeight
*				PhoHStatWrite
*				PhoHStat_GetTotInvalPhoLocations
*
*			Global variables defined:		none
*
**********************************************************************************
*
*			Revision Section (Also update version number, if relevant)
*
*			Programmer(s):		
*
*			Revision date:		
*
*			Revision description:
*
**********************************************************************************
*
*			Revision Section (Also update version number, if relevant)
*
*			Programmer(s):		Robert Harrison
*
*			Revision date:		2007
*
*			Revision description:
*				- support for eight-byte integer counting variables.
*
*********************************************************************************/

#include <stdio.h>
#include <time.h>

#include "SystemDependent.h"

#include "LbTypes.h"
#include "LbMacros.h"
#include "LbError.h"
#include "LbMemory.h"
#include "LbParamFile.h"
#include "LbFile.h"
#include "LbInterface.h"
#include "LbHeader.h"

#include "Photon.h"
#include "PhgParams.h"
#include "ProdTbl.h"
#include "SubObj.h"
#include "ColTypes.h"
#include "ColParams.h"
#include "PhgMath.h"
#include "ColUsr.h"
#include "CylPos.h"
#include "DetTypes.h"
#include "DetParams.h"
#include "PhoHFile.h"
#include "PhoHStat.h"
#include "phg.h"
#include "PhgBin.h"

#define MAX_ENERGY_BINS   200 	/* Maximum number of energy bins */
#define ENERGY_BIN_SIZE    10 	/* Size of energy bins (in energy units) */
#define MAX_SCATTER_BINS   10	/* Maximum number of scatter bins */


struct PhoHStat_Statistics { 

		/*	Total number of photons produced by simulated 
			positron annihilations and by direct
			radionuclide photon emission.
		*/
	LbEightByte total_started_photons;
	
		/*	Total number of primary photons produced by
			simulated positron annhiliations and by direct
			radionuclide photon emission.
		*/
	LbEightByte total_started_prim_photons;

		/*	Total number of primary only photons produced by
			simulated positron annhiliations and by direct
			radionuclide photon emission.
		*/
	LbEightByte total_started_prim_only_photons;

		/* Total number of primary only photons that scattered,
		   and hence, were discarded.
		   
		*/
	LbEightByte total_prim_only_that_scattered;
		/*	Total number of scatter photons produced by
			simulated positron annhiliations and by direct
			radionuclide photon emission.
		*/
	LbEightByte total_started_scat_photons;
	
	
		/*	Total weight of scatter photons generated by 
			nuclide or positron annihilation decay
		*/
	double total_scat_photon_starting_weight;
	
		/*	Total weight of primary photons generated by 
			nuclide or positron annihilation decay
		*/
	double total_prim_photon_starting_weight;
		
		/*	A total of all photons that fall below the
			minimum photon energy.
		*/
	LbEightByte total_low_energy_photons;
	
		/* A total of all photons that are absorbed due to
			interaction probability
		*/
	LbEightByte   total_absorbed_photons;
	double total_absorbed_photons_weight;
	
		/*	The total number photons created by photon
			splitting.  When a photon's weight exceeds the 
			maximum acceptable weight, it is split into 
			several photons with weight in the acceptable
			range.
		*/
	LbEightByte	total_split_photons;
	
		/*	The number of times we split photons.   I.E. the number
			of times a photon's weight went above our threshold and
			we split it.
		*/
	LbEightByte	total_split_attempts;
	
	
		/*	The total of photons eliminated due to
			Rusian Roulette.  If a photon's weight
			drops below the minimum acceptable weight,
			it is either terminated or its weight is
			raised into the acceptable range.  This is
			called Rusian Roulette.
		*/
	
	LbEightByte	total_rouletted_photons;
	
		/*	The total number of times a photon's weight went
			below the roulette threshold, and rouletting was
			attempted.
		*/
	LbEightByte   total_roulette_attempts;
	
		/*	The number of times attempts were
			made to force photons to the target surface.
		*/
	LbEightByte number_forced_detection_attempts;	
	
		/*	The total weight of photons for
			which forced detection attempts were made.
		*/
	double forced_detection_attempt_weight;
	
	
		/*	The number of photons hitting the
			target surface after forced detection attempts. Such photons are
			recorded in the gPhoton_Emission_History.
		*/
	LbEightByte number_forced_detection_hits;
											
		/*	The total weight of
			 photons that hit the target surface after forced detection attempts. 
			 Such photons are recorded in the gPhoton_Emission_History.
		*/
	double forced_detection_hits_weight;
	
	
		/*	The total number of photons that escaped the
			 attenuation object.
		*/
	LbEightByte total_escaped_photons;
	
	
		/*	The total number of photons saved in the photon
			history file
		*/
	LbEightByte total_detected_photons;
	
	
		/* The total detected photon scatter weight */
	double total_detected_photon_scatter_weight;
	
	
		/* The total detected photon primary weight */
	double total_detected_photon_primary_weight;
	
	
		/* The total detected photon scatter weight squared */
	double detected_photon_scatter_weight_squ;
	
	
		/* The total detected photon primary weight squared */
	double detected_photon_primary_weight_squ;

		/*	Histogram of the detected photons by energy.  The energy will be
			discretized into bins.  Each energy bin will be the total number of
			detected photons with that energy.
		*/
	LbEightByte detected_photon_energy_histogram[MAX_ENERGY_BINS];
	
	
		/*	Histogram of the detected photons by number of scatters.  The detected photons 
			will be tallied into bins depending on the number of times they have
			scattered.
		*/
	LbEightByte detected_photon_number_of_scatters[MAX_SCATTER_BINS];
	
	
		/*	Histogram of the weight of detected photons by energy.
			The energy will be discretized into bins.  Each energy bin will be the
			total weight of detected photons with that energy.
		*/
	double detected_photon_weight_by_energy[MAX_ENERGY_BINS];
	double detected_weight_squ_by_energy[MAX_ENERGY_BINS];
				 
		/*	Histogram of the weight of the detected photons by number of scatters.
		 	The weight of the detect photons will be tallied into bins depending
			on the number of times they have scattered.
		*/
	double detected_photon_weight_by_scatters[MAX_SCATTER_BINS];
	double detected_weight_squ_by_scatters[MAX_SCATTER_BINS];

		/*	Count of invalid decay locations generated. This counter gets incremented
			when a decay occurs within a boundary voxel that goes outside the object cylinder.
			In general the object editor will not put activity in the boundary voxels, but,
			if it does it is possible for decays to occur in the boundary voxel but outside the object.
			These photons are never tracked and do not affect any other statistic.
		*/
	LbEightByte	invalid_photon_decay_locations;
};

/* Global variables */
struct PhoHStat_Statistics	PhoHStat_CurStats;		/* Our statistics */
LbUsFourByte				PhoHStatNumEnergyBins;	/* Number of energy bins in use */
double						PhoHStatQualityFactor;	/* Our quality factory */
double						PhoHStatCompMerit;		/* Computational merit */

/* PROTOTYPES */
Boolean phoHStatWriteToFile(FILE *statFile_ptr);

/*********************************************************************************
*
*			Name:			PhoHStatIncDetectedPhotonStats
*
*			Summary:		Increment statistics for an discarded photon.
*
*			Arguments:
*				PHG_TrackingPhoton	*trackingPhotonPtr - The discarded photon.
*			Function return: None.
*
*********************************************************************************/
void PhoHStatIncDetectedPhotonStats(PHG_TrackingPhoton *trackingPhotonPtr)	
{
	double			combinedWeight;	/* Decay weight * photon weight */
	LbUsFourByte	scatterIndex;	/* Index into scatter table */
	LbUsFourByte	energyIndex;	/* Index into energy table */

	/* Increment count of detected photons */
	PhoHStat_CurStats.total_detected_photons += 1;

	/* Compute and record detected photon weight */
	if (trackingPhotonPtr->num_of_scatters > 0) {
	
		/* Compute weight */
		combinedWeight = (trackingPhotonPtr->decay_weight * 
			trackingPhotonPtr->photon_scatter_weight);
			
		PhoHStat_CurStats.total_detected_photon_scatter_weight += combinedWeight;
		
		PhoHStat_CurStats.detected_photon_scatter_weight_squ += 
			PHGMATH_Square(combinedWeight);
	}
	else {
		combinedWeight = 
			(trackingPhotonPtr->decay_weight * trackingPhotonPtr->photon_primary_weight);
		
		PhoHStat_CurStats.total_detected_photon_primary_weight += combinedWeight;
		PhoHStat_CurStats.detected_photon_primary_weight_squ += 
			PHGMATH_Square(combinedWeight);
	}
	
	/* Compute scatter index */
	if ((scatterIndex = trackingPhotonPtr->num_of_scatters) >= MAX_SCATTER_BINS)
		scatterIndex = MAX_SCATTER_BINS - 1;

	/* Compute energy index */
	{
		if ((energyIndex = (LbUsFourByte) (trackingPhotonPtr->energy/ENERGY_BIN_SIZE))
				>= PhoHStatNumEnergyBins) {
			
			energyIndex = PhoHStatNumEnergyBins - 1;
		}
		
		/*	We want the 511 keV bin to contain only primary photons so this check
			will push scattered photons up one bin if they would have fallen
			into the 511 keV bin.
		*/
		if ((trackingPhotonPtr->num_of_scatters != 0) && 
				(energyIndex == (LbUsFourByte) PhgRunTimeParams.PhgNuclide.photonEnergy_KEV/ENERGY_BIN_SIZE)) {
			energyIndex--;	
		}
	}
	
	/* Increment detection count for energy bin */
	PhoHStat_CurStats.detected_photon_energy_histogram[energyIndex] 
		+= 1;
		
	/* Increment detection count for scatter bin */
	PhoHStat_CurStats.detected_photon_number_of_scatters[scatterIndex] 
		+= 1;
	
	/* Increment histogram statistics */
	{
		PhoHStat_CurStats.detected_photon_weight_by_energy[energyIndex] 
			+= combinedWeight;
		
		PhoHStat_CurStats.detected_weight_squ_by_energy[energyIndex] 
			+= PHGMATH_Square(combinedWeight);
		
		PhoHStat_CurStats.detected_photon_weight_by_scatters[scatterIndex] 
			+= combinedWeight;
		
		PhoHStat_CurStats.detected_weight_squ_by_scatters[scatterIndex] 
			+= PHGMATH_Square(combinedWeight);
	}
}

/*********************************************************************************
*
*			Name:			PhoHStatIncTotEscPhotons
*
*			Summary:		Increment statistic for sum of photons that made it
*							out of the object cylinder.
*
*			Arguments:
*
*			Function return: None.
*
******************************************************************************/
void PhoHStatIncTotEscPhotons()	
{

	/* Increment field */
	PhoHStat_CurStats.total_escaped_photons += 1;
}

/*********************************************************************************
*
*			Name:			PhoHStatIncTotInvalPhoLocations
*
*			Summary:		Increment statistic for sum of total invalid photon
*							locations.
*
*			Arguments:
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatIncTotInvalPhoLocations()	
{

	/* Increment field */
	PhoHStat_CurStats.invalid_photon_decay_locations += 1;
}

/*********************************************************************************
*
*			Name:			PhoHStatIncTotLowEnPhotons
*
*			Summary:		Increment statistic for sum of low energy photons.
*
*			Arguments:
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatIncTotLowEnPhotons()	
{

	/* Increment field */
	PhoHStat_CurStats.total_low_energy_photons += 1;
}

/*********************************************************************************
*
*			Name:			PhoHStatIncTotAbsorbedPhotons
*
*			Summary:		Increment statistic for sum of absorbed photons.
*
*			Arguments:
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatIncTotAbsorbedPhotons(PHG_TrackingPhoton *trPhoton)	
{

	/* Increment count */
	PhoHStat_CurStats.total_absorbed_photons += 1;
	
	/* Increment weight */
	if (trPhoton->num_of_scatters == 0) {
		PhoHStat_CurStats.total_absorbed_photons_weight += (trPhoton->photon_primary_weight
			* trPhoton->decay_weight);
	}
	else {
		PhoHStat_CurStats.total_absorbed_photons_weight += (trPhoton->photon_scatter_weight
			* trPhoton->decay_weight);
	}
	
}

/*********************************************************************************
*
*			Name:			PhoHStatIncTotPrimOnlyScatter
*
*			Summary:		Increment statistic for sum of primary only
*							photons, that scattered.
*
*			Arguments:
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatIncTotPrimOnlyScatter()	
{

	/* Increment field */
	PhoHStat_CurStats.total_prim_only_that_scattered += 1;
}

/*********************************************************************************
*
*			Name:			PhoHStatInitPhgStatistics
*
*			Summary:		Initialize structure for tracking photon statistics.
*
*			Arguments:
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatInitPhgStatistics()	
{
	LbUsFourByte	index;			/* Loop index for initializing arrays */
	FILE			*statFile_ptr;	/* Temp FILE var for checking stat file */
	
	/* Set the number of energy bins we will be using based on the nuclide */
	PhoHStatNumEnergyBins = (LbUsFourByte) (PhgRunTimeParams.PhgNuclide.photonEnergy_KEV /
		ENERGY_BIN_SIZE) + 1;
	
	/* Set all values to initial defaults */
	PhoHStat_CurStats.total_started_photons = 0;
	PhoHStat_CurStats.total_started_prim_photons = 0;
	PhoHStat_CurStats.total_started_prim_only_photons = 0;
	PhoHStat_CurStats.total_prim_only_that_scattered = 0;
	PhoHStat_CurStats.total_started_scat_photons = 0;
	PhoHStat_CurStats.total_prim_photon_starting_weight = 0.0;
	PhoHStat_CurStats.total_scat_photon_starting_weight = 0.0;
	PhoHStat_CurStats.total_low_energy_photons = 0;
	PhoHStat_CurStats.total_absorbed_photons = 0;
	PhoHStat_CurStats.total_absorbed_photons_weight = 0.0;
	PhoHStat_CurStats.total_split_photons = 0;
	PhoHStat_CurStats.total_split_attempts = 0;
	PhoHStat_CurStats.total_rouletted_photons = 0;
	PhoHStat_CurStats.total_roulette_attempts = 0;
	PhoHStat_CurStats.number_forced_detection_attempts = 0;	
	PhoHStat_CurStats.forced_detection_attempt_weight = 0.0;
	PhoHStat_CurStats.number_forced_detection_hits = 0;
	PhoHStat_CurStats.forced_detection_hits_weight = 0.0;
	PhoHStat_CurStats.total_escaped_photons = 0;
	PhoHStat_CurStats.total_detected_photons = 0;
	PhoHStat_CurStats.total_detected_photon_scatter_weight = 0.0;
	PhoHStat_CurStats.total_detected_photon_primary_weight = 0.0;
	PhoHStat_CurStats.detected_photon_primary_weight_squ = 0.0;
	PhoHStat_CurStats.detected_photon_scatter_weight_squ = 0.0;
	PhoHStat_CurStats.invalid_photon_decay_locations = 0;
	
	/* Iinitialize arrays */
	{
		for (index=0; index < PhoHStatNumEnergyBins; index++) {
		  PhoHStat_CurStats.detected_photon_energy_histogram[index] = 0;
		  PhoHStat_CurStats.detected_photon_weight_by_energy[index] = 0.0;
		  PhoHStat_CurStats.detected_weight_squ_by_energy[index] = 0.0;
		}
	
		for (index=0; index < MAX_SCATTER_BINS; index++) {
			PhoHStat_CurStats.detected_photon_number_of_scatters[index] = 0;
			PhoHStat_CurStats.detected_photon_weight_by_scatters[index] = 0.0;
			PhoHStat_CurStats.detected_weight_squ_by_scatters[index] = 0.0;
		}
		
	}
	
	/* Attempt to create the file just to verify it can be done */
	if (PhgRunTimeParams.PhgPhoHStatFilePath[0] != '\0') {
		if ((statFile_ptr = LbFlFileOpen(PhgRunTimeParams.PhgPhoHStatFilePath,"w")) == (FILE *) NULL) {
		
			/* Abort the simulation */
			PhgAbort("\nERROR: Unable to open statistics file (PhoHStatInitPhgStatistics)!\n\n",
				false);
		}
		else {
			/* Close the stat file */
			fclose(statFile_ptr);
		}
	}
}
/*********************************************************************************
*
*			Name:			PhoHStatUpdateForDetAttemptWeight
*
*			Summary:		Increment statistic for sum of photon weight
*							of forced detection attempts and number of attempts.
*
*			Arguments:
*				double		attemptedPhoton_Weight	- Weight of photon.
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatUpdateForDetAttemptWeight(double attemptedPhoton_Weight)	
{

	/* Increment fields */
	PhoHStat_CurStats.forced_detection_attempt_weight += attemptedPhoton_Weight;
	(PhoHStat_CurStats.number_forced_detection_attempts)++;
}

/*********************************************************************************
*
*			Name:			PhoHStatUpdateForDetHitWeight
*
*			Summary:		Increment statistic for sum of photon weight
*							of forced detection hits and number of hits.
*
*			Arguments:
*				double		hitPhoton_Weight	- Weight of photon.
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatUpdateForDetHitWeight(double hitPhoton_Weight)	
{

	/* Increment fields */
	PhoHStat_CurStats.forced_detection_hits_weight += hitPhoton_Weight;
	(PhoHStat_CurStats.number_forced_detection_hits)++;
}

/*********************************************************************************
*
*			Name:			PhoHStatUpdateRoulettedPhotons
*
*			Summary:		Increment statistic for sum of rouletted photons.
*
*			Arguments:
*				LbUsTwoByte	numRouletted - Number of photons rouletted (0 or 1)
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatUpdateRoulettedPhotons(LbUsTwoByte	numRouletted)	
{

	/* Increment fields */
	PhoHStat_CurStats.total_rouletted_photons += numRouletted;
	PhoHStat_CurStats.total_roulette_attempts += 1;
}

/*********************************************************************************
*
*			Name:			PhoHStatIncTotSplitPhotons
*
*			Summary:		Increment statistics for sum of split photons.
*
*			Arguments:
*				LbUsTwoByte	numSplits	- The number of splits that occured.
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatUpdateSplitPhotons(LbUsTwoByte numSplits)	
{

	/* Increment fields */
	PhoHStat_CurStats.total_split_photons += numSplits;
	PhoHStat_CurStats.total_split_attempts += 1;
}

/*********************************************************************************
*
*			Name:			PhoHStatUpdateStartedPhotons
*
*			Summary:		Increment statistic for sum of photon weight
*							of started photons and number of started photons.
*
*			Arguments:
*				PHG_TrackingPhoton *trackingPhotonPtr - Started photon.
*
*			Function return: None.
*
*********************************************************************************/
void PhoHStatUpdateStartedPhotons(PHG_TrackingPhoton *trackingPhotonPtr)	
{
	double	scatter_weight;	/* Decay weight * photon weight */
	double	primary_weight;	/* Decay weight * photon weight */
	

	/* Increment fields */
	(PhoHStat_CurStats.total_started_photons)++;
	
	/* See if primary photon */
	if (PHG_IsTrackAsPrimary(trackingPhotonPtr)) {
		primary_weight = trackingPhotonPtr->decay_weight *
			trackingPhotonPtr->photon_primary_weight;
		
		PhoHStat_CurStats.total_prim_photon_starting_weight += primary_weight;
		(PhoHStat_CurStats.total_started_prim_photons)++;
		
		/* Update for track as primary only */
		if (!PHG_IsTrackAsScatter(trackingPhotonPtr))
			(PhoHStat_CurStats.total_started_prim_only_photons)++;
	}
	
	/* See if track as scatter */
	if (PHG_IsTrackAsScatter(trackingPhotonPtr)) {
		scatter_weight = trackingPhotonPtr->decay_weight *
			trackingPhotonPtr->photon_scatter_weight;
		
		PhoHStat_CurStats.total_scat_photon_starting_weight += scatter_weight;
		(PhoHStat_CurStats.total_started_scat_photons)++;
	}
}

/*********************************************************************************
*
*			Name:			PhoHStatWrite
*
*			Summary:		Write out the statistics.
*
*			Arguments:
*
*			Function return: True unles an error occurs.
*
*********************************************************************************/
Boolean PhoHStatWrite()	
{
	Boolean		okay = false;		/* Succes flag */
	FILE		*statFile_ptr;		/* Output file pointer */
	
	do { /* Proces Loop */
	
		/* See if statistics file was requested */
		if (PhgRunTimeParams.PhgPhoHStatFilePath[0] != '\0') {

			/* Attempt to create the file */
			if ((statFile_ptr = LbFlFileOpen(PhgRunTimeParams.PhgPhoHStatFilePath,"w")) != (FILE *) NULL) {
			
				/* Write to the statistics file */
				if (!phoHStatWriteToFile(statFile_ptr))
					PhgAbort("\nERROR: Unable to write to statistics file (PhoHStatWrite)!\n\n",
						false);
		
				/* Close the stat file */
				fclose(statFile_ptr);
			}
			else {
				
				/* Send alert but don't abort the simulation */
				ErAlert("\nERROR: Unable to open statistics file (PhoHStatWrite)!",
					false);
			}
		}
		okay = true;
	} while (false);
	
	return (okay);
}

/*********************************************************************************
*
*			Name:			PhoHStatWriteToFile
*
*			Summary:		Write out the statistics to file.
*
*			Arguments:
*				FILE	*statFile_ptr	- FILE pointer for statistics file.
*
*			Function return: TRUE unles an error occurs.
*
*********************************************************************************/
Boolean phoHStatWriteToFile(FILE *statFile_ptr)	
{
	Boolean	okay = false;	/* Succes flag */
	double	squareRoot;		/* Computation variable */
	int		i;				/* LCV */
	
	do { /* Proces Loop */
	
		/* Put in the time stamp for the run */
		if (fprintf(statFile_ptr, "Execution occurred on %s.\n\n",
				PhgExecutionDateStr) < 0) {
			break;
		}
		
		/* Write out all of the fields in the statistics structure */
		
		if (fprintf(statFile_ptr, "total_started_photons =\t%lld\n", 
				PhoHStat_CurStats.total_started_photons) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_started_prim_photons =\t%lld\n", 
				PhoHStat_CurStats.total_started_prim_photons) < 0) {
			break;
		}
		
		
		if (fprintf(statFile_ptr, "total_prim_photon_starting_weight =\t%3.5e\n",
				PhoHStat_CurStats.total_prim_photon_starting_weight) < 0) {
			break;
		} 
		
		
		if (fprintf(statFile_ptr, "total_started_scat_photons =\t%lld\n", 
				PhoHStat_CurStats.total_started_scat_photons) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_scat_photon_starting_weight =\t%3.5e\n",
				PhoHStat_CurStats.total_scat_photon_starting_weight) < 0) {
			break;
		} 

		if (fprintf(statFile_ptr, "total_started_prim_only_photons =\t%lld\n",
		        PhoHStat_CurStats.total_started_prim_only_photons) < 0) {
			break;
		}

		if (fprintf(statFile_ptr, "total_prim_only_that_scattered =\t%lld\n",
		        PhoHStat_CurStats.total_prim_only_that_scattered) < 0) {
			break;
		}

		if (fprintf(statFile_ptr, "total_low_energy_photons =\t%lld\n",
				PhoHStat_CurStats.total_low_energy_photons) < 0) {
			break;
		}

		if (fprintf(statFile_ptr, "total_absorbed_photons =\t%lld\n",
				PhoHStat_CurStats.total_absorbed_photons) < 0) {
			break;
		}

		if (fprintf(statFile_ptr, "total_absorbed_photons_weight =\t%2.5e\n",
				PhoHStat_CurStats.total_absorbed_photons_weight) < 0) {
			break;
		}
		
		/* Write out number of invalid photon locations */
		if (fprintf(statFile_ptr, "invalid_photon_decay_locations =\t%lld\n",
				PhoHStat_CurStats.invalid_photon_decay_locations) < 0) {
			break;
		}
														 
		if (fprintf(statFile_ptr, "total_split_photons =\t%lld\n",
				PhoHStat_CurStats.total_split_photons) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_split_attempts =\t%lld\n",
				PhoHStat_CurStats.total_split_attempts) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_rouletted_photons =\t%lld\n",
				PhoHStat_CurStats.total_rouletted_photons) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_roulette_attempts =\t%lld\n",
				PhoHStat_CurStats.total_roulette_attempts) < 0) {
			break;
		}

		if (fprintf(statFile_ptr, "number_forced_detection_attempts =\t%lld\n",
				PhoHStat_CurStats.number_forced_detection_attempts) < 0) {
			break;
		}
			
		if (fprintf(statFile_ptr, "forced_detection_attempt_weight =\t%3.5e\n",
				PhoHStat_CurStats.forced_detection_attempt_weight) < 0) {
			break;
		}
			
		if (fprintf(statFile_ptr, "number_forced_detection_hits =\t%lld\n",
				PhoHStat_CurStats.number_forced_detection_hits) < 0) {
			break;
		}
			
		if (fprintf(statFile_ptr, "forced_detection_hits_weight =\t%3.5e\n",
				PhoHStat_CurStats.forced_detection_hits_weight) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_escaped_photons =\t%lld\n",
				PhoHStat_CurStats.total_escaped_photons) < 0) {
			break;
		}

		if (fprintf(statFile_ptr, "total_detected_photons =\t%lld\n",
				PhoHStat_CurStats.total_detected_photons) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_detected_photon_scatter_weight =\t%3.5e\n",
				PhoHStat_CurStats.total_detected_photon_scatter_weight) < 0) {
			break;
		}
		
		/* Compute square root */
		if (PhoHStat_CurStats.detected_photon_scatter_weight_squ != 0.0) {
			squareRoot = PHGMATH_SquareRoot(PhoHStat_CurStats.detected_photon_scatter_weight_squ);
		}
		else
			squareRoot = 0;
			
		if (fprintf(statFile_ptr, "detected_photon_scatter_weight_squ =\t%3.5e\tsqrt =\t%3.5e\n",
				PhoHStat_CurStats.detected_photon_scatter_weight_squ,
				squareRoot) < 0) {
			break;
		}
		
		if (fprintf(statFile_ptr, "total_detected_photon_primary_weight =\t%3.5e\n",
				PhoHStat_CurStats.total_detected_photon_primary_weight) < 0) {
			break;
		}
		
		/* Compute square root */
		if (PhoHStat_CurStats.detected_photon_primary_weight_squ != 0.0) {
			squareRoot = PHGMATH_SquareRoot(PhoHStat_CurStats.detected_photon_primary_weight_squ);
		}
		else
			squareRoot = 0;
			
		if (fprintf(statFile_ptr, "detected_photon_primary_weight_squ =\t%3.5e\tsqrt =\t%3.5e\n",
				PhoHStat_CurStats.detected_photon_primary_weight_squ,
				PHGMATH_SquareRoot(PhoHStat_CurStats.detected_photon_primary_weight_squ)) < 0) {
			break;
		}

				
		/* Compute the quality factor */
		{
			double	temp;	/* Temp for avoiding div/0 */
			
			/* Assing denominator to temp */
			temp = (PhoHStat_CurStats.total_detected_photons *
					(PhoHStat_CurStats.detected_photon_scatter_weight_squ + 
					PhoHStat_CurStats.detected_photon_primary_weight_squ));
					
			if (temp == 0)
				PhoHStatQualityFactor = 0;
			else
				PhoHStatQualityFactor = (PHGMATH_Square(PhoHStat_CurStats.total_detected_photon_scatter_weight +
					PhoHStat_CurStats.total_detected_photon_primary_weight))/temp;
					
		}
				
		/* Spit out the quality factor */
		if (fprintf(statFile_ptr, "\n\nQuality Factor =\t%3.3e/(%lld * %3.3e) =\t%3.5e\n",
				PHGMATH_Square(PhoHStat_CurStats.total_detected_photon_scatter_weight +
				PhoHStat_CurStats.total_detected_photon_primary_weight),
				PhoHStat_CurStats.total_detected_photons,
				(PhoHStat_CurStats.detected_photon_scatter_weight_squ + 
				PhoHStat_CurStats.detected_photon_primary_weight_squ), 
				PhoHStatQualityFactor) < 0) {
			break;
		}

		if (fprintf(statFile_ptr, "\nDetected photon energy histogram (%ld - 0, %d per bin)\n",
				(unsigned long)PhoHStatNumEnergyBins * ENERGY_BIN_SIZE, ENERGY_BIN_SIZE) < 0) {
			break;
		}
		
		for(i = PhoHStatNumEnergyBins - 1; i >= 0; i--) {
			
			if (fprintf(statFile_ptr, "%lld\t",
					PhoHStat_CurStats.detected_photon_energy_histogram[i]) < 0) {
				
				goto FAIL;
			}
		}
		
				
		if (fprintf(statFile_ptr, "\n\nDetected photon weight by energy (%ld - 0, %d per bin)\n",
				(unsigned long)PhoHStatNumEnergyBins * ENERGY_BIN_SIZE, ENERGY_BIN_SIZE) < 0) {
			break;
		}
		
		for(i = PhoHStatNumEnergyBins - 1; i >= 0; i--) {
		
			if (fprintf(statFile_ptr, "%3.3e\t",
					PhoHStat_CurStats.detected_photon_weight_by_energy[i]) < 0) {
				
				goto FAIL;
			}
		}

		if (fprintf(statFile_ptr, "\n\nDetected photon weight squared by energy (%ld - 0, %d per bin)\n",
				(unsigned long)(PhoHStatNumEnergyBins * ENERGY_BIN_SIZE), ENERGY_BIN_SIZE) < 0) {
			break;
		}
		
		for(i = PhoHStatNumEnergyBins - 1; i >= 0; i--) {
		
			if (fprintf(statFile_ptr, "%3.3e\t",
					PhoHStat_CurStats.detected_weight_squ_by_energy[i]) < 0) {
				
				goto FAIL;
			}
		}
		
		if (fprintf(statFile_ptr, "\n\nDetected photon number of scatters ( 0 - 9 or more)\n") < 0) {
			break;
		}
		
		for(i=0; i < MAX_SCATTER_BINS; i++) {
		
			if (fprintf(statFile_ptr, "%lld\t",
					PhoHStat_CurStats.detected_photon_number_of_scatters[i]) < 0) {
				
				goto FAIL;
			}
		}
				
		
		if (fprintf(statFile_ptr, "\n\nDetected photon weight by scatters ( 0 - 9 or more)\n") < 0) {
			break;
		}
		
		/* Write out scatter bins */
		for(i=0; i < MAX_SCATTER_BINS; i++) {
		
			if (fprintf(statFile_ptr, "%3.3e%c",
					PhoHStat_CurStats.detected_photon_weight_by_scatters[i],
					'\t') < 0) {
					
				goto FAIL;
			}
		}
		
		
		if (fprintf(statFile_ptr, "\n\nDetected photon weight squared by scatters ( 0 - 9 or more)\n") < 0) {
			break;
		}
		
		/* Write out scatter bins */
		for(i=0; i < MAX_SCATTER_BINS; i++) {
		
			if (fprintf(statFile_ptr, "%3.3e%c",
					PhoHStat_CurStats.detected_weight_squ_by_scatters[i],
					'\t') < 0) {
					
				goto FAIL;
			}
		}
		
		okay = true;
		FAIL:;
	} while (false);
	
	/* If we failed, register an error */
	if (!okay) {
		ErStGeneric("Unable to write to statistics file.");
	}
	
	return (okay);
}


/*********************************************************************************
*
*			Name:			PhoHStat_GetTotInvalPhoLocations
*
*			Summary:		Return number of invalid photon locations.
*
*			Arguments:
*
*			Function return: Number of invalid photon locations.
*
*********************************************************************************/
LbUsFourByte PhoHStat_GetTotInvalPhoLocations()	
{

	/* Increment field */
	return (PhoHStat_CurStats.invalid_photon_decay_locations);
}
