#pragma once

#include <iostream>
#include <stdlib.h>
#include <bitset>
#include <msa.h>
#include <stdio.h>
#include "cavlc_tables.h"

v4i32 msa_CAVLCParams(int16_t** blocks, v4i32* iLevel, v4i32* iLevelSize, v4i32* uiRun)
{
    v4i32 iTotalZeros = { 0, 0, 0, 0 };
    v4i32 firstNonZero = { 0, 0, 0, 0 };
    *iLevelSize = (v4i32){ 0, 0, 0, 0 };
    for(int i = 15; i >= 0; i--)
    {
        iLevel[15-i]= (v4i32){ 0, 0, 0, 0 };
        uiRun[15-i]= (v4i32){ 0, 0, 0, 0 };
        
        v4i32 val = { blocks[0][i], blocks[1][i], blocks[2][i], blocks[3][i] };
        v4i32 ceqi0 = __builtin_msa_ceqi_w(val, 0);
        firstNonZero |= ~ceqi0;
        iTotalZeros -= ceqi0 & firstNonZero;
        
        iLevel[iLevelSize[0][0]][0] = val[0];
        iLevel[iLevelSize[0][1]][1] = val[1];
        iLevel[iLevelSize[0][2]][2] = val[2];
        iLevel[iLevelSize[0][3]][3] = val[3];
        
        uiRun[iLevelSize[0][0]][0] -= ceqi0[0];
        uiRun[iLevelSize[0][1]][1] -= ceqi0[1];
        uiRun[iLevelSize[0][2]][2] -= ceqi0[2];
        uiRun[iLevelSize[0][3]][3] -= ceqi0[3];
        
        ceqi0 = ~ceqi0;
        *iLevelSize += ceqi0 & 1;
    }
    return iTotalZeros;
}

void msa_CAVLC_4Blocks(int16_t** blocks)
{
    //переводим все в векторы msa
    v4i32 iLevel[16];
    v4i32 iTotalCoeffs;
    v4i32 uiRunAfter[16];
    
    v4i32 iTotalZeros = msa_CAVLCParams(blocks, iLevel, &iTotalCoeffs, uiRunAfter);
    v4i32* uiRun = &uiRunAfter[1]; 
    
    //Trailing Ones
    v4i32 iTrailingOnes = { 0, 0, 0, 0 };
    v4i32 uiSign = { 0, 0, 0, 0 };
    v4i32 nOne = { 0, 0, 0, 0 }; //используется в качестве break, если попадается не +-1
    for(int i = 0; i < 3; i++)
    {
        v4i32 iOne = __builtin_msa_ceqi_w(iLevel[i], 1) | __builtin_msa_ceqi_w(iLevel[i], -1);
        nOne |= ~iOne;
        iTrailingOnes -= iOne & ~nOne;
        uiSign =  uiSign << (1 & iOne & ~nOne);  
        uiSign |= (v4i32)((v4u32)iLevel[i] >> 31) & iOne & ~nOne;
    }
    
    //Кодируем coeff_token по таблицам
    v4i32 iValue, n;
    for(int i = 0; i < 4; i++)
    {
        const uint8_t* upCoeffToken = &g_kuiVlcCoeffToken[g_kuiEncNcMapTable[0]][iTotalCoeffs[i]][iTrailingOnes[i]][0];
        iValue[i] = upCoeffToken[0];
        n[i] = upCoeffToken[1];
    }
    n += iTrailingOnes;
    iValue = (iValue << iTrailingOnes) + uiSign;
    
    //Кодируем ненулевые коэффициенты
    v4i32 uiSuffixLength= { 0, 0, 0, 0 };
    uiSuffixLength -= ~__builtin_msa_clei_s_w(iTotalCoeffs, 10) & __builtin_msa_clti_s_w(iTrailingOnes, 3);
    v4i32 iLevelCode, iLevelPrefix, iLevelSuffix, iLevelSuffixSize, iThreshold;
    //ищем макс и мин коэфф в векторе для уменьшения размера последующего цикла
    int maxTotalCoeffs = 0;
    int minTrailingOnes = 3;
    for(int i = 0; i < 4; i++)
    {
        if(minTrailingOnes > iTrailingOnes[i])
            minTrailingOnes = iTrailingOnes[i];
        if(maxTotalCoeffs < iTotalCoeffs[i])
            maxTotalCoeffs = iTotalCoeffs[i];
    }
    
    for(int i = minTrailingOnes; i < maxTotalCoeffs; i++)
    {
        v4i32 v_i = __builtin_msa_fill_w(i);
        v4i32 iVal = iLevel[i];
      
        iLevelCode = (iVal - 1) * (1 << 1);
        uiSign = (iLevelCode >> 31);
        iLevelCode = (iLevelCode ^ uiSign) + (uiSign << 1);
        iLevelCode -= 2 & (__builtin_msa_ceq_w(iTrailingOnes, v_i) 
                & __builtin_msa_clti_s_w(iTrailingOnes, 3));
        
        iLevelPrefix = iLevelCode >> uiSuffixLength;
	iLevelSuffixSize = uiSuffixLength;
        iLevelSuffix = iLevelCode - (iLevelPrefix << uiSuffixLength);
        
        //if-else
        v4i32 temp = __builtin_msa_ceqi_w(uiSuffixLength, 0) 
                & __builtin_msa_clt_s_w(iLevelPrefix, __builtin_msa_fill_w(30))
                & ~__builtin_msa_clti_s_w(iLevelPrefix, 14);
        iLevelPrefix += (14 - iLevelPrefix) & temp;
        iLevelSuffix += (iLevelCode - iLevelPrefix - iLevelSuffix) & temp;
        iLevelSuffixSize += (4 - iLevelSuffixSize) & temp;
        
        temp = ~temp & ~__builtin_msa_clti_s_w(iLevelPrefix, 15);
        iLevelPrefix += (15 - iLevelPrefix) & temp;
        iLevelSuffix += (iLevelCode 
                - (iLevelPrefix << uiSuffixLength)
                - iLevelSuffix) & temp;
        
        iLevelSuffix -= 15 & temp & __builtin_msa_ceqi_w(uiSuffixLength, 0);
        iLevelSuffixSize += (12 - iLevelSuffixSize) & temp;
        
        n = iLevelPrefix + 1 + iLevelSuffixSize;
	iValue = ((1 << iLevelSuffixSize) | iLevelSuffix);
        
        uiSuffixLength -= __builtin_msa_ceqi_w(uiSuffixLength, 0);
	iThreshold = 3 << (uiSuffixLength - 1);
	uiSuffixLength -= (__builtin_msa_clt_s_w(iThreshold, iVal) | __builtin_msa_clt_s_w(iVal, -iThreshold))
                & __builtin_msa_clti_s_w(uiSuffixLength, 6);
    }
    
    //кодируем totalZeros, использование msa не рационально, т.к. используем поиск в таблице
    for(int i = 0; i < 4; i++)
    {
        const uint8_t* upTotalZeros = &g_kuiVlcTotalZeros[iTotalCoeffs[i]][iTotalZeros[i]][0];
        n[i] = upTotalZeros[1];
        iValue[i] = upTotalZeros[0];
    }
    
    //кодируем run before
    for(int j = 0; j < 4; j++)
    {
        int32_t iZerosLeft = iTotalZeros[j];
	for (int i = 0; i + 1 < iTotalCoeffs[j] && iZerosLeft > 0; ++i) {
		const uint8_t uirun = uiRun[i][j];
		int32_t iZeroLeft = g_kuiZeroLeftMap[iZerosLeft];
		n[j] = g_kuiVlcRunBefore[iZeroLeft][uirun][1];
		iValue[j] = g_kuiVlcRunBefore[iZeroLeft][uirun][0];
		//cout << "Run-before " << "\tn = " << n[j] << "\tiValue = " << bitset<32>(iValue[j]) << endl;
		iZerosLeft -= uirun;
	}
    }
    /* нерациональный вариант с кучей ненужных операций для использования 1 операции msa
    v4i32 iZerosLeft = iTotalZeros;
    v4i32 temp0 = __builtin_msa_clei_s_w(iZerosLeft, 0);
    v4i32 v_i = {0, 0, 0, 0 };
    v4i32 temp1 = __builtin_msa_clt_s_w(v_i + 1, iTotalCoeffs);
    bool isZerosLeft = temp0[0] == 0 || temp0[1] == 0 || temp0[2] == 0 || temp0[3] == 0;
    bool lessTotalCoeffs = temp1[0] == -1 || temp1[1] == -1 || temp1[2] == -1 || temp1[3] == -1;
    while(lessTotalCoeffs && isZerosLeft)
    {
        int i = v_i[0];
        for(int j = 0; j < 4; j++) //поиск в таблице
        {
            const uint8_t uirun = uiRun[i][j];
            int32_t iZeroLeft = g_kuiZeroLeftMap[iZerosLeft[j]];
            n[j] = g_kuiVlcRunBefore[iZeroLeft][uirun][1];
            iValue[j] = g_kuiVlcRunBefore[iZeroLeft][uirun][0];
        }
        n &= ~temp0 & temp1;
        iValue &= ~temp0 & temp1;
    
        iZerosLeft -= uiRun[i];
        v_i += 1;
        temp0 = __builtin_msa_clei_s_w(iZerosLeft, 0);
        temp1 = __builtin_msa_clt_s_w(v_i + 1, iTotalCoeffs);
        isZerosLeft = temp0[0] == 0 || temp0[1] == 0 || temp0[2] == 0 || temp0[3] == 0;
        lessTotalCoeffs = temp1[0] == -1 || temp1[1] == -1 || temp1[2] == -1 || temp1[3] == -1;
    }
    */
}