#include <stdio.h>
#include <stdlib.h>
#include <msa.h>
#include <time.h>
#include "my_dct.h"

const int in[64] = {
    140, 144, 147, 140, 140, 155, 179, 175,
    144, 152, 140, 147, 140, 148, 167, 179,
    152, 155, 136, 167, 163, 162, 152, 172,
    168, 145, 156, 160, 152, 155, 136, 160,
    162, 148, 156, 148, 140, 136, 147, 162,
    147, 167, 140, 155, 155, 140, 136, 162,
    136, 156, 123, 167, 162, 144, 140, 147,
    148, 155, 136, 155, 152, 147, 147, 136
};

const int iin[64] = {
    1210, -17, 14, -8, 23, -9, -13, -18,
    20, -34, 26, -9, -10, 10, 13, 6,
    -10, -23, -1, 6, -18, 3, -20, 0,
    -8, -5, 14, -14, -8, -2, -3, 8,
    -3, 9, 7, 1, -11, 17, 18, 15,
    3, -2, -18, 8, 8, -3, 0, -6,
    8, 0, -2, 3, -1, -7, -1, -1,
    0, -7, -2, 1, 1, 4, -6, 0
};

void print_matrix(int* matrix, int h, int w)
{
    int i,j;
    for(i = 0; i < h; i++)
    {
        for(j = 0; j < w; j++)
        {
            printf("%d\t", matrix[i*w+j]);
        }
        printf("\n");
    }
}

int out[64];
int out2[64];

int main(void)
{
//    int* out = (int*)malloc(64*sizeof(int));
//    int* out2 = (int*)malloc(64*sizeof(int));
//    clock_t time_diff;
//    float diff;
   /* 
    time_diff = clock();
    fdct(in, out);
    idct(out, out2);
    time_diff = clock() - time_diff;
    diff = ((float)time_diff)/CLOCKS_PER_SEC;
    print_matrix(out2, 8, 8);
    printf("Time %f\n", diff);
    
    time_diff = clock();
    int_fdct(in, out);
    int_idct(out, out2);
    time_diff = clock() - time_diff;
    diff = ((float)time_diff)/CLOCKS_PER_SEC;
    print_matrix(out2, 8, 8);
    printf("Time %f\n", diff);
    */
    msa_idct_f32(iin, out);
    //msa_idct_i32(out, out2);
    //print_matrix(out, 8, 8);
    //printf("\n");
    //msa_fdct_i64(in, out);
    //msa_idct_i64(out, out2);
    //print_matrix(out2, 8, 8);
    
//    free(out);
//    free(out2);
    return 0;
}