#pragma once

namespace Wrappers{

    struct Matrix{
        int data[4][4];
        void init(int value = 5);
        void init_hadamard();
    };

    struct RNSMatrix{
        int data[4][4][4];
        void init();
    };

}