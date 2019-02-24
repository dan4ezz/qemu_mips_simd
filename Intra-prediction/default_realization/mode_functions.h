#ifndef MODE_FUNCTIONS_H
#define MODE_FUNCTIONS_H

#include<cstdlib>

int data[16][16] = {
  {118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120},
  {118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120},
  {118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120},
  {118, 118, 116, 116, 116, 116, 116, 116, 116, 116, 118, 118, 118, 119, 119, 119},
  {118, 117, 115, 115, 115, 115, 114, 114, 116, 116, 116, 118, 118, 118, 119, 119},
  {117, 117, 115, 115, 115, 115, 114, 114, 115, 115, 115, 115, 117, 117, 118, 118},
  {117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 115, 115, 117, 117, 117, 117},
  {117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 115, 115, 115, 117, 117, 117},
  {117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 115, 115, 115, 115, 117},
  {117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 115, 115, 115, 115},
  {117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 114, 114, 115, 115},
  {117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 114, 114, 115, 115},
  {117, 117, 115, 115, 115, 115, 114, 114, 112, 112, 113, 113, 113, 113, 113, 114},
  {117, 117, 115, 115, 115, 115, 114, 114, 112, 112, 113, 113, 113, 113, 113, 113},
  {117, 117, 115, 115, 115, 115, 114, 114, 112, 112, 112, 112, 113, 113, 113, 113},
  {117, 117, 115, 115, 115, 115, 114, 114, 113, 112, 112, 112, 113, 113, 113, 113}
};

int top_pixels[16] = {118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120};
int left_pixels[16] =  {117, 117, 115, 115, 115, 115, 114, 114, 113, 112, 112, 112, 113, 113, 113, 113};
int left_top_pixel = 115;

int result[4][4];

int count_SAD(int* replication_result, int block_size){
  int sad = 0;
  for(int i = 0; i < block_size; ++i)
    for(int j = 0; j < block_size; ++j)
      sad+=abs(data[i][j] - replication_result[i*block_size + j]);
  return sad;
}

int mean(int top_pixels[], int left_pixels[], int block_size){
  int sum = 0;
  for(int i = 0; i < block_size; ++i){
    sum += left_pixels[i];
    sum += top_pixels[i];
  }
  return sum/(2*block_size);
}

auto find_min(int SAD[], int size){
  struct result{
    int minimum;
    int indx;
  };
  int min = SAD[0];
  int min_indx = 0;
  for(int i = 0; i < size; ++i){
    if(SAD[i] < min){
      min = SAD[i];
      min_indx = i;
    }
  }
  return result{min, min_indx};
}

int* mode_vertical_replication(int top_pixels[], int block_size){
  for(int i = 0; i < block_size; ++i)
    for(int j = 0; j < block_size; ++j)
      result[i][j] = top_pixels[i];
  return &result[0][0];
}

int* mode_horizontal_replication(int left_pixels[], int block_size){
  for(int i = 0; i < block_size; ++i)
    for(int j = 0; j < block_size; ++j)
      result[i][j] = left_pixels[i];
  return &result[0][0];
}

int* mode_mean(int top_pixels[], int left_pixels[], int block_size){
  int mean_result = mean(top_pixels, left_pixels, block_size);
  for(int i = 0; i < block_size; ++i)
    for(int j = 0; j < block_size; ++j)
      result[i][j] = mean_result;
  return &result[0][0];
}

int* mode_ddl(int top_pixels[]){
  int a = (top_pixels[0] + 2*top_pixels[1] + top_pixels[2] + 2) / 4;
  int b = (top_pixels[1] + 2*top_pixels[2] + top_pixels[3] + 2) / 4;
  int c = (top_pixels[2] + 2*top_pixels[3] + top_pixels[4] + 2) / 4;
  int d = (top_pixels[3] + 2*top_pixels[4] + top_pixels[5] + 2) / 4;
  int e = (top_pixels[4] + 2*top_pixels[5] + top_pixels[6] + 2) / 4;
  int f = (top_pixels[5] + 2*top_pixels[6] + top_pixels[7] + 2) / 4;
  int g = (top_pixels[6] + 3*top_pixels[7] + 2) / 4;
  result[0][0] = a; result[0][1] = b; result[0][2] = c; result[0][3] = d;
  result[1][0] = b; result[1][1] = c; result[1][2] = d; result[1][3] = e;
  result[2][0] = c; result[2][1] = d; result[2][2] = e; result[2][3] = f;
  result[3][0] = d; result[3][1] = e; result[3][2] = f; result[3][3] = g;
  return &result[0][0];
}

int* mode_ddr(int top_pixels[], int left_pixels[], int left_top_pixel){
  int a = (left_pixels[3] + 2*left_pixels[2] + left_pixels[1] + 2) / 4;
  int b = (left_pixels[2] + 2*left_pixels[1] + left_pixels[0] + 2) / 4;
  int c = (left_pixels[1] + 2*left_pixels[0] + left_top_pixel + 2) / 4;
  int d = (left_pixels[0] + 2*left_top_pixel + top_pixels[0] + 2) / 4;
  int e = (left_top_pixel + 2*top_pixels[0] + top_pixels[1] + 2) / 4;
  int f = (top_pixels[0] + 2*top_pixels[1] + top_pixels[2] + 2) / 4;
  int g = (top_pixels[0] + 2*top_pixels[2] + top_pixels[3] + 2) / 4;
  result[0][0] = d; result[0][1] = e; result[0][2] = f; result[0][3] = g;
  result[1][0] = c; result[1][1] = d; result[1][2] = e; result[1][3] = f;
  result[2][0] = b; result[2][1] = c; result[2][2] = d; result[2][3] = e;
  result[3][0] = a; result[3][1] = b; result[3][2] = c; result[3][3] = d;
  return &result[0][0];
}

int* mode_vr(int top_pixels[], int left_pixels[], int left_top_pixel){
  int a = (left_top_pixel + top_pixels[0] + 1) / 2;
  int b = (top_pixels[0] + top_pixels[1] + 1) / 2;
  int c = (top_pixels[1] + top_pixels[2] + 1) / 2;
  int d = (top_pixels[2] + top_pixels[3] + 1) / 2;
  int e = (left_pixels[0] + 2*left_top_pixel + top_pixels[0] + 2) / 4;
  int f = (left_top_pixel + 2*top_pixels[0] + top_pixels[1] + 2) / 4;
  int g = (top_pixels[0] + 2*top_pixels[1] + top_pixels[2] + 2) / 4;
  int h = (top_pixels[1] + 2*top_pixels[2] + top_pixels[3] + 2) / 4;
  int i = (left_top_pixel + 2*left_pixels[0] + left_pixels[1] + 2) / 4;
  int j = (left_pixels[0] + 2*left_pixels[1] + left_pixels[2] + 2) / 4;
  result[0][0] = a; result[0][1] = b; result[0][2] = c; result[0][3] = d;
  result[1][0] = e; result[1][1] = f; result[1][2] = g; result[1][3] = h;
  result[2][0] = i; result[2][1] = a; result[2][2] = b; result[2][3] = c;
  result[3][0] = j; result[3][1] = e; result[3][2] = f; result[3][3] = g;
  return &result[0][0];
}

int* mode_hd(int top_pixels[], int left_pixels[], int left_top_pixel){
  int a = (left_top_pixel + left_pixels[0] + 1) / 2;
  int b = (left_pixels[0] + 2*left_top_pixel + top_pixels[0] + 2) / 4;
  int c = (left_top_pixel + 2*top_pixels[0] + top_pixels[1] + 2) / 4;
  int d = (top_pixels[0] + 2*top_pixels[1] + top_pixels[2] + 2) / 4;
  int e = (left_pixels[0] + left_pixels[1] + 1) / 2;
  int f = (left_top_pixel + 2*left_pixels[0] + left_pixels[1] + 2) / 4;
  int g = (left_pixels[1] + left_pixels[2] + 1) / 2;
  int h = (left_pixels[0] + 2*left_pixels[1] + left_pixels[2] + 2) / 4;
  int i = (left_pixels[2] + left_pixels[3] + 1) / 2;
  int j = (left_pixels[1] + 2*left_pixels[2] + left_pixels[3] + 2) / 4;
  result[0][0] = a; result[0][1] = b; result[0][2] = c; result[0][3] = d;
  result[1][0] = e; result[1][1] = f; result[1][2] = a; result[1][3] = b;
  result[2][0] = g; result[2][1] = h; result[2][2] = e; result[2][3] = f;
  result[3][0] = i; result[3][1] = j; result[3][2] = g; result[3][3] = h;
  return &result[0][0];
}

int* mode_vl(int top_pixels[]){
  int a = (top_pixels[0] + top_pixels[1] + 1) / 2;
  int b = (top_pixels[1] + top_pixels[2] + 1) / 2;
  int c = (top_pixels[2] + top_pixels[3] + 1) / 2;
  int d = (top_pixels[3] + top_pixels[4] + 1) / 2;
  int e = (top_pixels[4] + top_pixels[5] + 1) / 2;
  int f = (top_pixels[0] + 2*top_pixels[1] + top_pixels[2] + 2) / 4;
  int g = (top_pixels[1] + 2*top_pixels[2] + top_pixels[3] + 2) / 4;
  int h = (top_pixels[2] + 2*top_pixels[3] + top_pixels[4] + 2) / 4;
  int i = (top_pixels[3] + 2*top_pixels[4] + top_pixels[5] + 2) / 4;
  int j = (top_pixels[4] + 2*top_pixels[5] + top_pixels[6] + 2) / 4;
  result[0][0] = a; result[0][1] = b; result[0][2] = c; result[0][3] = d;
  result[1][0] = f; result[1][1] = g; result[1][2] = h; result[1][3] = i;
  result[2][0] = b; result[2][1] = c; result[2][2] = d; result[2][3] = e;
  result[3][0] = g; result[3][1] = h; result[3][2] = i; result[3][3] = j;
  return &result[0][0];
}

int* mode_hup(int left_pixels[]){
  int a = (left_pixels[0] + left_pixels[1] + 1) / 2;
  int b = (left_pixels[0] + 2*left_pixels[1] + left_pixels[2] + 2) / 4;
  int c = (left_pixels[1] + left_pixels[2] + 1) / 2;
  int d = (left_pixels[1] + 2*left_pixels[2] + left_pixels[3] + 2) / 4;
  int e = (left_pixels[2] + left_pixels[3] + 1) / 2;
  int f = (left_pixels[2] + 3*left_pixels[3] + 2) / 4;
  int g = left_pixels[3];
  result[0][0] = a; result[0][1] = b; result[0][2] = c; result[0][3] = d;
  result[1][0] = c; result[1][1] = d; result[1][2] = e; result[1][3] = f;
  result[2][0] = e; result[2][1] = f; result[2][2] = g; result[2][3] = g;
  result[3][0] = g; result[3][1] = g; result[3][2] = g; result[3][3] = g;
  return &result[0][0];
}

int* mode_plane(int top_pixels[], int left_pixels[], int left_top_pixel, int block_size){
  int H_bar = (top_pixels[8] - top_pixels[6]) + 2*(top_pixels[9] - top_pixels[5])+ 3*(top_pixels[10]
      - top_pixels[4]) + 4*(top_pixels[11] - top_pixels[3]) + 5*(top_pixels[12] - top_pixels[2])
      + 6*(top_pixels[13] - top_pixels[1]) + 7*(top_pixels[14] - top_pixels[0]) + 8*(top_pixels[15]
      - left_top_pixel);

  int V_bar = (left_pixels[8] - left_pixels[5]) + 2*(left_pixels[9] - left_pixels[5]) + 3*(left_pixels[10]
      - left_pixels[4]) + 4*(left_pixels[11] - left_pixels[3]) + 5*(left_pixels[12] - left_pixels[2])
      + 6*(left_pixels[13] - left_pixels[1]) + 7*(left_pixels[14] - left_pixels[0])
      + 8*(left_pixels[15]- left_top_pixel);

  int H = (5*H_bar + 32) / 64;
  int V = (5*V_bar + 32) / 64;

  int a = 16 * (left_pixels[15] + top_pixels[15] + 1) - 7*(V + H);

  for(int i = 0; i < block_size; ++i){
    for(int j = 0; j < block_size; ++j){
      int b = a + V*j + H*i;
      result[i][j] = b/32;
    }
  }
  return &result[0][0];
}

#endif /*MODE_FUNCTIONS_H*/
