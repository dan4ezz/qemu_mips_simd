#include <stdio.h>
#include <string>
#include <msa.h>
#include <iostream>
#include <climits>
#include "bitmap_image.hpp"
#include "timer.cpp"


	
int max_motion = 8, block = 16;

struct motion_vector
{
	int y;
	int x;
};


bitmap_image previous_frame("1.bmp");
bitmap_image current_frame("2.bmp");
bitmap_image motion_frame("1.bmp");
image_drawer draw(motion_frame);
rgb_t colour_prev, colour_cur;
int width = previous_frame.width();
int height = previous_frame.height();
	
v4u32 SAD(int y, int x){
	v4u32 error;
	if((x > 0) && (y > 0) && (x < width) && (y < height)){
		v4u32 pix_col_cur;
		v4i32 pix_col_prev;
		previous_frame.get_pixel(x, y, colour_prev);
			pix_col_prev[0] = colour_prev.red;
			pix_col_prev[1] = colour_prev.green;
			pix_col_prev[2] = colour_prev.blue;
		current_frame.get_pixel(x, y, colour_cur);
			pix_col_cur[0] = colour_cur.red;
			pix_col_cur[1] = colour_cur.green;
			pix_col_cur[2] = colour_cur.blue;
		
		error = __builtin_msa_subsus_u_w (pix_col_cur, pix_col_prev);
	}
	return error;
}

int main() {
	int x = 0, y = 0, t = 0, i = 0, j = 0;

	draw.pen_color(255, 0, 0);
	v4u32 min_error, error;
	motion_vector motion_vector;
		
	std::cout << "width = " << width << "; height = " << height << std::endl;
	
	int n = 1;
	stopwatch sw;

	sw.tick();
	for(int i = 0; i < n; i++){
		
		for (y = 0; y < height; y = y + block) {
			for (x = 0; x < width; x = x + block){
				for(int i = 0; i < 4; i++){
					min_error[i] = LONG_MAX;	
				}
				motion_vector.y = 0;
				motion_vector.x = 0;
				for (int i = -max_motion; i < max_motion; i = i + 1){
					for (int j = -max_motion; j < max_motion; j = j + 1){
						error = SAD (y + i, x + j);
						if (error[0] < min_error[0]){
							motion_vector.x =  i;
							motion_vector.y =  j;
							min_error = error;
						}
						else if(error[1] < min_error[1]){
							motion_vector.x =  i;
							motion_vector.y =  j;
							min_error = error;
						}
						else if(error[2] < min_error[2]){
							motion_vector.x =  i;
							motion_vector.y =  j;
							min_error = error;
						}
					}
				}
					error = SAD (y, x);
					if (error[0] <= min_error[0]){
						motion_vector.x =  0;
						motion_vector.y =  0;
					}
					else if(error[1] <= min_error[1]){
						motion_vector.x =  0;
						motion_vector.y =  0;
					}
					else if(error[2] <= min_error[2]){
						motion_vector.x =  0;
						motion_vector.y =  0;
					}

          		draw.line_segment(x, y, x + motion_vector.x, y + motion_vector.y);
			}
		}
		
	}

	sw.tock();
	std::cout << "version " << sw.report_ms() /(double)n << "ms.\n";
	sw.reset();
	
	motion_frame.save_image("motion_vector.bmp");
	/*
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++){
			previous_frame.get_pixel(x, y, colour_prev);
			current_frame.get_pixel(x, y, colour_cur);
			colour_sub.red = abs(colour_cur.red - colour_prev.red);
			colour_sub.green = abs(colour_cur.green - colour_prev.green);
			colour_sub.blue = abs(colour_cur.blue - colour_prev.blue);
			sub_frame.set_pixel(x, y, colour_sub);
		}
	}
	sub_frame.save_image("sub_vector.bmp");
*/
	return 0;
}