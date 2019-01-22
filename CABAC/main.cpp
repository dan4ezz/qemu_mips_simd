#include <cstdlib>
#include <stdio.h>

#include "cabac.h"

void print_arr(uint8_t* arr, int size) {
    for(int i = 0; i < size; i++) 
        printf("%d", (int)arr[i]);
    printf("\n");
}

//пишет биты от старшего к младшему в массив по индексам от 0
// 1101010000110011 -> ptr[0]=11010100 ptr[1]=00110011 
void write_bit(uint8_t* ptr, int idx, uint8_t bit) //idx - bit index 
{
    int i = idx / 8; // byte index;
    if(bit != 0)
        ptr[i] |= 0xF0 >> (idx - 8*i);
}
   
#define ABS(iX) ((iX)>0 ? (iX) : -(iX))

void print_vector_arr(v4i32* vecs, v4i32 idx)
{
    for(int i = 0; i < idx[0]; i++)
        printf("%d ", vecs[i][0]);
    printf("\n");
    
    for(int i = 0; i < idx[1]; i++)
        printf("%d ", vecs[i][1]);
    printf("\n");
    
    for(int i = 0; i < idx[2]; i++)
        printf("%d ", vecs[i][2]);
    printf("\n");
    
    for(int i = 0; i < idx[3]; i++)
        printf("%d ", vecs[i][3]);
    printf("\n");
}

int main(int argc, char** argv) {

    int8_t in[16] = { 9, 0, -5, 3, 0, 0, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t out[256];
    
    int ret = cabac(in, out);
    print_arr(out, ret);
    
    v4i32 msa_in[16];
    v4i32 msa_out[256];
    msa_in[0] = (v4i32){9, 8, -7, 0};
    msa_in[1] = (v4i32){0, 4, 0, -5};
    msa_in[2] = (v4i32){-5, 0, 6, 5};
    msa_in[3] = (v4i32){3, 0, 0, 4};
    msa_in[4] = (v4i32){0, -3, 0, 3};
    msa_in[5] = (v4i32){0, 0, 0, 0};
    msa_in[6] = (v4i32){-1, 0, 3, 0};
    msa_in[7] = (v4i32){0, -2, 0, 0};
    msa_in[8] = (v4i32){1, 1, 0, -1};
    msa_in[9] = (v4i32){0, 0, 0, 1};
    msa_in[10] = (v4i32){0, 0, -1, 0};
    msa_in[11] = (v4i32){0, 0, 0, 0};
    msa_in[12] = (v4i32){0, -1, 0, 0};
    msa_in[13] = (v4i32){0, 0, 0, 0};
    msa_in[14] = (v4i32){0, 0, 0, 0};
    msa_in[15] = (v4i32){0, 0, 0, 0};

    v4i32 msa_ret = msa_cabac_i32(msa_in, msa_out);
    print_vector_arr(msa_out, msa_ret);

    return 0;
}

