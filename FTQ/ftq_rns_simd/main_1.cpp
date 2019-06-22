#include <iostream>
#include "tools.h"
#include "arithmetic.h"
#include "matrices.h"
#include "ftq.h"
#include <cstdlib>

using std::cout;

int main()
{
    v4i32 basis = {3, 7, 31, 1};
    int basis_size = 3;

    Wrappers::Matrix mtx;
    mtx.init();
    FTQ_RNS::FTQ_RNS(mtx, basis, basis_size);
}