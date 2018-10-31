#include <stdio.h>
#include <string>
#include <msa.h>
#include <iostream>
#include "bitmap_image.hpp"
#include "timer.cpp"

int main() {
	int x = 0, y = 0, k = 0, m = 0, t = 0;
	bitmap_image image("1.bmp");
	bitmap_image imageF("1.bmp");

	int width = image.width();
	int height = image.height();
	
	std::cout << "width = " << width << "; height = " << height << std::endl;

	rgb_t colour;

	int bit = 16;
	
	int n = 100;
	stopwatch sw;

	/*
	float gauss_float[5][5] = {		{ 0.000789, 0.006581, 0.013347, 0.006581, 0.000789 },
									{ 0.006581, 0.054901, 0.111345, 0.054901, 0.006581 },
									{ 0.013347, 0.111345, 0.225821, 0.111345, 0.013347 },
									{ 0.006581, 0.054901, 0.111345, 0.054901, 0.006581 },
									{ 0.000789, 0.006581, 0.013347, 0.006581, 0.000789 } };
	int gauss[5][5];
	for (k = 0; k < 5; k++) {
		for (m = 0; m < 5; m++) {
			gauss[k][m] = (gauss_float[k][m] * pow(2, bit));
			std::cout << gauss[k][m] << std::endl;
		}
	}
	*/
	int gauss_int[5][5] = {	{51	, 431 , 874	 , 431 , 51	},
							{431, 3597, 7297 , 3597, 431},
							{874, 7297, 14799, 7297, 874},
							{431, 3597, 7297 , 3597, 431},
							{51	, 431 , 874	 , 431 , 51	} };
	
	//msa 
	v4i32 colour_vector;
	v4i32 colour_vector_sum;
	v4i32 intermediate_result;
	v4i32 mult;
	v4i32 shift = {bit, bit, bit, 0};

	sw.tick();
	for(int i = 0; i < n; i++){

		for (y = 2; y < width - 2; y++) {
			for (x = 2; x < height - 2; x++) {
					colour_vector_sum[0] = 0;
					colour_vector_sum[1] = 0;
					colour_vector_sum[2] = 0;

				for (k = 0; k < 5; k++) {
					for (m = 0; m < 5; m++) {
						image.get_pixel(y + k - 2, x + m - 2, colour);
						colour_vector[0] = colour.red;
						colour_vector[1] = colour.green;
						colour_vector[2] = colour.blue;
						colour_vector[3] = 0;

						mult[0] = gauss_int[k][m];
						mult[1] = gauss_int[k][m];
						mult[2] = gauss_int[k][m];
						mult[3] = 0;
						
						intermediate_result = __builtin_msa_mulv_w (colour_vector, mult);
						intermediate_result = __builtin_msa_sra_w (intermediate_result, shift);
						colour_vector_sum   = __builtin_msa_addv_w (colour_vector_sum, intermediate_result);
					}
				}
				/*if (colour_vector_sum[0] < 0) colour_vector_sum[0] = 0;
				if (colour_vector_sum[0] > 255) colour_vector_sum[0]= 255;
				if (colour_vector_sum[1] < 0) colour_vector_sum[1] = 0;
				if (colour_vector_sum[1] > 255) colour_vector_sum[1] = 255;
				if (colour_vector_sum[2] < 0) colour_vector_sum[2]= 0;
				if (colour_vector_sum[2] > 255) colour_vector_sum[2] = 255;*/
				imageF.set_pixel(y, x, colour_vector_sum[0], colour_vector_sum[1], colour_vector_sum[2]);
			}
		}
	}
	imageF.save_image("test01.bmp");

	sw.tock();
	std::cout << "int version " << sw.report_ms() /(double)n << "ms.\n";
	sw.reset();

	return 0;
}
