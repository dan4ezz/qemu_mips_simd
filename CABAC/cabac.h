#ifndef CABAC_H
#define CABAC_H

#include <msa.h>
#include <stdint.h>

v4i32 msa_cabac_i32(v4i32* in, v4i32* out) 
{
    v4i32 onesVec = __builtin_msa_fill_w(1);
    v4i32 nonZero = __builtin_msa_fill_w(0);
    
    for(int i = 0; i < 16; i++)
        nonZero += !__builtin_msa_ceqi_w (in[i], 0) & 0x1;
 
    v4i32 bit_idx = __builtin_msa_fill_w(0);
    v4i32 nonZeroCount = __builtin_msa_fill_w(0);
    v4i32 isNonZero;
    v4i32 coeff;
    v4i32 breakVec = __builtin_msa_fill_w(0);
    for(int i = 0; i < 16; i++) {
        coeff = in[i];
        isNonZero = !__builtin_msa_ceqi_w (coeff, 0) & 0x1;
        out[bit_idx[0]][0] = isNonZero[0];
        out[bit_idx[1]][1] = isNonZero[1];
        out[bit_idx[2]][2] = isNonZero[2];
        out[bit_idx[3]][3] = isNonZero[3];
        bit_idx += isNonZero & !breakVec;
        
        nonZeroCount += isNonZero;
        v4i32 isLastSig = __builtin_msa_ceq_w(nonZero, nonZeroCount) & 0x1;
        out[bit_idx[0]][0] = isLastSig[0];
        out[bit_idx[1]][1] = isLastSig[1];
        out[bit_idx[2]][2] = isLastSig[2];
        out[bit_idx[3]][3] = isLastSig[3];
        bit_idx += isNonZero & !breakVec;
        
        out[bit_idx[0]][0] = 0;
        out[bit_idx[1]][1] = 0;
        out[bit_idx[2]][2] = 0;
        out[bit_idx[3]][3] = 0;
        bit_idx += !isNonZero & 0x1 & !breakVec;
        
        breakVec |= isLastSig;
    }
    
    for(int i = 0; i < 16; i++) {
        coeff = in[i];
        
        isNonZero = !__builtin_msa_ceqi_w (coeff, 0) & 0x1;
        v4i32 isNegative =  __builtin_msa_clti_s_w (coeff, 0);
        
        v4i32 absMinus1 = coeff*(((!isNegative & 0x1) << 1) - 1) - 1;
        
        out[bit_idx[0]][0] = absMinus1[0];
        out[bit_idx[1]][1] = absMinus1[1];
        out[bit_idx[2]][2] = absMinus1[2];
        out[bit_idx[3]][3] = absMinus1[3];
        bit_idx += isNonZero;
        
        isNegative &= 0x1;
        out[bit_idx[0]][0] = isNegative[0];
        out[bit_idx[1]][1] = isNegative[1];
        out[bit_idx[2]][2] = isNegative[2];
        out[bit_idx[3]][3] = isNegative[3];
        bit_idx += isNonZero;
    }
    
    return bit_idx;
}

int cabac(int8_t* in, uint8_t* out)
{
    int bit_idx = 0;
    int nonZeroCount = 0;
    for(int i = 0; i < 16; i++)
        nonZeroCount += (in[i] != 0);
    
    int idx = 0, i = 0;
    while(true) {
        if(in[idx] != 0) {
            i++;
            out[bit_idx] = 1;
            bit_idx++;
            if(i == nonZeroCount) {
                out[bit_idx] = 1;
                bit_idx++;
                break;
            }
            else {
                out[bit_idx] = 0;
                bit_idx++;
            }
            idx++;
        }
        else {
            out[bit_idx] = 0;
            bit_idx++;
            idx++;
        }
    }
    
    for(int i = 0; i < 16; i++)
        if(in[i] != 0) {
            if(in[i] > 0) {
                out[bit_idx] = in[i] - 1;
                bit_idx++;
                out[bit_idx] = 0;
            }
            else {
                out[bit_idx] = -in[i] - 1;
                bit_idx++;
                out[bit_idx] = 1;
            }
            bit_idx++;
        }

    return bit_idx;
}

#endif /* CABAC_H */

