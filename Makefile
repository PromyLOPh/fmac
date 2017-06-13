TARGET = remote_spi
# must be before includes, otherwise dottedline’s targets will become default
all: $(TARGET)

BOARD=xmc4500relax
#BOARD=csmtda

# these are required for xmclib
ifeq ($(BOARD),xmc4500relax)
# xmc4500
UC = XMC4500
UC_TYPE = F100
UC_MEM = 1024
TOOLCHAIN = arm-none-eabi
CPU = cortex-m4
FABI = hard
FPU = fpv4-sp-d16
endif

ifeq ($(BOARD),csmtda)
# xmc1100
UC = XMC1100
UC_TYPE = Q024
UC_MEM = 0032
TOOLCHAIN = arm-none-eabi
CPU = cortex-m0
FABI = soft
# no FPU
endif

include src/rtt/rtt.mk
include src/bitbite/bitbite.mk
include src/prettylewis/prettylewis.mk
include src/dottedline/dottedline.mk
include src/xmclib/xmclib.mk

#### Setup ####
SRC = $(wildcard src/*.c) $(RTT_SRC) $(BITBITE_SRC) $(PRETTYLEWIS_SRC) $(DOTTEDLINE_SRC) $(XMCLIB_SRC)
GDB_ARGS = -ex "target extended-remote :3333" -ex "monitor reset"

CC   = $(TOOLCHAIN)-gcc
CP   = $(TOOLCHAIN)-objcopy
OD   = $(TOOLCHAIN)-objdump
GDB  = $(TOOLCHAIN)-gdb
SIZE = $(TOOLCHAIN)-size

CPUFLAGS=-mthumb -mcpu=$(CPU) -mfloat-abi=$(FABI)
ifdef FPU
CPUFLAGS+=-mfpu=$(FPU)
endif
CFLAGS = $(CPUFLAGS)
CFLAGS+= -Os -ffunction-sections -fdata-sections
CFLAGS+= -MD -std=c11 -Wall -Werror
CFLAGS+= -DXMC_ASSERT_ENABLE -DXMC_USER_ASSERT_FUNCTION
CFLAGS+= -ggdb3
#CFLAGS+= -fstack-protector-all
#CFLAGS+= -DNDEBUG
CFLAGS+= $(RTT_INC) $(BITBITE_INC) $(PRETTYLEWIS_INC) $(DOTTEDLINE_INC) $(XMCLIB_INC)
LFLAGS = -Wl,--gc-sections -nostartfiles
# use thumb mode libraries for this specific target
LFLAGS+=$(CPUFLAGS)
CPFLAGS = -Obinary
HEXFLAGS = -Oihex
ODFLAGS = -S

OBJS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRC)))
DEPS = $(patsubst %.S,%.d,$(patsubst %.c,%.d,$(SRC)))

-include $(DEPS)

%.o: %.S
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

bin:
	mkdir -p bin/

bin/$(TARGET).axf: $(OBJS) | bin
	$(CC) -T $(XMCLIB_LINKERSCRIPT) $(LFLAGS) -o $@ $(OBJS)

bin/$(TARGET).ihex: bin/$(TARGET).axf | bin
	$(CP) $(HEXFLAGS) $< $@

bin/$(TARGET).bin: bin/$(TARGET).axf | bin
	$(CP) $(CPFLAGS) $< $@

$(TARGET): bin/$(TARGET).axf bin/$(TARGET).ihex
	$(SIZE) bin/$(TARGET).axf

gdb: $(TARGET)
	$(GDB) bin/$(TARGET).axf $(GDB_ARGS)

clean:
	$(RM) $(OBJS) $(DEPS) bin/*

