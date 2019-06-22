#include "tools.h"
#include <iostream>
#include <msa.h>

int Tools::prod(v4i32 arr, int size){
    int res = 1;
    for(int i = 0; i < size; ++i){
        res *= arr[i];
    }
    return res;
}

v4i32 Tools::vec_prod(v4i32 a, v4i32 b, int basis_size){
    v4i32 res = a * b; 
    return res;
}

int Tools::vec_sum(v4i32 a, int size){
    int res = 0;
    for(int i = 0; i < size; ++i){
        res += a[i];
    }
    return res;
}

int Tools::gcd_coef(long long a, long long b){
    int s = 0, old_s = 1;
    int t = 1, old_t = 0;
    int r = b, old_r = a;

    while(r != 0) {
        int quotient = old_r/r;
        int temp = r;
        r = old_r - quotient*r;
        old_r = temp;
        temp = s;
        s = old_s - quotient*s;
        old_s = temp;
        temp = t;
        t = old_t - quotient*t;
        old_t = temp;
    }
    return old_s;
}


void Tools::print_RNSMatrix(Wrappers::RNSMatrix rns_mtx){
    using namespace std;
    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            for(int k = 0; k < 4; ++k){
                cout << rns_mtx.data[i][j][k] << " ";
            }
            cout << std::endl;
        }
    }
}

void Tools::print_Matrix(Wrappers::Matrix mtx){
    using namespace std;
    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            cout << mtx.data[i][j] << " ";
        }
        cout << std::endl;
    }
}

Wrappers::Matrix Tools::transpose_mtx(Wrappers::Matrix mtx){
    Wrappers::Matrix res;
    for(int i = 0; i < 4; ++i)
        for(int j = 0; j < 4; ++j)
            res.data[i][j] = mtx.data[j][i];
    return res;
}