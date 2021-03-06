//Code to test maximal diviation in integer arithmetic alorithm
#include <bits/stdc++.h>
#include <msa.h>
#include "timer.cpp"

#define SIZE_W 20 
#define SIZE_H 18 

typedef unsigned char byte; 

byte red[SIZE_H][SIZE_W] = {{248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}, {248, 249, 253, 254, 255, 237, 253, 252, 168, 71, 14, 0, 8, 2, 0, 50, 192, 238, 251, 252}}; 
byte green[SIZE_H][SIZE_W] = {{2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}, {2, 5, 1, 0, 0, 14, 115, 233, 255, 247, 245, 255, 251, 166, 37, 0, 0, 15, 5, 0}}; 
byte blue[SIZE_H][SIZE_W] = {{5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}, {5, 0, 0, 10, 13, 0, 0, 19, 6, 1, 45, 132, 229, 255, 238, 255, 255, 217, 106, 27}}; 


void naiveVersion(){
	//std::ofstream fout("naive.txt");
	for(int i = 0; i < SIZE_H; ++i){
		for(int j = 0; j < SIZE_W; ++j){
			byte R, G, B, Y, U, V;
			
			R = red[i][j];
			G = green[i][j];
			B = blue[i][j];

			Y = 0.299 * R + 0.587 * G + 0.114 * B;
			U = 128 + 0.564 * (B - Y);
			V = 128 + 0.713 * (R - Y);

			//fout << +Y << " " << +U << " " << +V << std::endl;
		}
	}
}

void msaVersionYCbCr(){
	//std::ofstream fout("msaZ.txt");
	byte R, G, B, Y, U, V;
	
	v4i32 MUL_Y = {19595, 38469, 7471, 0};
	v4i32 MUL_UV = {46727, 1, 36962, 0};		
	v4i32 SHIFT = {16, 16, 16, 0};

	for(unsigned long cnt = 0; cnt < SIZE_H * SIZE_W; ++cnt){
		R = *(red[0] + cnt);
		G = *(green[0] + cnt);
		B = *(blue[0] + cnt);

		v4i32 RGB = {R, G, B, 0};

		RGB = __builtin_msa_mulv_w(RGB, MUL_Y);
		RGB = __builtin_msa_sra_w(RGB, SHIFT);

		Y = RGB[0] + RGB[1] + RGB[2];

		RGB[0] = R; RGB[2] = B;

		v4i32 temp = {Y, 0, Y, 0};
		temp = __builtin_msa_subs_s_w(RGB, temp);
		temp = __builtin_msa_mulv_w(temp, MUL_UV);
		temp = __builtin_msa_sra_w(temp, SHIFT);
		
		U = 128 + temp[2];
		V = 128 + temp[0];

		//fout << +Y << " " << +U << " " << +V << std::endl;
	}
}

void msaVersionTest(){
	//?????????????????? (SIZE_W*SIZE_H) % 16 ???????????????? ???? ?????????????????? ?? ??????????????????????

	v16u8 *pR = (v16u8*)(&red[0]);
	v16u8 *pG = (v16u8*)(&green[0]);
	v16u8 *pB = (v16u8*)(&blue[0]);

	v16u8 Y, U, V;

	v4i32 tR, tG, tB, tY, tU, tV;
	
	v16u8 *pMax = pR + (SIZE_W * SIZE_H / 16); 
	for(;pR <= pMax; ++pR, ++pG, ++pB){
		for(int i = 0; i < 4; ++i){
			for(int j = 0; j < 4; ++j){
				int index = (i<<2) + j;
				tR[j] = (*pR)[index];
				tG[j] = (*pG)[index];
				tB[j] = (*pB)[index];
			}
			
			tY = (19595 * tR + 38469 * tG + 7471 * tB) >> 16;
			tU = 128 + ((36962 * (tB - tY)) >> 16);
			tV = 128 + ((46727 * (tR - tY)) >> 16);

			for(int j = 0; j < 4; j++){
				int index = (i<<2) + j;
				Y[index] = tY[j];
				U[index] = tU[j];
				V[index] = tV[j];
			}
		}

	}
	
	/* AltVersion
	{
		//?????????????????? ??????????, ?????? ?? ????????????, ???? ????????, ?????? ???????????????????????? ???? Z

		tR[0] = (*pR)[0]; tR[1] = (*pR)[1]; tR[2] = (*pR)[2]; tR[3] = (*pR)[3];
		tG[0] = (*pG)[0]; tG[1] = (*pG)[1]; tG[2] = (*pG)[2]; tG[3] = (*pG)[3];
		tB[0] = (*pB)[0]; tB[1] = (*pB)[1]; tB[2] = (*pB)[2]; tB[3] = (*pB)[3];

		tY = (19595 * tR + 38469 * tG + 7471 * tB) >> 16;
		tU = 128 + ((36962 * (tB - tY)) >> 16);
		tV = 128 + ((46727 * (tR - tY)) >> 16);

		tR[0] = (*pR)[4]; tR[1] = (*pR)[5]; tR[2] = (*pR)[6]; tR[3] = (*pR)[7];
		tG[0] = (*pG)[4]; tG[1] = (*pG)[5]; tG[2] = (*pG)[6]; tG[3] = (*pG)[7];
		tB[0] = (*pB)[4]; tB[1] = (*pB)[5]; tB[2] = (*pB)[6]; tB[3] = (*pB)[7];

		tY = (19595 * tR + 38469 * tG + 7471 * tB) >> 16;
		tU = 128 + ((36962 * (tB - tY)) >> 16);
		tV = 128 + ((46727 * (tR - tY)) >> 16);

		tR[0] = (*pR)[8]; tR[1] = (*pR)[9]; tR[2] = (*pR)[10]; tR[3] = (*pR)[11];
		tG[0] = (*pG)[8]; tG[1] = (*pG)[9]; tG[2] = (*pG)[10]; tG[3] = (*pG)[11];
		tB[0] = (*pB)[8]; tB[1] = (*pB)[9]; tB[2] = (*pB)[10]; tB[3] = (*pB)[11];

		tY = (19595 * tR + 38469 * tG + 7471 * tB) >> 16;
		tU = 128 + ((36962 * (tB - tY)) >> 16);
		tV = 128 + ((46727 * (tR - tY)) >> 16);

		tR[0] = (*pR)[12]; tR[1] = (*pR)[13]; tR[2] = (*pR)[14]; tR[3] = (*pR)[15];
		tG[0] = (*pG)[12]; tG[1] = (*pG)[13]; tG[2] = (*pG)[14]; tG[3] = (*pG)[15];
		tB[0] = (*pB)[12]; tB[1] = (*pB)[13]; tB[2] = (*pB)[14]; tB[3] = (*pB)[15];

		tY = (19595 * tR + 38469 * tG + 7471 * tB) >> 16;
		tU = 128 + ((36962 * (tB - tY)) >> 16);
		tV = 128 + ((46727 * (tR - tY)) >> 16);
	}*/
}

void naiveIntRGB(){
	byte R, G, B, Y, dY, dU, dV;
	
	for(unsigned long cnt = 0; cnt < SIZE_H * SIZE_W; ++cnt){
		Y = *(pY[0] + cnt)
		dY = Y - 128;
		dU = *(pU[0] + cnt) - 128;
		dV = *(pV[0] + cnt) - 128;

		R = Y + ((92242 * dV) >> 16);
		G = Y - ((22643 * dU) >> 16) - ((46983 * dV) >> 16);
		B = Y + ((116589 * dU) >> 16);
	}
	
}

void MSAIntRGB(byte** Y, byte** U, byte** V, int SIZE_W, int SIZE_H){
	//?????????????????? (SIZE_W*SIZE_H) % 16 ???????????????? ???? ?????????????????? ?? ??????????????????????
	v8u16 *pY = (v8u16*)(&Y[0]); 
	v8u16 *pU = (v8u16*)(&U[0]);
	v8u16 *pV = (v8u16*)(&Y[0]);  
	
	v16u8 R, G, B;

	v4i32 tR, tG, tB, tY, tdU, tdV;
	
	v16u8 *pMax = pY + (SIZE_W * SIZE_H / 16); 
	for(;pY <= pMax; ++pY, ++pU, ++pV){
		for(int i = 0; i < 4; ++i){
			for(int j = 0; j < 4; ++j){
				int index = (i<<2) + j;
				tY[j] = (*pY)[index];
				tdU[j] = (*pU)[index] - 128;
				tdV[j] = (*pV)[index] - 128;
			}
			
			tR = tY + ((92242 * tdV) >> 16);
			tG = tY - ((22643 * tdU) >> 16) - ((46983 * tdV) >> 16);
			tB = tY + ((116589 * tdU) >> 16);

			for(int j = 0; j < 4; j++){
				int index = (i<<2) + j;
				R[index] = tR[j];
				G[index] = tG[j];
				B[index] = tB[j];
			}
		}

	}
}

int main(){ 
	int n = 1;
	
	stopwatch sw;

	sw.tick();
	for(int i = 0; i < n; i++){
		naiveVersion();
	}
	
	sw.tock();
	std::cout << "naiveVersion() took " << sw.report_ms() /(double)n << "ms.\n";
	sw.reset();

	
	sw.tick();
	for(int i = 0; i < n; i++){
		msaVersionYCbCr();
	}
	
	sw.tock();
	std::cout << "msaVersionYCbCr() took " << sw.report_ms() /(double)n << "ms.\n";
	sw.reset();
	

	sw.tick();
	for(int i = 0; i < n; i++){
		msaVersionTest();
	}
	
	sw.tock();
	std::cout << "msaVersionTest() took " << sw.report_ms() /(double)n << "ms.\n";
	sw.reset();

	return 0; 
}
