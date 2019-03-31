    #include <iostream>
#include "mode_functions.h"
#include <msa.h>
#include<cstdio>
using namespace std;

int main(){
    
    int block_size = 4; // 4 or 8 or 16;
    
    /*
     * Если используется размер блока 4 пикселя, то в data.h массив 
     * data должен иметь тип int, для размера блока 8 пикселей массив 
     * data должен иметь тип short, для block_size = 16 - char.
     * Если не изменять тип массива data, то сложность алгоритма не изменится,
     * но результат работы будет некорректен.
     */

    if (block_size == 4){
        int SAD[9];
        
        //Vertical replication mode
        v4i32* mode_vertical_replication_result = mode_vertical_replication(top_pixels, block_size);
        SAD[0] = count_SAD(mode_vertical_replication_result, block_size);
        
        //Horizontal replication mode
        v4i32* mode_horizontal_replication_result = mode_horizontal_replication(left_pixels, block_size);
        SAD[1] = count_SAD(mode_horizontal_replication_result, block_size);
        
        //Mean(DC) mode
        v4i32* mode_mean_result = mode_mean(top_pixels, left_pixels, block_size);
        SAD[2] = count_SAD(mode_mean_result, block_size);
        
        //Diagonal down left(ddl) mode
        v4i32* mode_ddl_result = mode_ddl(top_pixels);
        SAD[3] = count_SAD(mode_ddl_result, block_size);
        
        //Diagonal down right(ddr) mode
        v4i32* mode_ddr_result = mode_ddr(top_pixels, left_pixels, left_top_pixel);
        SAD[4] = count_SAD(mode_ddl_result, block_size);

        //Vertical right(vr) mode
        v4i32* mode_vr_result = mode_vr(top_pixels, left_pixels, left_top_pixel);
        SAD[5] = count_SAD(mode_vr_result, block_size);

        //Horizontal down(hd) mode
        v4i32* mode_hd_result = mode_hd(top_pixels, left_pixels, left_top_pixel);
        SAD[6] = count_SAD(mode_hd_result, block_size);

        //Vertical left(vl) mode
        v4i32* mode_vl_result = mode_vl(top_pixels);
        SAD[7] = count_SAD(mode_vl_result, block_size);

        //Horizontal up(hup) mode
        v4i32* mode_hup_result = mode_hup(left_pixels);
        SAD[8] = count_SAD(mode_hup_result, block_size);
        
        auto res = find_min(SAD, sizeof(SAD)/sizeof(int));
        cout << "Mode with minimum SAD: " << res.indx << ", SAD = " << res.minimum << '\n';
       }
    else if (block_size == 8){
        int SAD[4];

        //Vertical replication mode
        v8i16* mode_vertical_replication_result = mode_vertical_replication_8(top_pixels, block_size);
        SAD[0] = count_SAD(mode_vertical_replication_result, block_size);

        //Horizontal replication mode
        v8i16* mode_horizontal_replication_result = mode_horizontal_replication_8(left_pixels, block_size);
        SAD[1] = count_SAD(mode_horizontal_replication_result, block_size);

        //Mean(DC) mode
        v8i16* mode_mean_result = mode_mean_8(top_pixels, left_pixels, block_size);
        SAD[2] = count_SAD(mode_mean_result, block_size);

        //Plane mode
        v8i16* mode_plane_result = mode_plane_8(top_pixels, left_pixels, left_top_pixel, block_size);
        SAD[3] = count_SAD(mode_plane_result, block_size);

        auto res = find_min(SAD, sizeof(SAD)/sizeof(int));
        cout << "Mode with minimum SAD: " << res.indx << ", SAD = " << res.minimum << '\n';
    }
    else if (block_size == 16){
        int SAD[4];

        //Vertical replication mode
        v16i8* mode_vertical_replication_result = mode_vertical_replication_16(top_pixels, block_size);
        SAD[0] = count_SAD(mode_vertical_replication_result, block_size);

        //Horizontal replication mode
        v16i8* mode_horizontal_replication_result = mode_horizontal_replication_16(left_pixels, block_size);
        SAD[1] = count_SAD(mode_horizontal_replication_result, block_size);

        //Mean(DC) mode
        v16i8* mode_mean_result = mode_mean_16(top_pixels, left_pixels, block_size);
        SAD[2] = count_SAD(mode_mean_result, block_size);

        //Plane mode
        v16i8* mode_plane_result = mode_plane_16(top_pixels, left_pixels, left_top_pixel, block_size);
        SAD[3] = count_SAD(mode_plane_result, block_size);

        auto res = find_min(SAD, sizeof(SAD)/sizeof(int));
        cout << "Mode with minimum SAD: " << res.indx << ", SAD = " << res.minimum << '\n';
    }
    return 0;
}