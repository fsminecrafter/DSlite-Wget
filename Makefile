#---------------------------------------------------------------------------------
# DS Lite wget-like downloader
# Requires devkitARM + libnds + dswifi
# Install devkitPro: https://devkitpro.org/wiki/Getting_Started
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
  $(error "DEVKITPRO not set. Source /etc/profile.d/devkit-env.sh or set manually.")
endif
ifeq ($(strip $(DEVKITARM)),)
  $(error "DEVKITARM not set. Source /etc/profile.d/devkit-env.sh or set manually.")
endif

include $(DEVKITARM)/ds_rules

TARGET   := dswget
BUILD    := build
SOURCES  := source
INCLUDES := include
DATA     :=

ARCH     := -mthumb -mthumb-interwork

CFLAGS   := -g -Wall -O2 \
             -march=armv5te -mtune=arm946e-s \
             $(ARCH) \
             -fomit-frame-pointer -ffast-math \
             $(INCLUDE) \
             -DARM9

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS     := -ldswifi9 -lfat -lnds9
LIBDIRS  := $(LIBNDS) $(PORTLIBS)

include $(DEVKITARM)/base_rules

VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
            $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
            -I$(CURDIR)/$(BUILD)

LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
OUTPUT   := $(CURDIR)/$(TARGET)

.PHONY: $(BUILD) clean

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).map

$(OUTPUT).nds: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPSDIR)/*.d
