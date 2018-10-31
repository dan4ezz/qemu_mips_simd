#include <iostream>
#include <bitset>
#include <msa.h>

#include "cavlc.h"
#include "msa_cavlc.h"
#include "msa_blocks_cavlc.h"

int main(void)
{
    int16_t arr0[16] = { 0, 3, 0, 0, 0, 1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0 };
    int16_t arr1[16] = { 0, 3, 0, 1, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 };
    int16_t arr2[16] = { 0, -5, 4, 0, 0, -3, 3, 0, 0, 2, 0, -1, 1, 1, 0, 0 };
    int16_t arr3[16] = {12, -10, -8, 6, 6, -5, 4, 4, 3, -3, -3, 2, 2, -1, -1, 1 };
    
    //int16_t arr0[16] = { 16, 12, -14, -10, -7, 8, 0, 6, -6, 5, 0, 5, 1, -1, 0, -1 };
    //int16_t arr1[16] = { 19, 16, -13, -11, -10, 13, -12, 9, 5, -3, 2, -1, 0, 0, 1, 1 };
    //int16_t arr2[16] = { -18, 17, -15, 13, 14, 10, 11, 7, 5, -6, 3, -2, 3, -1, 1, -1 };
    //int16_t arr3[16] = { 25, -19, 20, 21, -23, 16, 9, 7, -11, 8, -7, 1, 1, 0, 1, 0 };
    
    int16_t* blocks[4];
    blocks[0] = arr0; blocks[1] = arr1; blocks[2] = arr2; blocks[3] = arr3;

    for(int i = 0; i < 1000; i++)
        msa_CAVLC_4Blocks(blocks);

    return 0;
}