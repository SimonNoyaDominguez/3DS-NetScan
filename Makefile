#---------------------------------------------------------------------------------
TARGET		:=	scanner
BUILD		:=	build
SOURCES		:=	source

#---------------------------------------------------------------------------------
# Herramientas
#---------------------------------------------------------------------------------
CC		:=	arm-none-eabi-gcc
CXX		:=	arm-none-eabi-g++
LD		:=	arm-none-eabi-gcc

#---------------------------------------------------------------------------------
# Rutas de devkitPro
#---------------------------------------------------------------------------------
DEVKITPRO	:= /opt/devkitpro
DEVKITARM	:= $(DEVKITPRO)/devkitARM
CTRULIB		:= $(DEVKITPRO)/libctru

#---------------------------------------------------------------------------------
# Opciones
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections -fdata-sections \
			$(ARCH) \
			-I$(CTRULIB)/include

LDFLAGS	:=	-specs=3dsx.specs -g $(ARCH) \
			-L$(CTRULIB)/lib

LIBS	:= -lctru -lm

#---------------------------------------------------------------------------------
# Reglas
#---------------------------------------------------------------------------------
CFILES := $(wildcard $(SOURCES)/*.c)
OBJS := $(addprefix $(BUILD)/, $(notdir $(CFILES:.c=.o)))

.PHONY: all clean

all: $(TARGET).3dsx

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(TARGET).3dsx: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/3dsxtool $< $@

clean:
	rm -f $(TARGET).3dsx $(TARGET).elf
	rm -rf $(BUILD)
