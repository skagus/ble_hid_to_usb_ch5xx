################################################################################
RM := rm -rf
GNU_PREF = riscv-none-embed-
# PREFIX ?= riscv-none-embed-
CC := $(GNU_PREF)gcc

CHIP ?= CH573


# All of the sources participating in the build are defined here
OUTDIR := out
TARGET := $(OUTDIR)/usb_ble_brdg
DEFINE := -DDEBUG=1 -DCLK_OSC32K=0

ifeq ($(CHIP),CH573)
	DEFINE += -DCH573
	SDK_TOP := ch573
else ifeq ($(CHIP),CH582)
	DEFINE += -DCH582
	SDK_TOP := ch582
else
	$(error Unknown CHIP $(CHIP))
endif

INC += -I app
INC += -I $(SDK_TOP)/rvmsis
INC += -I $(SDK_TOP)/StdPeriphDriver\inc
INC += -I $(SDK_TOP)/ble/hal/include
INC += -I $(SDK_TOP)/ble/lib

LIBS := -L$(SDK_TOP)/StdPeriphDriver
LIBS += -L$(SDK_TOP)/ble/lib

ifeq ($(CHIP),CH573)
	LIBS += -lISP573 -lCH57XBLE
else
	LIBS += -lISP583 -lCH58XBLE
endif


C_SRCS := $(wildcard app/*.c)
#C_SRCS += $(wildcard rvmsis/*.c)
#C_SRCS += $(wildcard profile/*.c)

#C_SRCS += $(wildcard hal/*.c)
C_SRCS += \
	$(SDK_TOP)/ble/hal/MCU.c \
	$(SDK_TOP)/ble/hal/RTC.c \
	$(SDK_TOP)/ble/hal/LED.c

C_SRCS += $(wildcard $(SDK_TOP)/StdPeriphDriver/*.c)

ASM_SRCS := $(wildcard $(SDK_TOP)/startup/*.S)



C_OBJS_TMP = $(patsubst %.c,%.o,$(C_SRCS))
C_OBJS := $(addprefix out/,$(C_OBJS_TMP))	# replace ".." to "."

ASM_OBJS_TMP += $(patsubst %.S,%.o,$(ASM_SRCS))
ASM_OBJS := $(addprefix out/,$(ASM_OBJS_TMP))	# replace ".." to "."

OBJS := $(C_OBJS) $(ASM_OBJS)

C_DEPS := $(subst .o,.d,$(C_OBJS))
ASM_DEPS := $(subst .o,.d,$(ASM_OBJS))
DEPS := $(C_DEPS) $(ASM_DEPS)

#C_FLAGS += -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore 
C_FLAGS += -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8

C_FLAGS += -fsigned-char -ffunction-sections -fno-common
C_FLAGS += -Os -g -std=gnu99 -Wall -Wno-comment $(DEFINE) 
#C_FLAGS += -save-temps  # 중간파일 안지운다.
#C_FLAGS += -DDEBUG=1

LD_FLAGS := -g -T $(SDK_TOP)/ld/link.ld -nostartfiles -Xlinker --gc-sections -Xlinker --print-memory-usage --specs=nano.specs --specs=nosys.specs

TARGET_FILES := $(TARGET).elf $(TARGET).hex $(TARGET).bin $(TARGET).lst $(TARGET).map

DIR_GUARD = @mkdir -p $(@D)

# All Target
all:  $(TARGET_FILES) $(TARGET).size

# Each subdirectory must supply rules for building sources it contributes

$(OUTDIR)/app/%.o: app/%.c
	@echo "CC:" $<
	$(DIR_GUARD)
	$(CC) $(C_FLAGS) $(INC) -MMD -MP -MF$(@:%.o=%.d) -MT$@ -c $< -o $@

$(OUTDIR)/$(SDK_TOP)/ble/hal/%.o: $(SDK_TOP)/ble/hal/%.c
	@echo "CC:" $<
	$(DIR_GUARD)
	$(CC) $(C_FLAGS) $(INC) -MMD -MP -MF$(@:%.o=%.d) -MT$@ -c $< -o $@

$(OUTDIR)/$(SDK_TOP)/ble/profile/%.o: $(SDK_TOP)/ble/profile/%.c
	@echo "CC:" $<
	$(DIR_GUARD)
	$(CC) $(C_FLAGS) $(INC) -MMD -MP -MF$(@:%.o=%.d) -MT$@ -c $< -o $@

$(OUTDIR)/$(SDK_TOP)/rvmsis/%.o: $(SDK_TOP)/rvmsis/%.c
	@echo "CC:" $<
	$(DIR_GUARD)
	$(CC) $(C_FLAGS) $(INC) -MMD -MP -MF$(@:%.o=%.d) -MT$(@) -c $< -o $@
	
$(OUTDIR)/$(SDK_TOP)/StdPeriphDriver/%.o: $(SDK_TOP)/StdPeriphDriver/%.c
	@echo "CC:" $<
	$(DIR_GUARD)
	$(CC) $(C_FLAGS) $(INC) -MMD -MP -MF$(@:%.o=%.d) -MT$(@) -c $< -o $@

$(OUTDIR)/$(SDK_TOP)/startup/%.o: $(SDK_TOP)/startup/%.S
	@echo "ASM:" $<
	$(DIR_GUARD)
	$(CC) $(C_FLAGS) -x assembler-with-cpp -MMD -MP -MF$(@:%.o=%.d) -MT$(@) -c $< -o $@ 

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

# Add inputs and outputs from these tool invocations to the build variables 

# Tool invocations

$(TARGET).elf: $(OBJS)
	echo "Making:" $@
	$(CC) $(LD_FLAGS) -Wl,--cref,-Map,$(TARGET).map $(OBJS) -o $@  $(LIBS)

$(TARGET).hex: $(TARGET).elf
	@echo "Making:" $@
	@$(GNU_PREF)objcopy -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	@echo "Making:" $@
	@$(GNU_PREF)objcopy -O binary $< $@

$(TARGET).lst: $(TARGET).elf
	@echo "Making:" $@
	@$(GNU_PREF)objdump --source --all-headers --demangle --line-numbers --wide $< > $@

$(TARGET).size: $(TARGET).elf
	@$(GNU_PREF)size --format=berkeley $<

# Other Targets
clean:
	@$(RM) $(TARGET_FILES) $(OUTDIR)/*

