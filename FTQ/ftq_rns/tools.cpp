#include "tools.h"
#include <iostream>

int Tools::prod(const int* arr, int size){
    int res = 1;
    for(int i = 0; i < size; ++i){
        res *= arr[i];
    }
    return res;
}

int* Tools::vec_prod(const int* a, const int* b, int basis_size){
    int* res = new int[basis_size];
    for(int i = 0; i < basis_size; ++i){
        res[i] = a[i]*b[i];
    }
    return res;
}

int Tools::vec_sum(const int* a, int size){
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

int* Tools::form_rns_vec(Wrappers::RNSMatrix mtx, int row, int col, int basis_size){
    basis_size++;
    int* rns = new int[basis_size];
    for(int i = 0; i < basis_size; ++i){
        rns[i] = mtx.data[row][col][i];
    }
    return rns;
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