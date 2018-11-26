#ifndef MIPS_CPV_H
#define MIPS_CPV_H

#define mdmx_base(i)           ((uint16_t) ((i >> 21) & 0x01f))
#define mdmx_index(i)          ((uint16_t) ((i >> 16) & 0x01f))
#define extend_sign_11b(i) \
    ((int16_t)  (((i) & 0x400) ? ((i) | 0xF800) : (i)))

#define extend_sign_18b(i) \
    ((int32_t)  (((i) & 0x20000) ? ((i) | 0xFFFC0000) : (i)))

#define mdmx_offset(i) \
    extend_sign_11b((uint16_t) (((i >> 4) & 0x07) | (((i >> 13) & 0x0ff) << 3)))

#define cpv_vb_offset(i)       extend_sign_18b((i & 0xFFFF) << 2)

#define cpv_prmL(i)            ((uint16_t) ((i >> 19) & 0x03))
#define cpv_sL(i)              ((uint16_t) ((i >> 21) & 0x01))
#define cpv_prmH(i)            ((uint16_t) ((i >> 22) & 0x03))
#define cpv_sH(i)              ((uint16_t) ((i >> 24) & 0x01))
#define cpv_scalen(i)          ((uint16_t) ((i >> 19) & 0x003))

#define cpv_modH(i) \
    ((uint16_t) (((i >> 6) & 0x01) | ((i >> 19) & 0x02)))

#define cpv_modL(i)            ((uint16_t) ((i >> 4) & 0x03))

#define cpv_cc2(i)             ((uint16_t) ((i >> 20) & 0x01))
#define cpv_tf2(i)             ((uint16_t) ((i >> 6) & 0x01))

#define cpv_cond_bit(i, opcode) \
    ((uint16_t) ((((opcode >> 3) & 0x0f) >> i) & 0x1))

#define cpv_valuen(i)          ((int8_t)   ((i & 0x07f) | ((i & 0x040) << 1)))
#define cpv_sem(i)             ((uint16_t) ((i >> 4) & 0x007))
#define cpv_cc(i)              ((uint16_t) ((i >> 18) & 0x03))
#define cpv_mask(i)            ((uint16_t) ((i >> 8) & 0x0ff))
#define cpv_br_cc(i)           ((uint16_t) ((i >> 18) & 0x07))

#define cpv_var_4_3(v)         ((uint16_t) (((v) >> 4) & 0x007))
#define cpv_var_5_2(v)         ((uint16_t) (((v) >> 5) & 0x003))
#define cpv_var_19_4(v)        ((uint16_t) (((v) >> 19) & 0x00f))
#define cpv_var_13_3(v)        ((uint16_t) (((v) >> 13) & 0x007))

#define MDMX2_FT(v)            ((uint16_t) ((v >> 19) & 0x3f))
#define MDMX2_FS(v)            ((uint16_t) ((v >> 13) & 0x3f))
#define MDMX2_FD(v)            ((uint16_t) ((v >> 7) & 0x3f))
#define MDMX2_SD(v)            ((uint16_t) ((v >> 5) & 0x01))
#define MDMX2_RT(v)            ((uint16_t) ((v >> 16) & 0x01f))
#define MDMX_FD(v)             MDMX2_FD(v)

/* Control Registers */
#define CPV_VCIR    0
#define CPV_VCSR    31

/* for MOVE inst */
#define MOVE_HH 0
#define MOVE_HL 1
#define MOVE_LH 2
#define MOVE_LL 3

/* for VLD/VSD/VLDX/VSDX inst */
#define TYPE_X 1

enum {
    OPC_VCFC                 = 0x74000000,
    OPC_VMFH                 = 0x74000001,
    OPC_VMFHFP               = 0x74000002,
    OPC_VMFL                 = 0x74000003,
    OPC_VMFLFP               = 0x74000004,
    OPC_VCTC                 = 0x74000008,
    OPC_VMTH                 = 0x74000009,
    OPC_VMTHFP               = 0x7400000a,
    OPC_VMTL                 = 0x7400000b,
    OPC_VMTLFP               = 0x7400000c,
    OPC_VMFCC                = 0x74000010,
    OPC_VMTCC                = 0x74000018,
    OPC_VCMP                 = 0x74200000,
    OPC_IFSET                = 0x74200001,
    OPC_IFMOVE               = 0x74200009,
    OPC_VSCALE               = 0x74800000,
    OPC_VGET                 = 0x74a00000,
    OPC_VSWAP                = 0x74c00000,
    OPC_VBF                  = 0x75000000,
    OPC_VBT                  = 0x75010000,
    OPC_VBFL                 = 0x75020000,
    OPC_VBTL                 = 0x75030000,
    OPC_VZERO                = 0x75200000,
    OPC_SGNMOD               = 0x75000001,
    OPC_VMAX                 = 0x75200002,
    OPC_VMIN                 = 0x75200003,
    OPC_VRECIP               = 0x75200008,
    OPC_VRSQRT               = 0x75200009,
    OPC_VMOST                = 0x7520000a,
    OPC_VLEAST               = 0x7520000b,
    OPC_VADD                 = 0x76000000,
    OPC_VMADD                = 0x76000001,
    OPC_CMADD                = 0x76000002,
    OPC_CHMADD               = 0x76000003,
    OPC_MVMADD               = 0x76000004,
    OPC_MTVMADD              = 0x76000005,
    OPC_CHW                  = 0x76000006,
    OPC_MOVEHH               = 0x76000007,
    OPC_VSUB                 = 0x76000008,
    OPC_VMSUB                = 0x76000009,
    OPC_CMSUB                = 0x7600000a,
    OPC_CHMSUB               = 0x7600000b,
    OPC_MVMSUB               = 0x7600000c,
    OPC_MTVMSUB              = 0x7600000d,
    OPC_MOVEHL               = 0x7600000f,
    OPC_VMUL                 = 0x76000010,
    OPC_VMUL4                = 0x76000011,
    OPC_CMUL                 = 0x76000012,
    OPC_CHMUL                = 0x76000013,
    OPC_MVMUL                = 0x76000014,
    OPC_MTVMUL               = 0x76000015,
    OPC_PRM                  = 0x76000016,
    OPC_MOVELH               = 0x76000017,
    OPC_VADDSUB              = 0x76000018,
    OPC_VMADDSUB             = 0x76000019,
    OPC_CMADDSUB             = 0x7600001a,
    OPC_CONV                 = 0x7600001b,
    OPC_SMUL                 = 0x7600001c,
    OPC_CMAGSQ2              = 0x7600001d,
    OPC_VSUM                 = 0x7600001e,
    OPC_MOVELL               = 0x7600001f,
    OPC_CSD                  = 0x76800006,
    OPC_VLD                  = 0x78000000,
    OPC_VSD                  = 0x78000001,
    OPC_VLDD                 = 0x78000002,
    OPC_VSDD                 = 0x78000003,
    OPC_VLDM                 = 0x78000004,
    OPC_VSDM                 = 0x78000005,
    OPC_VLDX                 = 0x78000006,
    OPC_VSDX                 = 0x78000007,
    OPC_VLDH                 = 0x78000008,
    OPC_VSDH                 = 0x78000009,
    OPC_VLDDH                = 0x7800000a,
    OPC_VSDDH                = 0x7800000b,
    OPC_VLDQ                 = 0x7800000c,
    OPC_VSDQ                 = 0x7800000d,
    OPC_VLDMX                = 0x7800000e,
    OPC_VSDMX                = 0x7800000f,
    OPC_VLDHX                = 0x78000016,
    OPC_VSDHX                = 0x78000017,
    OPC_VLDLX                = 0x7800001e,
    OPC_VLDQX                = 0x78000046,
    OPC_VSDQX                = 0x78000047,
    OPC_VLDDX                = 0x7800004e,
    OPC_VSDDX                = 0x7800004f,
    OPC_VLDDHX               = 0x78000056,
    OPC_VSDDHX               = 0x78000057,
    OPC_VLDDLX               = 0x7800005e,
};

#endif /* MIPS_CPV_H */
