#pragma once

#include <iostream>
#include <stdlib.h>
#include <bitset>
#include <msa.h>
#include <stdio.h>
#include "cavlc_tables.h"

using namespace std;

int32_t msa_CavlcParamCal_c(int16_t* pCoffLevel, uint8_t* pRun, int32_t* pLevel, int32_t* pTotalCoeff, int32_t iLastIndex) {
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

__inline__ void calc_uiSuffixLength(v4i32* vec, int32_t* iLevel, int32_t iTrailingOnes, int32_t iTotalCoeffs, int32_t suiSuffixLength)
{
    vec[0][0] = suiSuffixLength;
    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            if(iTrailingOnes == iTotalCoeffs) 
                return;
            if(i == 0 && j == 0)
                j++;
            int32_t iVal = iLevel[iTrailingOnes];
            suiSuffixLength += !suiSuffixLength; //if(uiSuffixLength==0) uiSuffixLength++;
            int32_t iThreshold = 3 << (suiSuffixLength - 1);
            suiSuffixLength += ((iVal > iThreshold) || (iVal < -iThreshold)) && (suiSuffixLength < 6);
            vec[i][j] = suiSuffixLength;
            iTrailingOnes++;
        }
    }
}

int32_t  msa_WriteBlockResidualCavlc(int16_t* pCoffLevel, int32_t iEndIdx = 15, int32_t iCalRunLevelFlag = 1, int32_t iResidualProperty = 0, int8_t iNC = 0) {

    int32_t iLevel[16];
    uint8_t uiRun[16];

    int32_t iTotalCoeffs = 0;
    int32_t iTrailingOnes = 0;
    int32_t iTotalZeros = 0, iZerosLeft = 0;
    uint32_t uiSign = 0;
    int32_t uiSuffixLength = 0;
    int32_t iValue = 0, iZeroLeft;
    int32_t n = 0;
    int32_t i = 0;

    /*Step 1: calculate iLevel and iRun and total 
    * Этот шаг невозможно реализовать на MSA
    */
    int32_t iCount = 0;
    iTotalZeros = msa_CavlcParamCal_c(pCoffLevel, uiRun, iLevel, &iTotalCoeffs, iEndIdx);
    iCount = (iTotalCoeffs > 3) ? 3 : iTotalCoeffs;
    for (i = 0; i < iCount; i++) 
    {
            if (abs(iLevel[i]) == 1) 
            {
                    iTrailingOnes++;
                    uiSign <<= 1;
                    if (iLevel[i] < 0)
                            uiSign |= 1;
            }
            else 
                    break;
    }
    
    /*Step 3: coeff token */
    const uint8_t* upCoeffToken = &g_kuiVlcCoeffToken[g_kuiEncNcMapTable[iNC]][iTotalCoeffs][iTrailingOnes][0];
    iValue = upCoeffToken[0];
    n = upCoeffToken[1];

    if (iTotalCoeffs == 0) {
            //CAVLC_BS_WRITE(n, iValue);
            return 0;
    }

    /* Step 4: */
    /*  trailing */
    n += iTrailingOnes;
    iValue = (iValue << iTrailingOnes) + uiSign;
    //cout << "Trailing Ones Sign: " << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;
    //CAVLC_BS_WRITE(n, iValue);

    /*  levels 
     *  Используем MSA для кодирования коэффициентов с помощью векторизации
     */
    uiSuffixLength = (iTotalCoeffs > 10 && iTrailingOnes < 3) ? 1 : 0;

    v4i32 v_uiSuffixLength[4];
    v4i32 v_iVal;

    calc_uiSuffixLength(v_uiSuffixLength, iLevel, iTrailingOnes, iTotalCoeffs, uiSuffixLength);
    int step;
    for(int i = iTrailingOnes; i < iTotalCoeffs; i+=step)
    {
        if(iTotalCoeffs - i >= 4) 
            step = 4;
        else
            step = iTotalCoeffs - i;

        v_iVal = *(v4i32*)(&iLevel[i]);

        v4i32 v_iLevelCode = (v_iVal - 1) * (1 << 1);
        v4i32 v_uiSign = (v_iLevelCode >> 31);
        v_iLevelCode = (v_iLevelCode ^ v_uiSign) + (v_uiSign << 1);
        if(i == iTrailingOnes && iTrailingOnes < 3)
            v_iLevelCode[0] -= 2;
        
        v4i32 v_iLevelPrefix = v_iLevelCode >> v_uiSuffixLength[(i-iTrailingOnes)/4];
        v4i32 v_iLevelSuffixSize = v_uiSuffixLength[(i-iTrailingOnes)/4];
        v4i32 v_iLevelSuffix = v_iLevelCode - (v_iLevelPrefix << v_uiSuffixLength[(i-iTrailingOnes)/4]);

        //реализация векторного if-else
        v4i32 lp_ge_14, lp_lt_30, sl_eq_0;
        v4i32 lp_ge_15;
        lp_ge_14 = ~__builtin_msa_clti_s_w(v_iLevelPrefix, 14); // !(lp < 14)
        lp_lt_30 = __builtin_msa_clt_s_w(v_iLevelPrefix, (v4i32){30, 30, 30, 30}); 
        sl_eq_0 = __builtin_msa_ceqi_w(v_uiSuffixLength[(i-iTrailingOnes)/4], 0); 

        v4i32 temp = lp_ge_14 & lp_lt_30 & sl_eq_0; //if(iLevelPrefix >= 14 && iLevelPrefix < 30 && uiSuffixLength == 0)
        v_iLevelPrefix += (14 - v_iLevelPrefix) & temp;
        v_iLevelSuffix += (v_iLevelCode - v_iLevelPrefix - v_iLevelSuffix) & temp;
        v_iLevelSuffixSize += (4 - v_iLevelSuffixSize) & temp;

        lp_ge_15 = ~__builtin_msa_clti_s_w(v_iLevelPrefix, 15); // !(lp < 15)
        temp = ~temp & lp_ge_15; //else if (iLevelPrefix >= 15);
        v_iLevelPrefix += (15 - v_iLevelPrefix) & temp;
        v_iLevelSuffix += (v_iLevelCode 
                - (v_iLevelPrefix << v_uiSuffixLength[(i-iTrailingOnes)/4])
                - v_iLevelSuffix) & temp;
        sl_eq_0 = __builtin_msa_ceqi_w(v_uiSuffixLength[(i-iTrailingOnes)/4], 0); //if (uiSuffixLength == 0)
        v_iLevelSuffix -= 15 & temp & sl_eq_0;
        v_iLevelSuffixSize += (12 - v_iLevelSuffixSize) & temp;

        v4i32 n = v_iLevelPrefix + 1 + v_iLevelSuffixSize;
        v4i32 iValue = ((1 << v_iLevelSuffixSize) | v_iLevelSuffix);

        //for(int k = 0; k < step; k++)
        //    cout << "n = " << n[k] << "\tiValue = " << bitset<32>(iValue[k]) << endl;
    }

    /* Step 5: total zeros 
     * Не требует реализации на MSA
     */
    if (iTotalCoeffs < iEndIdx + 1) 
    {
        const uint8_t* upTotalZeros = &g_kuiVlcTotalZeros[iTotalCoeffs][iTotalZeros][0];
        n = upTotalZeros[1];
        iValue = upTotalZeros[0];
        //cout << "Total Zeros(CH):" << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;
    }
    
    iZerosLeft = iTotalZeros;
    for (i = 0; i + 1 < iTotalCoeffs && iZerosLeft > 0; ++i) 
    {
        const uint8_t uirun = uiRun[i];
        iZeroLeft = g_kuiZeroLeftMap[iZerosLeft];
        n = g_kuiVlcRunBefore[iZeroLeft][uirun][1];
        iValue = g_kuiVlcRunBefore[iZeroLeft][uirun][0];
        //cout << "Run-before " << "\tn = " << n << "\tiValue = " << bitset<32>(iValue) << endl;
        iZerosLeft -= uirun;
    }
        
    return 0;
}

