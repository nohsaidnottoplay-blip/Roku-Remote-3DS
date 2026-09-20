# Roku Remote 3DS - devkitPro / libctru build
.DEFAULT_GOAL := all
ifeq ($(strip $(DEVKITPRO)),)
$(error Open a devkitPro shell or set DEVKITPRO to your devkitPro directory)
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
include $(DEVKITARM)/3ds_rules

TARGET := RokuRemote3DS
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := $(ARCH) -std=gnu11 -O2 -Wall -Wextra -Werror -mword-relocations -ffunction-sections -fdata-sections -D__3DS__ -Iinclude -I$(CTRULIB)/include
LDFLAGS := $(ARCH) -specs=3dsx.specs -Wl,--gc-sections -Wl,-Map,build/$(TARGET).map
OBJECTS := $(patsubst source/%.c,build/%.o,$(wildcard source/*.c))

.PHONY: all clean
all: $(TARGET).3dsx

build:
	mkdir -p build

build/%.o: source/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -L$(CTRULIB)/lib -lctru -lm -o $@

$(TARGET).smdh: icon.png Makefile
	smdhtool --create "Roku Remote 3DS" "A personal Wi-Fi Roku remote" "Homebrew project" icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	3dsxtool $< $@ --smdh=$(TARGET).smdh

clean:
	rm -rf build $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh

-include $(OBJECTS:.o=.d)
