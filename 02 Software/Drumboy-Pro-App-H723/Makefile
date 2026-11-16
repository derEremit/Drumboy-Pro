# Command-line build for the Drumboy Pro application, mirroring the
# STM32CubeIDE "Debug" configuration in .cproject. CubeIDE keeps working
# as before; this exists for CI and for building without the IDE.
#
#   make                         -> build/app.elf, build/app.bin
#   make package                 -> build/Drumboy_Pro_Fw_MM_mm.bin (SD-card update package),
#                                   version from Core/Library/Global/Version.h
#   make package VERSION=0.00    -> same, overriding the version (test builds)
#   make WERROR=1                -> treat warnings in our own code (Core/) as errors, as CI does
#   make PROFILE=1               -> measurement build: shows DSP load, the cost of each stage and I2S errors on screen
#                                   (use a separate BUILD dir, e.g. BUILD=build-profile)
#   make DCACHE=0                -> build without the data cache (Drumboy_Pro_Fw_MM_mm_nc.bin; _dsp_nc with PROFILE=1). The
#                                   cache is on by default since V1.15. The Makefile does not see a change of
#                                   DCACHE or PROFILE: use a clean (or another) BUILD dir after changing them.
#   make version                 -> print the version from Version.h
#
# Requires arm-none-eabi-gcc (e.g. Ubuntu: apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib).

PREFIX ?= arm-none-eabi-
CC      = $(PREFIX)gcc
CXX     = $(PREFIX)g++
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

BUILD  ?= build
TARGET  = $(BUILD)/app

ARCH = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
# The data cache is on (V1.15: the audio load of a typical song fell from 45% to about 15%, same sound);
# make DCACHE=0 builds without it.
DCACHE ?= 1
DEFS = -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H723xx $(if $(PROFILE),-DDSP_PROFILE) $(if $(filter 1,$(DCACHE)),-DENABLE_DCACHE)
INCS = \
	-ICore/Inc \
	-IFATFS/Target \
	-IFATFS/App \
	-IDrivers/STM32H7xx_HAL_Driver/Inc \
	-IDrivers/STM32H7xx_HAL_Driver/Inc/Legacy \
	-IMiddlewares/Third_Party/FatFs/src \
	-IDrivers/CMSIS/Device/ST/STM32H7xx/Include \
	-IDrivers/CMSIS/Include \
	-ICore/Library/Global \
	-ICore/Library/Lcd \
	-ICore/Library/Dac \
	-ICore/Library/Controller

COMMON   = $(ARCH) $(DEFS) $(INCS) -Ofast -ffast-math -ffunction-sections -fdata-sections -Wall --specs=nano.specs -MMD -MP
CFLAGS   = -std=gnu11 $(COMMON)
CXXFLAGS = -std=gnu++14 $(COMMON) -fno-exceptions -fno-rtti -fno-use-cxa-atexit
LDFLAGS  = $(ARCH) -TSTM32H723ZGTX_FLASH.ld --specs=nosys.specs --specs=nano.specs \
           -Wl,--gc-sections -Wl,-Map=$(TARGET).map
LDLIBS   = -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group

SRC_DIRS = Core FATFS Middlewares Drivers
# Sorted: find lists files in filesystem order, which differs between machines and changes the
# link order (and with it the binary). Sorting makes the same source give the same file
# everywhere, so the release built on GitHub is byte-identical to a local build.
C_SRCS   = $(sort $(shell find $(SRC_DIRS) -name '*.c' -not -path '*/Templates/*'))
CXX_SRCS = $(sort $(shell find $(SRC_DIRS) -name '*.cpp'))
ASM_SRCS = $(sort $(shell find $(SRC_DIRS) -name '*.s'))
OBJS     = $(addprefix $(BUILD)/,$(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o) $(ASM_SRCS:.s=.o))

VERSION_FILE = Core/Library/Global/Version.h
VERSION ?= $(shell awk '/define FW_VERSION_MAJOR/{a=$$3} /define FW_VERSION_MINOR/{b=$$3} END{printf "%d.%02d", a, b}' $(VERSION_FILE))
VERSION_MAJOR = $(word 1,$(subst ., ,$(VERSION)))
VERSION_MINOR = $(word 2,$(subst ., ,$(VERSION)))
PACKAGE = $(BUILD)/Drumboy_Pro_Fw_$(shell echo $(VERSION) | awk -F. '{printf "%02d_%02d", $$1, $$2}')$(if $(PROFILE),_dsp)$(if $(filter 1,$(DCACHE)),,_nc).bin

.PHONY: all package version clean

all: $(TARGET).bin

$(TARGET).elf: $(OBJS) STM32H723ZGTX_FLASH.ld
	$(CXX) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

package: $(TARGET).bin
	python3 ../tools/package_firmware.py $< $(PACKAGE) --version $(VERSION)

version:
	@echo $(VERSION)

# WERROR=1 applies -Werror to our own code only; vendor HAL/CMSIS/FatFs keep their warnings.
own_werror = $(if $(WERROR),$(if $(filter Core/%,$<),-Werror))

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(own_werror) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(own_werror) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ARCH) -x assembler-with-cpp -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
