/*******************************************************************************************
 
			(c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2017] ++
			
			(c) Copyright Nexus Developers 2014 - 2017
			
			http://www.opensource.org/licenses/mit-license.php
  
*******************************************************************************************/

#include "core.h"

using namespace std;

namespace Core
{
	
	/* Trust Retargeting: Modulate Difficulty based on production rate. */
	unsigned int RetargetTrust(const CBlockIndex* pindex, bool output)
	{

		/* Get Last Block Index [1st block back in Channel]. **/
		const CBlockIndex* pindexFirst = GetLastChannelIndex(pindex, 0);
		if (pindexFirst->pprev == NULL)
			return bnProofOfWorkStart[0].GetCompact();
			
			
		/* Get Last Block Index [2nd block back in Channel]. */
		const CBlockIndex* pindexLast = GetLastChannelIndex(pindexFirst->pprev, 0);
		if (pindexLast->pprev == NULL)
			return bnProofOfWorkStart[0].GetCompact();
		
		
		/* Get the Block Time and Target Spacing. */
		int64 nBlockTime = GetWeightedTimes(pindexFirst, 5);
		int64 nBlockTarget = STAKE_TARGET_SPACING;
		
		
		/* The Upper and Lower Bound Adjusters. */
		int64 nUpperBound = nBlockTarget;
		int64 nLowerBound = nBlockTarget;
		
			
		/* If the time is above target, reduce difficulty by modular
			of one interval past timespan multiplied by maximum decrease. */
		if(nBlockTime >= nBlockTarget)
		{
			/* Take the Minimum overlap of Target Timespan to make that maximum interval. */
			uint64 nOverlap = (uint64)min((nBlockTime - nBlockTarget), (nBlockTarget * 2));
				
			/* Get the Mod from the Proportion of Overlap in one Interval. */
			double nProportions = (double)nOverlap / (nBlockTarget * 2);
				
			/* Get Mod from Maximum Decrease Equation with Decimal portions multiplied by Propotions. */
			double nMod = 1.0 - (0.15 * nProportions);
			nLowerBound = nBlockTarget * nMod;
		}
			
			
		/* If the time is below target, increase difficulty by modular
			of interval of 1 - Block Target with time of 1 being maximum increase */
		else
		{
			/* Get the overlap in reference from Target Timespan. */
			uint64 nOverlap = nBlockTarget - nBlockTime;
				
			/* Get the mod from overlap proportion. Time of 1 will be closest to mod of 1. */
			double nProportions = (double) nOverlap / nBlockTarget;
				
			/* Get the Mod from the Maximum Increase Equation with Decimal portion multiplied by Proportions. */
			double nMod = 1.0 + (nProportions * 0.075);
			nLowerBound = nBlockTarget * nMod;
		}
			
			
		/* Get the Difficulty Stored in Bignum Compact. */
		CBigNum bnNew;
		bnNew.SetCompact(pindexFirst->nBits);
		
		
		/* Change Number from Upper and Lower Bounds. */
		bnNew *= nUpperBound;
		bnNew /= nLowerBound;
		
		
		/* Don't allow Difficulty to decrease below minimum. */
		if (bnNew > bnProofOfWorkLimit[0])
			bnNew = bnProofOfWorkLimit[0];
			
			
		/* Verbose Debug Output. */
		if(GetArg("-verbose", 0) >= 1 && output)
		{
			int64 nDays, nHours, nMinutes;
			GetChainTimes(GetChainAge(pindexFirst->GetBlockTime()), nDays, nHours, nMinutes);
			
			printf("CHECK[POS] weighted time=%"PRId64" actual time =%"PRId64"[%f %%]\n\tchain time: [%"PRId64" / %"PRId64"]\n\tdifficulty: [%f to %f]\n\tPOS height: %"PRId64" [AGE %"PRId64" days, %"PRId64" hours, %"PRId64" minutes]\n\n", 
			nBlockTime, max(pindexFirst->GetBlockTime() - pindexLast->GetBlockTime(), (int64) 1), ((100.0 * nLowerBound) / nUpperBound), nBlockTarget, nBlockTime, GetDifficulty(pindexFirst->nBits, 0), GetDifficulty(bnNew.GetCompact(), 0), pindexFirst->nChannelHeight, nDays, nHours, nMinutes);
		}
		
		return bnNew.GetCompact();
	}
	
	
	
	/* Prime Retargeting: Modulate Difficulty based on production rate. */
	unsigned int RetargetPrime(const CBlockIndex* pindex, bool output)
	{
		
		/* Get Last Block Index [1st block back in Channel]. */
		const CBlockIndex* pindexFirst = GetLastChannelIndex(pindex, 1);
		if (!pindexFirst->pprev)
			return bnProofOfWorkStart[1].getuint();
			
		
		/* Get Last Block Index [2nd block back in Channel]. */
		const CBlockIndex* pindexLast = GetLastChannelIndex(pindexFirst->pprev, 1);
		if (!pindexLast->pprev)
			return bnProofOfWorkStart[1].getuint();
		
		
		/* Standard Time Proportions */
		int64 nBlockTime = ((pindex->nVersion >= 4) ? GetWeightedTimes(pindexFirst, 5) : max(pindexFirst->GetBlockTime() - pindexLast->GetBlockTime(), (int64) 1));
		int64 nBlockTarget = nTargetTimespan;
		
		
		/* Chain Mod: Is a proportion to reflect outstanding released funds. Version 1 Deflates difficulty slightly
			to allow more blocks through when blockchain has been slow, Version 2 Deflates Target Timespan to lower the minimum difficulty. 
			This helps stimulate transaction processing while helping get the Nexus production back on track */
		double nChainMod = GetFractionalSubsidy(GetChainAge(pindexFirst->GetBlockTime()), 0, ((pindex->nVersion >= 3) ? 40.0 : 20.0)) / (pindexFirst->nReleasedReserve[0] + 1);
		nChainMod = min(nChainMod, 1.0);
		nChainMod = max(nChainMod, (pindex->nVersion == 1) ? 0.75 : 0.5);
		
		
		/* Enforce Block Version 2 Rule. Chain mod changes block time requirements, not actual mod after block times. */
		if(pindex->nVersion >= 2)
			nBlockTarget *= nChainMod;
		
		
		/* These figures reduce the increase and decrease max and mins as difficulty rises
			this is due to the time difference between each cluster size [ex. 1, 2, 3] being 50x */
		double nDifficulty = GetDifficulty(pindexFirst->nBits, 1);
		
		
		/* The Mod to Change Difficulty. */
		double nMod = 1.0;
		
		
		/* Handle for Version 3 Blocks. Mod determined by time multiplied by max / min. */
		if(pindex->nVersion >= 3)
		{

			/* If the time is above target, reduce difficulty by modular
				of one interval past timespan multiplied by maximum decrease. */
			if(nBlockTime >= nBlockTarget)
			{
				/* Take the Minimum overlap of Target Timespan to make that maximum interval. */
				uint64 nOverlap = (uint64)min((nBlockTime - nBlockTarget), (nBlockTarget * 2));
				
				/* Get the Mod from the Proportion of Overlap in one Interval. */
				double nProportions = (double)nOverlap / (nBlockTarget * 2);
				
				/* Get Mod from Maximum Decrease Equation with Decimal portions multiplied by Propotions. */
				nMod = 1.0 - (nProportions * (0.5 / ((nDifficulty - 1) * 5.0)));
			}
			
			/* If the time is below target, increase difficulty by modular
				of interval of 1 - Block Target with time of 1 being maximum increase */
			else
			{
				/* Get the overlap in reference from Target Timespan. */
				uint64 nOverlap = nBlockTarget - nBlockTime;
				
				/* Get the mod from overlap proportion. Time of 1 will be closest to mod of 1. */
				double nProportions = (double) nOverlap / nBlockTarget;
				
				/* Get the Mod from the Maximum Increase Equation with Decimal portion multiplied by Proportions. */
				nMod = 1.0 + (nProportions * (0.125 / ((nDifficulty - 1) * 10.0)));
			}
			
		}
		
		/* Handle for Block Version 2 Difficulty Adjustments. */
		else
		{
			/* Equations to Determine the Maximum Increase / Decrease. */
			double nMaxDown = 1.0 - (0.5 / ((nDifficulty - 1) * ((pindex->nVersion == 1) ? 10.0 : 25.0)));
			double nMaxUp = (0.125 / ((nDifficulty - 1) * 50.0)) + 1.0;
			
			/* Block Modular Determined from Time Proportions. */
			double nBlockMod = (double) nBlockTarget / nBlockTime; 
			nBlockMod = min(nBlockMod, 1.125);
			nBlockMod = max(nBlockMod, 0.50);
			
			/* Version 1 Block, Chain Modular Modifies Block Modular. **/
			nMod = nBlockMod;
			if(pindex->nVersion == 1)
				nMod *= nChainMod;
				
			/* Set Modular to Max / Min values. */
			nMod = min(nMod, nMaxUp);
			nMod = max(nMod, nMaxDown);
		}
		
		
		/* If there is a change in difficulty, multiply by mod. */
		nDifficulty *= nMod;

		
		/* Set the Bits of the Next Difficulty. */
		unsigned int nBits = SetBits(nDifficulty);
		if(nBits < bnProofOfWorkLimit[1].getuint())
			nBits = bnProofOfWorkLimit[1].getuint();
			
		/* Console Output */
		if(GetArg("-verbose", 0) >= 1 && output)
		{
			int64 nDays, nHours, nMinutes;
			GetChainTimes(GetChainAge(pindexFirst->GetBlockTime()), nDays, nHours, nMinutes);
			
			printf("RETARGET[CPU] weighted time=%"PRId64" actual time %"PRId64", [%f %%]\n\tchain time: [%"PRId64" / %"PRId64"]\n\treleased reward: %"PRId64" [%f %%]\n\tdifficulty: [%f to %f]\n\tCPU height: %"PRId64" [AGE %"PRId64" days, %"PRId64" hours, %"PRId64" minutes]\n\n", 
			nBlockTime, max(pindexFirst->GetBlockTime() - pindexLast->GetBlockTime(), (int64) 1), nMod * 100.0, nBlockTarget, nBlockTime, pindexFirst->nReleasedReserve[0] / COIN, 100.0 * nChainMod, GetDifficulty(pindexFirst->nBits, 1), GetDifficulty(nBits, 1), pindexFirst->nChannelHeight, nDays, nHours, nMinutes);
		}
		
		return nBits;
	}

	
	
	/* Trust Retargeting: Modulate Difficulty based on production rate. */
	unsigned int RetargetHash(const CBlockIndex* pindex, bool output)
	{
	
		/* Get the Last Block Index [1st block back in Channel]. */
		const CBlockIndex* pindexFirst = GetLastChannelIndex(pindex, 2);
		if (pindexFirst->pprev == NULL)
			return bnProofOfWorkStart[2].GetCompact();
			
			
		/* Get Last Block Index [2nd block back in Channel]. */
		const CBlockIndex* pindexLast = GetLastChannelIndex(pindexFirst->pprev, 2);
		if (pindexLast->pprev == NULL)
			return bnProofOfWorkStart[2].GetCompact();

			
		/* Get the Block Times with Minimum of 1 to Prevent Time Warps. */
		int64 nBlockTime = ((pindex->nVersion >= 4) ? GetWeightedTimes(pindexFirst, 5) : max(pindexFirst->GetBlockTime() - pindexLast->GetBlockTime(), (int64) 1));
		int64 nBlockTarget = nTargetTimespan;
		
		
		/* Get the Chain Modular from Reserves. */
		double nChainMod = GetFractionalSubsidy(GetChainAge(pindexFirst->GetBlockTime()), 0, ((pindex->nVersion >= 3) ? 40.0 : 20.0)) / (pindexFirst->nReleasedReserve[0] + 1);
		nChainMod = min(nChainMod, 1.0);
		nChainMod = max(nChainMod, (pindex->nVersion == 1) ? 0.75 : 0.5);
		
		
		/* Enforce Block Version 2 Rule. Chain mod changes block time requirements, not actual mod after block times. */
		if(pindex->nVersion >= 2)
			nBlockTarget *= nChainMod;
			
			
		/* The Upper and Lower Bound Adjusters. */
		int64 nUpperBound = nBlockTarget;
		int64 nLowerBound = nBlockTarget;
		
			
		/* Handle for Version 3 Blocks. Mod determined by time multiplied by max / min. */
		if(pindex->nVersion >= 3)
		{

			/* If the time is above target, reduce difficulty by modular
				of one interval past timespan multiplied by maximum decrease. */
			if(nBlockTime >= nBlockTarget)
			{
				/* Take the Minimum overlap of Target Timespan to make that maximum interval. */
				uint64 nOverlap = (uint64)min((nBlockTime - nBlockTarget), (nBlockTarget * 2));
				
				/* Get the Mod from the Proportion of Overlap in one Interval. */
				double nProportions = (double)nOverlap / (nBlockTarget * 2);
				
				/* Get Mod from Maximum Decrease Equation with Decimal portions multiplied by Propotions. */
				double nMod = 1.0 - (((pindex->nVersion >= 4) ? 0.15 : 0.75) * nProportions);
				nLowerBound = nBlockTarget * nMod;
			}
			
			/* If the time is below target, increase difficulty by modular
				of interval of 1 - Block Target with time of 1 being maximum increase */
			else
			{
				/* Get the overlap in reference from Target Timespan. */
				uint64 nOverlap = nBlockTarget - nBlockTime;
				
				/* Get the mod from overlap proportion. Time of 1 will be closest to mod of 1. */
				double nProportions = (double) nOverlap / nBlockTarget;
				
				/* Get the Mod from the Maximum Increase Equation with Decimal portion multiplied by Proportions. */
				double nMod = 1.0 + (nProportions * 0.075);
				nLowerBound = nBlockTarget * nMod;
			}
		}
		
		
		/* Handle for Version 2 Difficulty Adjustments. */
		else
		{
			double nBlockMod = (double) nBlockTarget / nBlockTime; 
			nBlockMod = min(nBlockMod, 1.125);
			nBlockMod = max(nBlockMod, 0.75);
			
			/* Calculate the Lower Bounds. */
			nLowerBound = nBlockTarget * nBlockMod;
			
			/* Version 1 Blocks Change Lower Bound from Chain Modular. */
			if(pindex->nVersion == 1)
				nLowerBound *= nChainMod;
			
			/* Set Maximum [difficulty] up to 8%, and Minimum [difficulty] down to 50% */
			nLowerBound = min(nLowerBound, (int64)(nUpperBound + (nUpperBound / 8)));
			nLowerBound = max(nLowerBound, (3 * nUpperBound ) / 4);
		}
			
			
		/* Get the Difficulty Stored in Bignum Compact. */
		CBigNum bnNew;
		bnNew.SetCompact(pindexFirst->nBits);
		
		
		/* Change Number from Upper and Lower Bounds. */
		bnNew *= nUpperBound;
		bnNew /= nLowerBound;
		
		
		/* Don't allow Difficulty to decrease below minimum. */
		if (bnNew > bnProofOfWorkLimit[2])
			bnNew = bnProofOfWorkLimit[2];
			
			
		/* Console Output if Flagged. */
		if(GetArg("-verbose", 0) >= 1 && output)
		{
			int64 nDays, nHours, nMinutes;
			GetChainTimes(GetChainAge(pindexFirst->GetBlockTime()), nDays, nHours, nMinutes);
			
			printf("RETARGET[GPU] weighted time=%"PRId64" actual time %"PRId64" [%f %%]\n\tchain time: [%"PRId64" / %"PRId64"]\n\treleased reward: %"PRId64" [%f %%]\n\tdifficulty: [%f to %f]\n\tGPU height: %"PRId64" [AGE %"PRId64" days, %"PRId64" hours, %"PRId64" minutes]\n\n", 
			nBlockTime, max(pindexFirst->GetBlockTime() - pindexLast->GetBlockTime(), (int64) 1), (100.0 * nLowerBound) / nUpperBound, nBlockTarget, nBlockTime, pindexFirst->nReleasedReserve[0] / COIN, 100.0 * nChainMod, GetDifficulty(pindexFirst->nBits, 2), GetDifficulty(bnNew.GetCompact(), 2), pindexFirst->nChannelHeight, nDays, nHours, nMinutes);
		}
		
		return bnNew.GetCompact();
	}

}