/**
 * \file src/k128cp2elfun_sqrt.c
 * \brief Вычисление квадратного корня
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Вычисление квадратного корня
 */

#include <math.h>
#include <assert.h>
#include "local.h"

// Elementary function calculation: sqrt

// function prototypes
float k128cp2elfun_common (k128cp2elfun_params_t *func_params, float x);

// sqrt (x)
// quadratic polinom coeffs table
static k128cp2elfun_table_t k128cp2elfun_sqrt_coefstable[] = {
	/*   0 */ { 0xffffff80, 0x00008000, 0x04000001 },
	/*   1 */ { 0xffffff81, 0x00007f81, 0x0403fe03 },
	/*   2 */ { 0xffffff83, 0x00007f03, 0x0407f811 },
	/*   3 */ { 0xffffff86, 0x00007e86, 0x040bee36 },
	/*   4 */ { 0xffffff86, 0x00007e0c, 0x040fe07d },
	/*   5 */ { 0xffffff88, 0x00007d92, 0x0413cef4 },
	/*   6 */ { 0xffffff89, 0x00007d1a, 0x0417b9a4 },
	/*   7 */ { 0xffffff8b, 0x00007ca3, 0x041ba097 },
	/*   8 */ { 0xffffff8b, 0x00007c2e, 0x041f83d9 },
	/*   9 */ { 0xffffff8e, 0x00007bb9, 0x04236374 },
	/*  10 */ { 0xffffff8f, 0x00007b46, 0x04273f74 },
	/*  11 */ { 0xffffff8f, 0x00007ad5, 0x042b17de },
	/*  12 */ { 0xffffff91, 0x00007a64, 0x042eecc2 },
	/*  13 */ { 0xffffff91, 0x000079f5, 0x0432be26 },
	/*  14 */ { 0xffffff92, 0x00007987, 0x04368c14 },
	/*  15 */ { 0xffffff94, 0x0000791a, 0x043a5694 },
	/*  16 */ { 0xffffff95, 0x000078ae, 0x043e1db3 },
	/*  17 */ { 0xffffff97, 0x00007843, 0x0441e177 },
	/*  18 */ { 0xffffff97, 0x000077da, 0x0445a1e9 },
	/*  19 */ { 0xffffff99, 0x00007771, 0x04495f13 },
	/*  20 */ { 0xffffff99, 0x0000770a, 0x044d18fe },
	/*  21 */ { 0xffffff9b, 0x000076a3, 0x0450cfb1 },
	/*  22 */ { 0xffffff9b, 0x0000763e, 0x04548334 },
	/*  23 */ { 0xffffff9d, 0x000075d9, 0x04583391 },
	/*  24 */ { 0xffffff9d, 0x00007576, 0x045be0ce },
	/*  25 */ { 0xffffff9f, 0x00007513, 0x045f8af4 },
	/*  26 */ { 0xffffff9f, 0x000074b2, 0x04633209 },
	/*  27 */ { 0xffffffa1, 0x00007451, 0x0466d617 },
	/*  28 */ { 0xffffffa1, 0x000073f2, 0x046a7724 },
	/*  29 */ { 0xffffffa3, 0x00007393, 0x046e1538 },
	/*  30 */ { 0xffffffa4, 0x00007335, 0x0471b05b },
	/*  31 */ { 0xffffffa3, 0x000072d9, 0x04754892 },
	/*  32 */ { 0xffffffa4, 0x0000727d, 0x0478dde6 },
	/*  33 */ { 0xffffffa6, 0x00007221, 0x047c705f },
	/*  34 */ { 0xffffffa7, 0x000071c7, 0x047fffff },
	/*  35 */ { 0xffffffa6, 0x0000716e, 0x04838cd3 },
	/*  36 */ { 0xffffffa8, 0x00007115, 0x048716dc },
	/*  37 */ { 0xffffffa9, 0x000070bd, 0x048a9e24 },
	/*  38 */ { 0xffffffaa, 0x00007066, 0x048e22af },
	/*  39 */ { 0xffffffaa, 0x00007010, 0x0491a486 },
	/*  40 */ { 0xffffffac, 0x00006fba, 0x049523ae },
	/*  41 */ { 0xffffffad, 0x00006f65, 0x0498a02d },
	/*  42 */ { 0xffffffae, 0x00006f11, 0x049c1a08 },
	/*  43 */ { 0xffffffae, 0x00006ebe, 0x049f9147 },
	/*  44 */ { 0xffffffad, 0x00006e6c, 0x04a305ef },
	/*  45 */ { 0xffffffaf, 0x00006e1a, 0x04a67803 },
	/*  46 */ { 0xffffffaf, 0x00006dc9, 0x04a9e78e },
	/*  47 */ { 0xffffffb1, 0x00006d78, 0x04ad5493 },
	/*  48 */ { 0xffffffb0, 0x00006d29, 0x04b0bf16 },
	/*  49 */ { 0xffffffb1, 0x00006cda, 0x04b4271e },
	/*  50 */ { 0xffffffb3, 0x00006c8b, 0x04b78cb1 },
	/*  51 */ { 0xffffffb4, 0x00006c3d, 0x04baefd4 },
	/*  52 */ { 0xffffffb4, 0x00006bf0, 0x04be508c },
	/*  53 */ { 0xffffffb4, 0x00006ba4, 0x04c1aedc },
	/*  54 */ { 0xffffffb5, 0x00006b58, 0x04c50acc },
	/*  55 */ { 0xffffffb5, 0x00006b0d, 0x04c86461 },
	/*  56 */ { 0xffffffb7, 0x00006ac2, 0x04cbbb9d },
	/*  57 */ { 0xffffffb7, 0x00006a78, 0x04cf1089 },
	/*  58 */ { 0xffffffb7, 0x00006a2f, 0x04d26326 },
	/*  59 */ { 0xffffffb8, 0x000069e6, 0x04d5b37c },
	/*  60 */ { 0xffffffb8, 0x0000699e, 0x04d9018d },
	/*  61 */ { 0xffffffba, 0x00006956, 0x04dc4d5e },
	/*  62 */ { 0xffffffba, 0x0000690f, 0x04df96f6 },
	/*  63 */ { 0xffffffba, 0x000068c9, 0x04e2de55 },
	/*  64 */ { 0xffffffba, 0x00006883, 0x04e62386 },
	/*  65 */ { 0xffffffba, 0x0000683e, 0x04e96687 },
	/*  66 */ { 0xffffffbb, 0x000067f9, 0x04eca75f },
	/*  67 */ { 0xffffffbd, 0x000067b4, 0x04efe613 },
	/*  68 */ { 0xffffffbe, 0x00006770, 0x04f322a6 },
	/*  69 */ { 0xffffffbe, 0x0000672d, 0x04f65d1d },
	/*  70 */ { 0xffffffbf, 0x000066ea, 0x04f9957b },
	/*  71 */ { 0xffffffbe, 0x000066a8, 0x04fccbc7 },
	/*  72 */ { 0xffffffbf, 0x00006666, 0x05000001 },
	/*  73 */ { 0xffffffbf, 0x00006625, 0x0503322e },
	/*  74 */ { 0xffffffc0, 0x000065e4, 0x05066253 },
	/*  75 */ { 0xffffffc0, 0x000065a4, 0x05099074 },
	/*  76 */ { 0xffffffc1, 0x00006564, 0x050cbc94 },
	/*  77 */ { 0xffffffc0, 0x00006525, 0x050fe6b8 },
	/*  78 */ { 0xffffffc1, 0x000064e6, 0x05130ee1 },
	/*  79 */ { 0xffffffc3, 0x000064a7, 0x05163515 },
	/*  80 */ { 0xffffffc3, 0x00006469, 0x05195958 },
	/*  81 */ { 0xffffffc2, 0x0000642c, 0x051c7bac },
	/*  82 */ { 0xffffffc2, 0x000063ef, 0x051f9c16 },
	/*  83 */ { 0xffffffc4, 0x000063b2, 0x0522ba96 },
	/*  84 */ { 0xffffffc3, 0x00006376, 0x0525d736 },
	/*  85 */ { 0xffffffc4, 0x0000633a, 0x0528f1f3 },
	/*  86 */ { 0xffffffc6, 0x000062fe, 0x052c0ad4 },
	/*  87 */ { 0xffffffc6, 0x000062c3, 0x052f21db },
	/*  88 */ { 0xffffffc5, 0x00006289, 0x0532370c },
	/*  89 */ { 0xffffffc5, 0x0000624f, 0x05354a6a },
	/*  90 */ { 0xffffffc6, 0x00006215, 0x05385bf7 },
	/*  91 */ { 0xffffffc8, 0x000061db, 0x053b6bb8 },
	/*  92 */ { 0xffffffc8, 0x000061a2, 0x053e79b0 },
	/*  93 */ { 0xffffffc7, 0x0000616a, 0x054185e1 },
	/*  94 */ { 0xffffffc7, 0x00006132, 0x0544904e },
	/*  95 */ { 0xffffffc8, 0x000060fa, 0x054798fa },
	/*  96 */ { 0xffffffc9, 0x000060c2, 0x054a9feb },
	/*  97 */ { 0xffffffca, 0x0000608b, 0x054da51f },
	/*  98 */ { 0xffffffca, 0x00006054, 0x0550a89f },
	/*  99 */ { 0xffffffca, 0x0000601e, 0x0553aa68 },
	/* 100 */ { 0xffffffca, 0x00005fe8, 0x0556aa81 },
	/* 101 */ { 0xffffffcb, 0x00005fb2, 0x0559a8ea },
	/* 102 */ { 0xffffffcb, 0x00005f7d, 0x055ca5a7 },
	/* 103 */ { 0xffffffcb, 0x00005f48, 0x055fa0bc },
	/* 104 */ { 0xffffffcd, 0x00005f13, 0x05629a29 },
	/* 105 */ { 0xffffffcc, 0x00005edf, 0x056591f4 },
	/* 106 */ { 0xffffffcd, 0x00005eab, 0x0568881c },
	/* 107 */ { 0xffffffcc, 0x00005e78, 0x056b7ca7 },
	/* 108 */ { 0xffffffce, 0x00005e44, 0x056e6f97 },
	/* 109 */ { 0xffffffce, 0x00005e11, 0x057160ee },
	/* 110 */ { 0xffffffcd, 0x00005ddf, 0x057450ad },
	/* 111 */ { 0xffffffcf, 0x00005dac, 0x05773ed9 },
	/* 112 */ { 0xffffffcf, 0x00005d7a, 0x057a2b75 },
	/* 113 */ { 0xffffffce, 0x00005d49, 0x057d1680 },
	/* 114 */ { 0xffffffcf, 0x00005d17, 0x05800001 },
	/* 115 */ { 0xffffffd0, 0x00005ce6, 0x0582e7f5 },
	/* 116 */ { 0xffffffd0, 0x00005cb5, 0x0585ce64 },
	/* 117 */ { 0xffffffd0, 0x00005c85, 0x0588b34c },
	/* 118 */ { 0xffffffd0, 0x00005c55, 0x058b96b2 },
	/* 119 */ { 0xffffffd0, 0x00005c25, 0x058e7899 },
	/* 120 */ { 0xffffffd1, 0x00005bf5, 0x05915902 },
	/* 121 */ { 0xffffffd1, 0x00005bc6, 0x059437ed },
	/* 122 */ { 0xffffffd1, 0x00005b97, 0x05971560 },
	/* 123 */ { 0xffffffd2, 0x00005b68, 0x0599f15c },
	/* 124 */ { 0xffffffd1, 0x00005b3a, 0x059ccbe3 },
	/* 125 */ { 0xffffffd3, 0x00005b0b, 0x059fa4f8 },
	/* 126 */ { 0xffffffd3, 0x00005add, 0x05a27c9c },
	/* 127 */ { 0xffffffd2, 0x00005ab0, 0x05a552d1 }
};

// sqrt (2x) (for odd exponent)
// quadratic polinom coeffs table
static k128cp2elfun_table_t k128cp2elfun_sqrt2_coefstable[] = {
	/*   0 */ { 0xffffff4c, 0x0000b505, 0x05a82799 },
	/*   1 */ { 0xffffff4e, 0x0000b451, 0x05adccef },
	/*   2 */ { 0xffffff50, 0x0000b39f, 0x05b36caf },
	/*   3 */ { 0xffffff52, 0x0000b2ef, 0x05b906e8 },
	/*   4 */ { 0xffffff55, 0x0000b241, 0x05be9ba8 },
	/*   5 */ { 0xffffff57, 0x0000b195, 0x05c42b03 },
	/*   6 */ { 0xffffff59, 0x0000b0eb, 0x05c9b506 },
	/*   7 */ { 0xffffff5b, 0x0000b043, 0x05cf39c1 },
	/*   8 */ { 0xffffff5c, 0x0000af9d, 0x05d4b944 },
	/*   9 */ { 0xffffff5d, 0x0000aef9, 0x05da339b },
	/*  10 */ { 0xffffff60, 0x0000ae56, 0x05dfa8d7 },
	/*  11 */ { 0xffffff62, 0x0000adb5, 0x05e51905 },
	/*  12 */ { 0xffffff63, 0x0000ad16, 0x05ea8435 },
	/*  13 */ { 0xffffff64, 0x0000ac79, 0x05efea71 },
	/*  14 */ { 0xffffff66, 0x0000abdd, 0x05f54bc9 },
	/*  15 */ { 0xffffff68, 0x0000ab43, 0x05faa849 },
	/*  16 */ { 0xffffff68, 0x0000aaab, 0x06000000 },
	/*  17 */ { 0xffffff6a, 0x0000aa14, 0x060552f8 },
	/*  18 */ { 0xffffff6d, 0x0000a97e, 0x060aa140 },
	/*  19 */ { 0xffffff6e, 0x0000a8ea, 0x060feae4 },
	/*  20 */ { 0xffffff6f, 0x0000a858, 0x06152fed },
	/*  21 */ { 0xffffff71, 0x0000a7c7, 0x061a706a },
	/*  22 */ { 0xffffff72, 0x0000a738, 0x061fac66 },
	/*  23 */ { 0xffffff73, 0x0000a6aa, 0x0624e3ed },
	/*  24 */ { 0xffffff75, 0x0000a61d, 0x062a170a },
	/*  25 */ { 0xffffff76, 0x0000a592, 0x062f45c7 },
	/*  26 */ { 0xffffff78, 0x0000a508, 0x06347030 },
	/*  27 */ { 0xffffff78, 0x0000a480, 0x06399650 },
	/*  28 */ { 0xffffff79, 0x0000a3f9, 0x063eb830 },
	/*  29 */ { 0xffffff7b, 0x0000a373, 0x0643d5dc },
	/*  30 */ { 0xffffff7d, 0x0000a2ee, 0x0648ef5f },
	/*  31 */ { 0xffffff7d, 0x0000a26b, 0x064e04c2 },
	/*  32 */ { 0xffffff80, 0x0000a1e8, 0x0653160f },
	/*  33 */ { 0xffffff7f, 0x0000a168, 0x06582350 },
	/*  34 */ { 0xffffff81, 0x0000a0e8, 0x065d2c8d },
	/*  35 */ { 0xffffff83, 0x0000a069, 0x066231d2 },
	/*  36 */ { 0xffffff84, 0x00009fec, 0x06673326 },
	/*  37 */ { 0xffffff84, 0x00009f70, 0x066c3095 },
	/*  38 */ { 0xffffff85, 0x00009ef5, 0x06712a25 },
	/*  39 */ { 0xffffff86, 0x00009e7b, 0x06761fe2 },
	/*  40 */ { 0xffffff87, 0x00009e02, 0x067b11d3 },
	/*  41 */ { 0xffffff89, 0x00009d8a, 0x067fffff },
	/*  42 */ { 0xffffff8a, 0x00009d13, 0x0684ea73 },
	/*  43 */ { 0xffffff8c, 0x00009c9d, 0x0689d133 },
	/*  44 */ { 0xffffff8d, 0x00009c28, 0x068eb44b },
	/*  45 */ { 0xffffff8d, 0x00009bb5, 0x069393bf },
	/*  46 */ { 0xffffff8e, 0x00009b42, 0x06986f9c },
	/*  47 */ { 0xffffff90, 0x00009ad0, 0x069d47e5 },
	/*  48 */ { 0xffffff8f, 0x00009a60, 0x06a21ca6 },
	/*  49 */ { 0xffffff91, 0x000099f0, 0x06a6ede2 },
	/*  50 */ { 0xffffff92, 0x00009981, 0x06abbba6 },
	/*  51 */ { 0xffffff93, 0x00009913, 0x06b085f6 },
	/*  52 */ { 0xffffff94, 0x000098a6, 0x06b54cda },
	/*  53 */ { 0xffffff95, 0x0000983a, 0x06ba105a },
	/*  54 */ { 0xffffff95, 0x000097cf, 0x06bed07d },
	/*  55 */ { 0xffffff97, 0x00009764, 0x06c38d4a },
	/*  56 */ { 0xffffff97, 0x000096fb, 0x06c846c8 },
	/*  57 */ { 0xffffff99, 0x00009692, 0x06ccfcfd },
	/*  58 */ { 0xffffff98, 0x0000962b, 0x06d1aff0 },
	/*  59 */ { 0xffffff99, 0x000095c4, 0x06d65fa9 },
	/*  60 */ { 0xffffff9a, 0x0000955e, 0x06db0c2e },
	/*  61 */ { 0xffffff9c, 0x000094f8, 0x06dfb586 },
	/*  62 */ { 0xffffff9c, 0x00009494, 0x06e45bb5 },
	/*  63 */ { 0xffffff9d, 0x00009430, 0x06e8fec6 },
	/*  64 */ { 0xffffff9e, 0x000093cd, 0x06ed9eba },
	/*  65 */ { 0xffffff9f, 0x0000936b, 0x06f23b9a },
	/*  66 */ { 0xffffff9f, 0x0000930a, 0x06f6d56d },
	/*  67 */ { 0xffffffa0, 0x000092a9, 0x06fb6c38 },
	/*  68 */ { 0xffffffa1, 0x00009249, 0x07000000 },
	/*  69 */ { 0xffffffa1, 0x000091ea, 0x070490cd },
	/*  70 */ { 0xffffffa3, 0x0000918b, 0x07091ea2 },
	/*  71 */ { 0xffffffa2, 0x0000912e, 0x070da987 },
	/*  72 */ { 0xffffffa3, 0x000090d1, 0x07123180 },
	/*  73 */ { 0xffffffa5, 0x00009074, 0x0716b695 },
	/*  74 */ { 0xffffffa4, 0x00009019, 0x071b38c9 },
	/*  75 */ { 0xffffffa5, 0x00008fbe, 0x071fb823 },
	/*  76 */ { 0xffffffa7, 0x00008f63, 0x072434a8 },
	/*  77 */ { 0xffffffa7, 0x00008f0a, 0x0728ae5c },
	/*  78 */ { 0xffffffa7, 0x00008eb1, 0x072d2547 },
	/*  79 */ { 0xffffffa9, 0x00008e58, 0x0731996d },
	/*  80 */ { 0xffffffa8, 0x00008e01, 0x07360ad2 },
	/*  81 */ { 0xffffffa9, 0x00008daa, 0x073a797b },
	/*  82 */ { 0xffffffab, 0x00008d53, 0x073ee56e },
	/*  83 */ { 0xffffffac, 0x00008cfd, 0x07434eb1 },
	/*  84 */ { 0xffffffac, 0x00008ca8, 0x0747b548 },
	/*  85 */ { 0xffffffab, 0x00008c54, 0x074c1937 },
	/*  86 */ { 0xffffffac, 0x00008c00, 0x07507a83 },
	/*  87 */ { 0xffffffad, 0x00008bac, 0x0754d933 },
	/*  88 */ { 0xffffffae, 0x00008b59, 0x07593548 },
	/*  89 */ { 0xffffffae, 0x00008b07, 0x075d8ec9 },
	/*  90 */ { 0xffffffaf, 0x00008ab5, 0x0761e5b9 },
	/*  91 */ { 0xffffffaf, 0x00008a64, 0x07663a1f },
	/*  92 */ { 0xffffffb1, 0x00008a13, 0x076a8bfc },
	/*  93 */ { 0xffffffb1, 0x000089c3, 0x076edb58 },
	/*  94 */ { 0xffffffb1, 0x00008974, 0x07732834 },
	/*  95 */ { 0xffffffb1, 0x00008925, 0x07777298 },
	/*  96 */ { 0xffffffb3, 0x000088d6, 0x077bba85 },
	/*  97 */ { 0xffffffb4, 0x00008888, 0x077fffff },
	/*  98 */ { 0xffffffb3, 0x0000883b, 0x0784430f },
	/*  99 */ { 0xffffffb4, 0x000087ee, 0x078883b3 },
	/* 100 */ { 0xffffffb4, 0x000087a2, 0x078cc1f2 },
	/* 101 */ { 0xffffffb4, 0x00008756, 0x0790fdd2 },
	/* 102 */ { 0xffffffb6, 0x0000870a, 0x07953754 },
	/* 103 */ { 0xffffffb5, 0x000086c0, 0x07996e7b },
	/* 104 */ { 0xffffffb6, 0x00008675, 0x079da34f },
	/* 105 */ { 0xffffffb7, 0x0000862b, 0x07a1d5d0 },
	/* 106 */ { 0xffffffb6, 0x000085e2, 0x07a60604 },
	/* 107 */ { 0xffffffb7, 0x00008599, 0x07aa33ed },
	/* 108 */ { 0xffffffb8, 0x00008550, 0x07ae5f92 },
	/* 109 */ { 0xffffffb9, 0x00008508, 0x07b288f2 },
	/* 110 */ { 0xffffffb8, 0x000084c1, 0x07b6b015 },
	/* 111 */ { 0xffffffba, 0x00008479, 0x07bad4fd },
	/* 112 */ { 0xffffffb9, 0x00008433, 0x07bef7ac },
	/* 113 */ { 0xffffffbb, 0x000083ec, 0x07c31828 },
	/* 114 */ { 0xffffffbc, 0x000083a6, 0x07c73673 },
	/* 115 */ { 0xffffffbb, 0x00008361, 0x07cb5292 },
	/* 116 */ { 0xffffffbc, 0x0000831c, 0x07cf6c85 },
	/* 117 */ { 0xffffffbd, 0x000082d7, 0x07d38454 },
	/* 118 */ { 0xffffffbd, 0x00008293, 0x07d79a00 },
	/* 119 */ { 0xffffffbc, 0x00008250, 0x07dbad8b },
	/* 120 */ { 0xffffffbe, 0x0000820c, 0x07dfbefa },
	/* 121 */ { 0xffffffbe, 0x000081c9, 0x07e3ce52 },
	/* 122 */ { 0xffffffbe, 0x00008187, 0x07e7db92 },
	/* 123 */ { 0xffffffbe, 0x00008145, 0x07ebe6c0 },
	/* 124 */ { 0xffffffbf, 0x00008103, 0x07efefdf },
	/* 125 */ { 0xffffffbe, 0x000080c2, 0x07f3f6f3 },
	/* 126 */ { 0xffffffbf, 0x00008081, 0x07f7fbfc },
	/* 127 */ { 0xffffffc0, 0x00008040, 0x07fbff00 }
};

//
static k128cp2elfun_params_t k128cp2elfun_sqrt_params = {
	"sqrt",                            // function name
	k128cp2elfun_sqrt_coefstable,      // pointer to quadratic polinom coeffs table
	{ .f = 1.0 },                      // minimum argument value
	{ .f = 2.0 },                      // maximum argument value
	{ .f = 1.0 },                      // function value for small (i.e. x1=x2=0) arguments
	127,                               // minimum exponent value
	127                                // maximum exponent value
};


//
static k128cp2elfun_params_t k128cp2elfun_sqrt2_params = {
	"sqrt(2x)",                        // function name
	k128cp2elfun_sqrt2_coefstable,     // pointer to quadratic polinom coeffs table
	{ .f = 1.0     },                  // minimum argument value
	{ .f = 2.0     },                  // maximum argument value
	{ .f = M_SQRT2 },                  // function value for small (i.e. x1=x2=0) arguments
	127,                               // minimum exponent value
	127                                // maximum exponent value
};


//
float k128cp2elfun_core_sqrt (float x) {

	k128cp2elfun_single_t arg, rarg, res;
	k128cp2elfun_params_t *elfunptr;

	if (x==1.0)
		return 1.0;

	// return 0.0 for x = 0.0
	if (x == 0.0) {
		res.f = 0.0;
		goto return_result;
	}

	// check argument value
	arg.f = x;
	assert (arg.fields.s == 0);

	// reduce argument to interval [1.0, 2.0)
	rarg.f = x;
	rarg.fields.s = 0;
	rarg.fields.e = 127;

	// call common function for quadratic approximation
	// (depends on exponent parity)
	if (((arg.fields.e-127) % 2) == 0) {
		// calculate sqrt(x)
		elfunptr = &k128cp2elfun_sqrt_params;
	} else {
		// calculate sqrt(2x)
		elfunptr = &k128cp2elfun_sqrt2_params;
	}
	res.f = k128cp2elfun_common (elfunptr, rarg.f);

	// check return value
	assert (res.fields.s == 0);
	assert (res.fields.e == (127 - (0)));

	// restore exponent
	res.fields.e = 127 + ((arg.fields.e-127) >> 1);

return_result:
	// return result
	return res.f;
}


/**
 * \brief Вычисление квадратного корня
 * \param x Значение аргумента
 * \return sqrt(x)
 */
float k128cp2elfun_sqrt (float x) {

	k128cp2elfun_single_t res;

	// return nan for negative values
	if ((x < 0) && (x != 0)) {
		res.f = NAN;
		goto return_result;
	}

	// switch for non-negative values
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;      break;
		case FP_INFINITE  :  res.f = INFINITY; break;
		case FP_ZERO      :  res.f = 0.0;      break;
		case FP_SUBNORMAL :  res.f = 0.0;      break;
		case FP_NORMAL    :
			// call core function
			res.f = k128cp2elfun_core_sqrt (x);
			break;
		default: /* internal error */ assert(0==1); break;
	}

	// return result
return_result:
	return res.f;

}


/**
 * \brief Эталонное вычисление квадратного корня
 * \param x Значение аргумента
 * \return sqrt(x)
 */
float k128cp2elfun_sqrt_gold (float x) {

	k128cp2elfun_single_t res;

	// return nan for negative values
	if (x < 0) {
		res.f = NAN;
		goto return_result;
	}

	// switch for non-negative values
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;       break;
		case FP_INFINITE  :  res.f = INFINITY;  break;
		case FP_ZERO      :  res.f = x;         break;
		case FP_SUBNORMAL :  res.f = 0.0;       break;
		case FP_NORMAL    :  res.f = sqrtf (x); break;
		default: /* internal error */ assert(0==1); break;
	}

	// return result
return_result:
	return res.f;

}


