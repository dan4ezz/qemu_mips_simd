# Makefile for CP2 library building.

CFLAGS = -fPIC -Wall -g -O0 -DHOST_BIG_ENDIAN=0
C_INCLUDES = -I$(BUILDDIR)/deps/common/include -I$(BUILDDIR)/deps/k64/include \
 			-I$(BUILDDIR)/../include/fpu \
 			-I$(BUILDDIR)/k128cp2elfun/include -I/usr/include/libxml2

K128CP2ELFUN_DIR = k128cp2elfun

COMMON_OBJS += insn_code.o instr.o \
			machine.o observer.o \
			options.o prockern_access.o \
			prockern.o system.o

K128CP2ELFUN_OBJS += $(K128CP2ELFUN_DIR)/k128cp2elfun_atanc.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_common.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_exp2.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_log2c.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_recip.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_rrcos.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_rrlog2.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_rrsin.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_sinc.o \
	$(K128CP2ELFUN_DIR)/k128cp2elfun_sqrt.o

%.o: %.c
	$(CC) $(CFLAGS) $(C_INCLUDES) -c $< -o $@

.PHONY: all clean

all: $(COMMON_OBJS) $(K128CP2ELFUN_OBJS)
	$(AR) rcs libcp2.a $(COMMON_OBJS) $(K128CP2ELFUN_OBJS)

clean:
	rm -f $(COMMON_OBJS) $(K128CP2ELFUN_OBJS)
	rm -f libcp2.a
