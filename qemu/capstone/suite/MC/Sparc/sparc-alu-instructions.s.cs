# CS_ARCH_SPARC, CS_MODE_BIG_ENDIAN, None
0x80,0x00,0x00,0x00 = add %g0, %g0, %g0
0x86,0x00,0x40,0x02 = add %g1, %g2, %g3
0xa0,0x02,0x00,0x09 = add %o0, %o1, %l0
0xa0,0x02,0x20,0x0a = add %o0, 10,  %l0
0x86,0x80,0x40,0x02 = addcc %g1, %g2, %g3
0x86,0xc0,0x40,0x02 = addxcc %g1, %g2, %g3
0x86,0x70,0x40,0x02 = udiv %g1, %g2, %g3
0x86,0x78,0x40,0x02 = sdiv %g1, %g2, %g3
0x86,0x08,0x40,0x02 = and %g1, %g2, %g3
0x86,0x28,0x40,0x02 = andn %g1, %g2, %g3
0x86,0x10,0x40,0x02 = or %g1, %g2, %g3
0x86,0x30,0x40,0x02 = orn %g1, %g2, %g3
0x86,0x18,0x40,0x02 = xor %g1, %g2, %g3
0x86,0x38,0x40,0x02 = xnor %g1, %g2, %g3
0x86,0x50,0x40,0x02 = umul %g1, %g2, %g3
0x86,0x58,0x40,0x02 = smul %g1, %g2, %g3
0x01,0x00,0x00,0x00 = nop 
0x21,0x00,0x00,0x0a = sethi 10, %l0
0x87,0x28,0x40,0x02 = sll %g1, %g2, %g3
0x87,0x28,0x60,0x1f = sll %g1, 31, %g3
0x87,0x30,0x40,0x02 = srl %g1, %g2, %g3
0x87,0x30,0x60,0x1f = srl %g1, 31, %g3
0x87,0x38,0x40,0x02 = sra %g1, %g2, %g3
0x87,0x38,0x60,0x1f = sra %g1, 31, %g3
0x86,0x20,0x40,0x02 = sub %g1, %g2, %g3
0x86,0xa0,0x40,0x02 = subcc %g1, %g2, %g3
0x86,0xe0,0x40,0x02 = subxcc %g1, %g2, %g3
0x86,0x10,0x00,0x01 = or %g0, %g1, %g3
0x86,0x10,0x20,0xff = or %g0, 255, %g3
0x81,0xe8,0x00,0x00 = restore 
0x86,0x40,0x80,0x01 = addx %g2, %g1, %g3
0x86,0x60,0x80,0x01 = subx %g2, %g1, %g3
0x86,0xd0,0x80,0x01 = umulcc %g2, %g1, %g3
0x86,0xd8,0x80,0x01 = smulcc %g2, %g1, %g3
0x86,0xf0,0x80,0x01 = udivcc %g2, %g1, %g3
0x86,0xf8,0x80,0x01 = sdivcc %g2, %g1, %g3
0x86,0x88,0x80,0x01 = andcc %g2, %g1, %g3
0x86,0xa8,0x80,0x01 = andncc %g2, %g1, %g3
0x86,0x90,0x80,0x01 = orcc %g2, %g1, %g3
0x86,0xb0,0x80,0x01 = orncc %g2, %g1, %g3
0x86,0x98,0x80,0x01 = xorcc %g2, %g1, %g3
0x86,0xb8,0x80,0x01 = xnorcc %g2, %g1, %g3
0x87,0x00,0x80,0x01 = taddcc %g2, %g1, %g3
0x87,0x08,0x80,0x01 = tsubcc %g2, %g1, %g3
0x87,0x10,0x80,0x01 = taddcctv %g2, %g1, %g3
0x87,0x18,0x80,0x01 = tsubcctv %g2, %g1, %g3
