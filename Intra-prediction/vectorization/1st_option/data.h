/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   data.h
 * Author: qwerty
 *
 * Created on March 21, 2019, 12:00 AM
 */

#ifndef DATA_H
#define DATA_H
#include <cstdint>

int data[256] = {
  118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120,
  118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120,
  118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120,
  118, 118, 116, 116, 116, 116, 116, 116, 116, 116, 118, 118, 118, 119, 119, 119,
  118, 117, 115, 115, 115, 115, 114, 114, 116, 116, 116, 118, 118, 118, 119, 119,
  117, 117, 115, 115, 115, 115, 114, 114, 115, 115, 115, 115, 117, 117, 118, 118,
  117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 115, 115, 117, 117, 117, 117,
  117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 115, 115, 115, 117, 117, 117,
  117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 115, 115, 115, 115, 117,
  117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 115, 115, 115, 115,
  117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 114, 114, 115, 115,
  117, 117, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114, 114, 114, 115, 115,
  117, 117, 115, 115, 115, 115, 114, 114, 112, 112, 113, 113, 113, 113, 113, 114,
  117, 117, 115, 115, 115, 115, 114, 114, 112, 112, 113, 113, 113, 113, 113, 113,
  117, 117, 115, 115, 115, 115, 114, 114, 112, 112, 112, 112, 113, 113, 113, 113,
  117, 117, 115, 115, 115, 115, 114, 114, 113, 112, 112, 112, 113, 113, 113, 113
};

int top_pixels[16] = {118, 118, 116, 116, 116, 116, 116, 116, 117, 117, 119, 119, 120, 120, 120, 120};
int left_pixels[16] =  {117, 117, 115, 115, 115, 115, 114, 114, 113, 112, 112, 112, 113, 113, 113, 113};
int left_top_pixel = 115;

#endif /* DATA_H */
