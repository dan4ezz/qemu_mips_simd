#include <string>
#include <cmath>
#include <iostream>
#include <climits>
#include "bitmap_image.hpp"
#include <ctime>

bitmap_image previous_frame("1.bmp");
bitmap_image current_frame("2.bmp");
bitmap_image motion_frame("1.bmp");
image_drawer draw(motion_frame);
rgb_t colour_prev, colour_cur;
long cost_func(int x1, int y1, int x2, int y2, int size){
    long error = 0;
    int i1 = x2;
    int j1 = y2;
    for(int i = x1, i1 = x2; i < x1 + size; i++, i1++){
        for(int j = y1, j1 = y2; j < y1 + size; j++, j1++){
            previous_frame.get_pixel(i, j, colour_prev);
            current_frame.get_pixel(i1, j1, colour_cur);
            error += (abs(colour_prev.red - colour_cur.red)) +
				abs(colour_prev.green - colour_cur.green) +
				abs(colour_prev.blue - colour_cur.blue);
        }
    }
    return error;
}

int main(){
    image_drawer draw(motion_frame);
    draw.pen_color(255, 0, 0);
    int row = previous_frame.width();
    int col = previous_frame.height();
    std::cout << row <<" "<<col << '\n';
    int block_size = 16;
    int p = 8;
    int L = floor(log2(p+1));
    int step_max = pow(2,(L-1));
    int step_size;
    int x, y;
    int cost_col, cost_row;
    long costs[3][3];
		for(int i = 0; i < row; i += block_size){
    	for (int j = 0; j < col - 7; j += block_size){
            x = i;
            y = j;
            if(j!=352) costs[1][1] = cost_func(i, j, i, j, block_size);
            step_size = step_max;
            while(step_size >= 1){
              int dy=0, dx=0;
              for(int l = 0; l < 3; l++){
                for(int k = 0; k < 3; k++){
                  if(l != 1 && k != 1)
                    costs[l][k]=9999999;
                }
              }
                for(int m = -step_size; m <= step_size; m += step_size){
                    for(int n = -step_size; n <= step_size; n += step_size){
			int ref_block_hor = x + m;
                        int ref_block_ver = y + n;
                        if (ref_block_ver < 0 || ref_block_hor < 0 ||
                                ref_block_ver + block_size > col ||
                                ref_block_hor + block_size > row)
                                  continue;
                            cost_row = m/step_size + 1;
                            cost_col = n/step_size + 1;
                        if (cost_row == 1 && cost_col == 1 && j != 352)
                            continue;
                        costs[cost_row][cost_col] = cost_func(i, j, ref_block_hor, ref_block_ver, block_size);
                    }
                }
								double min = 99999;
									for (int a = 0; a < 3; a++){
										for(int b = 0; b < 3; b++){
											if (costs[a][b] < min){
												min = costs[a][b];
												dx = a;
												dy = b;
											}
										}
									}
                if (costs[1][1] == min){
                  dx = 1;
                  dy = 1;
                }
                x = x + (dx - 1)*step_size;
                y = y + (dy - 1)*step_size;
                step_size/=2;
                costs[1][1] = costs[dx][dy];
            }
            draw.line_segment(i, j, x, y);
            }
        }
    motion_frame.save_image("motion_vector.bmp");
		std::cout << "end" << '\n';
  }
