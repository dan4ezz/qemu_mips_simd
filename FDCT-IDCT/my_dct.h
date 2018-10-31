#ifndef MY_DCT_H
#define MY_DCT_H

#include <msa.h>

#define NORM 0.25
#define C 0.70711 
float Cosine[8][8] = {
    {1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000},
    {0.98079, 0.83147, 0.55557, 0.19509, -0.19509, -0.55557, -0.83147, -0.98079},
    {0.92388, 0.38268, -0.38268, -0.92388, -0.92388, -0.38268, 0.38268, 0.92388},
    {0.83147, -0.19509, -0.98079, -0.55557, 0.55557, 0.98079, 0.19509, -0.83147},
    {0.70711, -0.70711, -0.70711, 0.70711, 0.70711, -0.70711, -0.70711, 0.70711},
    {0.55557, -0.98079, 0.19509, 0.83147, -0.83147, -0.19509, 0.98079, -0.55557},
    {0.38268, -0.92388, 0.92388, -0.38268, -0.38268, 0.92388, -0.92388, 0.38268},
    {0.19509, -0.55557, 0.83147, -0.98079, 0.98079, -0.83147, 0.55557, -0.19509}
};
const int size = 8;

//Обычная реализация на центральном процессоре
void fdct_f32(int* in, int* out)
{
    int i, j, x, y;
    float s, ci, cj;
    for(i = 0; i < 8; i++)
    {
        for(j = 0; j < 8; j++)
        {
            
            if(i == 0)
                ci = C;
            else
                ci = 1;
            
            if(j == 0)
                cj = C;
            else
                cj = 1;
            s = 0.0;
            for(x = 0; x < size; x++)
                for(y = 0; y < size; y++)
                    s += Cosine[i][x]*Cosine[j][y]*in[x*size+y];
            s *= ci*cj*NORM;
            out[i*size+j] = (int)s;
        }
    }
}

void idct_f32(int* in, int* out)
{
    int i, j, x, y;
    float s, ax, ay;
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j++)
        {
            s = 0.0;
            for(x = 0; x < size; x++)
            {
                for(y = 0; y < size; y++)
                {
                    if(x == 0)
                        ax = C;
                    else
                        ax = 1;
                    
                    if(y == 0)
                        ay = C;
                    else
                        ay = 1;
                   
                    s += ax*ay*in[x*size+y]*Cosine[x][i]*Cosine[y][j];
                }
            }
            s *= NORM;
            out[i*size+j] = (int)s;
        }
    }
}

//Фиксированная точка
long _Fract lrCosine[8][8] = {
    {1.00000LR, 1.00000LR, 1.00000LR, 1.00000LR, 1.00000LR, 1.00000LR, 1.00000LR, 1.00000LR},
    {0.98079LR, 0.83147LR, 0.55557LR, 0.19509LR, -0.19509LR, -0.55557LR, -0.83147LR, -0.98079LR},
    {0.92388LR, 0.38268LR, -0.38268LR, -0.92388LR, -0.92388LR, -0.38268LR, 0.38268LR, 0.92388LR},
    {0.83147LR, -0.19509LR, -0.98079LR, -0.55557LR, 0.55557LR, 0.98079LR, 0.19509LR, -0.83147LR},
    {0.70711LR, -0.70711LR, -0.70711LR, 0.70711LR, 0.70711LR, -0.70711LR, -0.70711LR, 0.70711LR},
    {0.55557LR, -0.98079LR, 0.19509LR, 0.83147LR, -0.83147LR, -0.19509LR, 0.98079LR, -0.55557LR},
    {0.38268LR, -0.92388LR, 0.92388LR, -0.38268LR, -0.38268LR, 0.92388LR, -0.92388LR, 0.38268LR},
    {0.19509LR, -0.55557LR, 0.83147LR, -0.98079LR, 0.98079LR, -0.83147LR, 0.55557LR, -0.19509LR}
};

void fdct_q31(int* in, int* out)
{
    int i, j, x, y;
    long _Fract ci, cj;
    long _Accum s;
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j++)
        {
            if(i == 0)
                ci = 0.70711LR;
            else
                ci = 1.0LR;
            if(j == 0)
                cj = 0.70711LR;
            else
                cj = 1.0LR;
            s = 0.0LK;
            for(x = 0; x < size; x++)
                for(y = 0; y < size; y++)
                    s += lrCosine[i][x]*lrCosine[j][y]*(long)in[x*size+y];
            s = (s*ci*cj) / 4;
            out[i*size+j] = (int)s;
        }
    } 
}

void idct_q31(int* in, int* out)
{
    int i, j, x, y;
    long _Fract ax, ay;
    long _Accum s;
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j++)
        {
            s = 0.0LK;
            for(x = 0; x < size; x++)
            {
                for(y = 0; y < size; y++)
                {
                    if(x == 0)
                        ax = 0.70711LR;
                    else
                        ax = 1.0LR;
                    
                    if(y == 0)
                        ay = 0.70711LR;
                    else
                        ay = 1.0LR;
                    
                    s += ax*ay*in[x*size+y]*lrCosine[x][i]*lrCosine[y][j];
                }
            }
            s = s / 4;
            out[i*size+j] = (int)s;
        }
    }
}

void msa_fdct_f32(int* in, int* out)
{
    int i, j, x, y;
    v4f32 ci, cj;
    v4f32 s;
    
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j+=4)
        {
            if(i == 0)
                ci = (v4f32) {0.70711f, 0.70711f, 0.70711f, 0.70711f};
            else
                ci = (v4f32) {1.0f, 1.0f, 1.0f, 1.0f};
            
            if(j == 0)
                cj = (v4f32) {0.70711f, 1.0f, 1.0f, 1.0f};
            else
                cj = (v4f32) {1.0f, 1.0f, 1.0f, 1.0f};
                
            s = (v4f32) {0.0f, 0.0f, 0.0f, 0.0f};
            
            for(x = 0; x < size; x++)
            {
                for(y = 0; y < size; y++)
                {
                    v4f32 vcos0, vcos1, vcos2, pixs;
                    pixs = (v4f32) {(float)in[x*size+y], (float)in[x*size+y], 
                            (float)in[x*size+y], (float)in[x*size+y]};
                    vcos1 = (v4f32) { Cosine[i][x], Cosine[i][x],
                            Cosine[i][x], Cosine[i][x] };
                    vcos2 = (v4f32) { Cosine[j][y], Cosine[j+1][y],
                            Cosine[j+2][y], Cosine[j+3][y] };
                    vcos0 = vcos1*vcos2;
                    s = __builtin_msa_fmadd_w(s, pixs, vcos0);
                }
            }
            s = (s*ci*cj) / 4;
            
            out[i*size+j] = (int)s[0];
            out[i*size+j+1] = (int)s[1];
            out[i*size+j+2] = (int)s[2];
            out[i*size+j+3] = (int)s[3];
        }
    } 
}

void msa_idct_f32(int* in, int* out)
{
    int i, j, x, y;
    v4f32 ax, ay, s;
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j+=4)
        {
            s = (v4f32) { 0.0f, 0.0f, 0.0f, 0.0f };
            for(x = 0; x < size; x++)
            {
                for(y = 0; y < size; y++)
                {
                    if(x == 0)
                        ax = (v4f32) { 0.70711f, 0.70711f, 0.70711f, 0.70711f };
                    else
                        ax = (v4f32) { 1.0f, 1.0f, 1.0f, 1.0f };
                    
                    if(y == 0)
                        ay = (v4f32) { 0.70711f, 0.70711f, 0.70711f, 0.70711f };
                    else
                        ay = (v4f32) { 1.0f, 1.0f, 1.0f, 1.0f };
                    
                    v4f32 vcos0, vcos1, vcos2, a;
                    vcos1 = (v4f32) { Cosine[x][i], Cosine[x][i],
                            Cosine[x][i], Cosine[x][i] };
                    vcos2 = (v4f32) { Cosine[y][j], Cosine[y][j+1],
                            Cosine[y][j+2], Cosine[y][j+3] };
                    vcos0 = vcos1*vcos2;
                    
                    a = ax*ay*(float)in[x*size+y];
                    s = __builtin_msa_fmadd_w(s, a, vcos0);
                }
            }
            s = s / 4;
            
            out[i*size+j] = (int)s[0];
            out[i*size+j+1] = (int)s[1];
            out[i*size+j+2] = (int)s[2];
            out[i*size+j+3] = (int)s[3];
        }
    }
}

//Целочисленная реализация
#define BITS 20 //точность
const int intCosine[8][8] =
{
    { (int)(1.00000000 * (1 << BITS)), (int)(1.00000000 * (1 << BITS)), (int)(1.00000000 * (1 << BITS)), (int)(1.00000000 * (1 << BITS)), (int)(1.00000000 * (1 << BITS)), (int)(1.00000000 * (1 << BITS)), (int)(1.00000000 * (1 << BITS)), (int)(1.00000000 * (1 << BITS)) },
    { (int)(0.98078528 * (1 << BITS)), (int)(0.83146961 * (1 << BITS)), (int)(0.55557023 * (1 << BITS)), (int)(0.19509032 * (1 << BITS)), (int)(-0.19509032 * (1 << BITS)), (int)(-0.55557023 * (1 << BITS)), (int)(-0.83146961 * (1 << BITS)), (int)(-0.98078528 * (1 << BITS)) },
    { (int)(0.92387953 * (1 << BITS)), (int)(0.38268343 * (1 << BITS)), (int)(-0.38268343 * (1 << BITS)), (int)(-0.92387953 * (1 << BITS)), (int)(-0.92387953 * (1 << BITS)), (int)(-0.38268343 * (1 << BITS)), (int)(0.38268343 * (1 << BITS)), (int)(0.92387953 * (1 << BITS)) },
    { (int)(0.83146961 * (1 << BITS)), (int)(-0.19509032 * (1 << BITS)), (int)(-0.98078528 * (1 << BITS)), (int)(-0.55557023 * (1 << BITS)), (int)(0.55557023 * (1 << BITS)), (int)(0.98078528 * (1 << BITS)), (int)(0.19509032 * (1 << BITS)), (int)(-0.83146961 * (1 << BITS)) },
    { (int)(0.70710678 * (1 << BITS)), (int)(-0.70710678 * (1 << BITS)), (int)(-0.70710678 * (1 << BITS)), (int)(0.70710678 * (1 << BITS)), (int)(0.70710678 * (1 << BITS)), (int)(-0.70710678 * (1 << BITS)), (int)(-0.70710678 * (1 << BITS)), (int)(0.70710678 * (1 << BITS)) },
    { (int)(0.55557023 * (1 << BITS)), (int)(-0.98078528 * (1 << BITS)), (int)(0.19509032 * (1 << BITS)), (int)(0.83146961 * (1 << BITS)), (int)(-0.83146961 * (1 << BITS)), (int)(-0.19509032 * (1 << BITS)), (int)(0.98078528 * (1 << BITS)), (int)(-0.55557023 * (1 << BITS)) },
    { (int)(0.38268343 * (1 << BITS)), (int)(-0.92387953 * (1 << BITS)), (int)(0.92387953 * (1 << BITS)), (int)(-0.38268343 * (1 << BITS)), (int)(-0.38268343 * (1 << BITS)), (int)(0.92387953 * (1 << BITS)), (int)(-0.92387953 * (1 << BITS)), (int)(0.38268343 * (1 << BITS)) },
    { (int)(0.19509032 * (1 << BITS)), (int)(-0.55557023 * (1 << BITS)), (int)(0.83146961 * (1 << BITS)), (int)(-0.98078528 * (1 << BITS)), (int)(0.98078528 * (1 << BITS)), (int)(-0.83146961 * (1 << BITS)), (int)(0.55557023 * (1 << BITS)), (int)(-0.19509032 * (1 << BITS)) }
};
const int divider = 1 << BITS;
const int c = (int)(0.70710678 * (1 << BITS));

void msa_fdct_i32(int* in, int* out)
{
    int i, j, x, y;
    v4i32 ci, cj, s;

    for(i = 0; i < 8; i++)
    {
        for(j = 0; j < 8; j+=4)
        {
            if(i == 0)
                ci = (v4i32) {c, c, c, c};
            else
                ci = (v4i32) {divider, divider, divider, divider};
            
            if(j == 0)
                cj = (v4i32) {c, divider, divider, divider};
            else
                cj = (v4i32) {divider, divider, divider, divider};
            
            s = (v4i32){0, 0, 0, 0};
            
            for(x = 0; x < size; x++)
                for(y = 0; y < size; y++)
                {
                    v4i32 vcos0, vcos1, vcos2, pixs;
                    vcos1 = (v4i32) { intCosine[i][x], intCosine[i][x],
                            intCosine[i][x], intCosine[i][x] };
                    vcos2 = (v4i32) { intCosine[j][y], intCosine[j+1][y],
                            intCosine[j+2][y], intCosine[j+3][y] };
                    
                    vcos0 = vcos1*vcos2;
                    vcos0 = vcos0 >> BITS;
                    s += vcos0*in[x*size+y];
                }
            
            s = s >> BITS;
            
            s = (s*((ci*cj) >> BITS)) >> BITS;
            s = s >> 2;
            out[i*size+j] = s[0];
            out[i*size+j+1] = s[1];
            out[i*size+j+2] = s[2];
            out[i*size+j+3] = s[3];
        }
    } 
}

void msa_idct_i32(int* in, int* out)
{
    int i, j, x, y;
    v4i32 s, ax, ay;
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j+=4)
        {
            s = (v4i32) {0, 0, 0, 0};
            for(x = 0; x < size; x++)
            {
                for(y = 0; y < size; y++)
                {
                    if(x == 0)
                        ax = (v4i32) {c, c, c, c};
                    else
                        ax = (v4i32) {divider, divider, divider, divider};
                    
                    if(y == 0)
                        ay = (v4i32) {c, c, c, c};
                    else
                        ay = (v4i32) {divider, divider, divider, divider};
                    
                    v4i32 vcos0, vcos1, vcos2, a;
                    vcos1 = (v4i32) { intCosine[x][i], intCosine[x][i],
                        intCosine[x][i], intCosine[x][i] };
                    vcos2 = (v4i32) { intCosine[y][j], intCosine[y][j+1],
                        intCosine[y][j+2], intCosine[y][j+3] };
                    
                    vcos0 = vcos1*vcos2 >> BITS;
                    a = ax*ay >> BITS;
                    a *= in[x*size+y];
                    s += a*vcos0 >> BITS;
                }
            }
            s = s >> (BITS + 2);
            
            out[i*size+j] = s[0];
            out[i*size+j+1] = s[1];
            out[i*size+j+2] = s[2];
            out[i*size+j+3] = s[3];
        }
    }
}

const long longCosine[8][8] =
{
    { (long)(1.00000000 * (1 << BITS)), (long)(1.00000000 * (1 << BITS)), (long)(1.00000000 * (1 << BITS)), (long)(1.00000000 * (1 << BITS)), (long)(1.00000000 * (1 << BITS)), (long)(1.00000000 * (1 << BITS)), (long)(1.00000000 * (1 << BITS)), (long)(1.00000000 * (1 << BITS)) },
    { (long)(0.98078528 * (1 << BITS)), (long)(0.83146961 * (1 << BITS)), (long)(0.55557023 * (1 << BITS)), (long)(0.19509032 * (1 << BITS)), (long)(-0.19509032 * (1 << BITS)), (long)(-0.55557023 * (1 << BITS)), (long)(-0.83146961 * (1 << BITS)), (long)(-0.98078528 * (1 << BITS)) },
    { (long)(0.92387953 * (1 << BITS)), (long)(0.38268343 * (1 << BITS)), (long)(-0.38268343 * (1 << BITS)), (long)(-0.92387953 * (1 << BITS)), (long)(-0.92387953 * (1 << BITS)), (long)(-0.38268343 * (1 << BITS)), (long)(0.38268343 * (1 << BITS)), (long)(0.92387953 * (1 << BITS)) },
    { (long)(0.83146961 * (1 << BITS)), (long)(-0.19509032 * (1 << BITS)), (long)(-0.98078528 * (1 << BITS)), (long)(-0.55557023 * (1 << BITS)), (long)(0.55557023 * (1 << BITS)), (long)(0.98078528 * (1 << BITS)), (long)(0.19509032 * (1 << BITS)), (long)(-0.83146961 * (1 << BITS)) },
    { (long)(0.70710678 * (1 << BITS)), (long)(-0.70710678 * (1 << BITS)), (long)(-0.70710678 * (1 << BITS)), (long)(0.70710678 * (1 << BITS)), (long)(0.70710678 * (1 << BITS)), (long)(-0.70710678 * (1 << BITS)), (long)(-0.70710678 * (1 << BITS)), (long)(0.70710678 * (1 << BITS)) },
    { (long)(0.55557023 * (1 << BITS)), (long)(-0.98078528 * (1 << BITS)), (long)(0.19509032 * (1 << BITS)), (long)(0.83146961 * (1 << BITS)), (long)(-0.83146961 * (1 << BITS)), (long)(-0.19509032 * (1 << BITS)), (long)(0.98078528 * (1 << BITS)), (long)(-0.55557023 * (1 << BITS)) },
    { (long)(0.38268343 * (1 << BITS)), (long)(-0.92387953 * (1 << BITS)), (long)(0.92387953 * (1 << BITS)), (long)(-0.38268343 * (1 << BITS)), (long)(-0.38268343 * (1 << BITS)), (long)(0.92387953 * (1 << BITS)), (long)(-0.92387953 * (1 << BITS)), (long)(0.38268343 * (1 << BITS)) },
    { (long)(0.19509032 * (1 << BITS)), (long)(-0.55557023 * (1 << BITS)), (long)(0.83146961 * (1 << BITS)), (long)(-0.98078528 * (1 << BITS)), (long)(0.98078528 * (1 << BITS)), (long)(-0.83146961 * (1 << BITS)), (long)(0.55557023 * (1 << BITS)), (long)(-0.19509032 * (1 << BITS)) }
};
const long ldivider = (long)(1 << BITS);
const long lc = (long)(0.70710678 * (1 << BITS));

void msa_fdct_i64(int* in, int* out)
{
    int i, j, x, y;
    v2i64 ci, cj, s;

    for(i = 0; i < 8; i++)
    {
        for(j = 0; j < 8; j+=2)
        {
            if(i == 0)
                ci = (v2i64) {lc, lc};
            else
                ci = (v2i64) {ldivider, ldivider};
            
            if(j == 0)
                cj = (v2i64) {lc, ldivider};
            else
                cj = (v2i64) {ldivider, ldivider};
            
            s = (v2i64){0, 0};
            
            for(x = 0; x < size; x++)
                for(y = 0; y < size; y++)
                {
                    v2i64 vcos0, vcos1, vcos2, pixs;
                    vcos1 = (v2i64) { longCosine[i][x], longCosine[i][x] };
                    vcos2 = (v2i64) { longCosine[j][y], longCosine[j+1][y] };
                    
                    vcos0 = vcos1*vcos2;
                    vcos0 = vcos0 >> BITS;
                    s += vcos0*(long)in[x*size+y];
                }
            
            s = s >> BITS;
            
            s = (s*((ci*cj) >> BITS)) >> BITS;
            s = s >> 2;
            out[i*size+j] = s[0];
            out[i*size+j+1] = s[1];
        }
    } 
}

void msa_idct_i64(int* in, int* out)
{
    int i, j, x, y;
    v2i64 s, ax, ay;
    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j+=2)
        {
            s = (v2i64) {0, 0};
            
            for(x = 0; x < size; x++)
            {
                for(y = 0; y < size; y++)
                {
                    if(x == 0)
                        ax = (v2i64) {lc, lc};
                    else
                        ax = (v2i64) {ldivider, ldivider};
                    
                    if(y == 0)
                        ay = (v2i64) {lc, lc};
                    else
                        ay = (v2i64) {ldivider, ldivider};
                      
                    v2i64 vcos0, vcos1, vcos2, a;
                    vcos1 = (v2i64) { longCosine[x][i], longCosine[x][i]};
                    vcos2 = (v2i64) { longCosine[y][j], longCosine[y][j+1]};
                    
                    vcos0 = vcos1*vcos2 >> BITS;
                    a = ax*ay >> BITS;
                    a *= in[x*size+y];
                    s += a*vcos0 >> BITS;
                    
                }
            }
            
            s = s >> BITS;
            s = s >> 2;
            out[i*size+j] = s[0];
            out[i*size+j+1] = s[1];
        }
    }
}

#endif /* MY_DCT_H */