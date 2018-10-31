#pragma once

#include <iostream>
#include <stdlib.h>
#include <bitset>

#include "cavlc_tables.h"

using namespace std;


int32_t CavlcParamCal_c(int16_t* pCoffLevel, uint8_t* pRun, int16_t* pLevel, int32_t* pTotalCoeff, int32_t iLastIndex) {
	int32_t iTotalZeros = 0;
	int32_t iTotalCoeffs = 0;

	while (iLastIndex >= 0 && pCoffLevel[iLastIndex] == 0) {
		--iLastIndex;
	}

	while (iLastIndex >= 0) {
		int32_t iCountZero = 0;
		pLevel[iTotalCoeffs] = pCoffLevel[iLastIndex--];

		while (iLastIndex >= 0 && pCoffLevel[iLastIndex] == 0) {
			++iCountZero;
			--iLastIndex;
		}
		iTotalZeros += iCountZero;
		pRun[iTotalCoeffs++] = iCountZero;
	}
	*pTotalCoeff = iTotalCoeffs;
	return iTotalZeros;
}

int32_t  WriteBlockResidualCavlc(int16_t* pCoffLevel, int32_t iEndIdx = 15, int32_t iCalRunLevelFlag = 1, int32_t iResidualProperty = 0, int8_t iNC = 0) {
//	ENFORCE_STACK_ALIGN_1D(int16_t, iLevel, 16, 16)
//	ENFORCE_STACK_ALIGN_1D(uint8_t, uiRun, 16, 16)
	int16_t iLevel[16];
	uint8_t uiRun[16];

	int32_t iTotalCoeffs = 0;
	int32_t iTrailingOnes = 0;
	int32_t iTotalZeros = 0, iZerosLeft = 0;
	uint32_t uiSign = 0;
	int32_t iLevelCode = 0, iLevelPrefix = 0, iLevelSuffix = 0, uiSuffixLength = 0, iLevelSuffixSize = 0;
	int32_t iValue = 0, iThreshold, iZeroLeft;
	int32_t n = 0;
	int32_t i = 0;


	//CAVLC_BS_INIT(pBs);

	/*Step 1: calculate iLevel and iRun and total */
	if (iCalRunLevelFlag) {
		int32_t iCount = 0;
		iTotalZeros = CavlcParamCal_c(pCoffLevel, uiRun, iLevel, &iTotalCoeffs, iEndIdx);
		iCount = (iTotalCoeffs > 3) ? 3 : iTotalCoeffs;
		for (i = 0; i < iCount; i++) {
			if (abs(iLevel[i]) == 1) {
				iTrailingOnes++;
				uiSign <<= 1;
				if (iLevel[i] < 0)
					uiSign |= 1;
			}
			else {
				break;

			}
		}
	}
	/*Step 3: coeff token */
	const uint8_t* upCoeffToken = &g_kuiVlcCoeffToken[g_kuiEncNcMapTable[iNC]][iTotalCoeffs][iTrailingOnes][0];
	iValue = upCoeffToken[0];
	n = upCoeffToken[1];

	if (iTotalCoeffs == 0) {
		//CAVLC_BS_WRITE(n, iValue);

		//CAVLC_BS_UNINIT(pBs);
		//return ENC_RETURN_SUCCESS;
		return 0;
	}
	
	/* Step 4: */
	/*  trailing */
	n += iTrailingOnes;
	iValue = (iValue << iTrailingOnes) + uiSign;
	//cout << "Trailing Ones Sign: " << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;
	//CAVLC_BS_WRITE(n, iValue);

	/*  levels */
	uiSuffixLength = (iTotalCoeffs > 10 && iTrailingOnes < 3) ? 1 : 0;

	for (i = iTrailingOnes; i < iTotalCoeffs; i++) {
		int32_t iVal = iLevel[i];

		iLevelCode = (iVal - 1) * (1 << 1);
		uiSign = (iLevelCode >> 31);
		iLevelCode = (iLevelCode ^ uiSign) + (uiSign << 1);
		iLevelCode -= ((i == iTrailingOnes) && (iTrailingOnes < 3)) << 1;
                
		iLevelPrefix = iLevelCode >> uiSuffixLength;
		iLevelSuffixSize = uiSuffixLength;
		iLevelSuffix = iLevelCode - (iLevelPrefix << uiSuffixLength);
                
		if (iLevelPrefix >= 14 && iLevelPrefix < 30 && uiSuffixLength == 0) {
			iLevelPrefix = 14;
			iLevelSuffix = iLevelCode - iLevelPrefix;
			iLevelSuffixSize = 4;
		}
		else if (iLevelPrefix >= 15) {
			iLevelPrefix = 15;
			iLevelSuffix = iLevelCode - (iLevelPrefix << uiSuffixLength);
			if (uiSuffixLength == 0) {
				iLevelSuffix -= 15;
			}
			iLevelSuffixSize = 12;
		}

		n = iLevelPrefix + 1 + iLevelSuffixSize;
		iValue = ((1 << iLevelSuffixSize) | iLevelSuffix);
		//cout << "Coeff: " << iLevel[i] << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;

		uiSuffixLength += !uiSuffixLength;
		iThreshold = 3 << (uiSuffixLength - 1);
		uiSuffixLength += ((iVal > iThreshold) || (iVal < -iThreshold)) && (uiSuffixLength < 6);

	}

	/* Step 5: total zeros */
	if (iTotalCoeffs < iEndIdx + 1) {
		//if (CHROMA_DC != iResidualProperty) {
		if (!iResidualProperty) {
			const uint8_t* upTotalZeros = &g_kuiVlcTotalZeros[iTotalCoeffs][iTotalZeros][0];
			n = upTotalZeros[1];
			iValue = upTotalZeros[0];
			//cout << "Total Zeros(CH):" << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;
		}
		else {
			const uint8_t* upTotalZeros = &g_kuiVlcTotalZerosChromaDc[iTotalCoeffs][iTotalZeros][0];
			n = upTotalZeros[1];
			iValue = upTotalZeros[0];
			//cout << "Total Zeros:" << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;
		}
	}

	/* Step 6: pRun before */
	iZerosLeft = iTotalZeros;
	for (i = 0; i + 1 < iTotalCoeffs && iZerosLeft > 0; ++i) {
		const uint8_t uirun = uiRun[i];
		iZeroLeft = g_kuiZeroLeftMap[iZerosLeft];
		n = g_kuiVlcRunBefore[iZeroLeft][uirun][1];
		iValue = g_kuiVlcRunBefore[iZeroLeft][uirun][0];
		//cout << "Run-before " << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;
		iZerosLeft -= uirun;
	}

	//CAVLC_BS_UNINIT(pBs);
	//return ENC_RETURN_SUCCESS;
	
	return 0;
}