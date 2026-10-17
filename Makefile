#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Configuración del Proyecto y Datos de la App
#---------------------------------------------------------------------------------
TARGET          := 3DSocial
APP_TITLE       := 3DSocial
APP_DESCRIPTION := App Social de Comunicación para 3DS
APP_AUTHOR      := Miventech

BUILD           := build
SOURCES         := source/app source/gui source/system source/network source/media
DATA            := data
INCLUDES        := include source source/app source/gui source/system source/network source/media ctr-led-brary/source ctr-led-brary/include ctr-led-brary/lib
GRAPHICS        := gfx
GFXBUILD        := $(BUILD)

#---------------------------------------------------------------------------------
# Opciones de Compilación
#---------------------------------------------------------------------------------
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS  := -g -Wall -O2 -mword-relocations \
           -ffunction-sections \
           $(ARCH)

CFLAGS  += $(INCLUDE) -D__3DS__

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS := -g $(ARCH)
LDFLAGS =  -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS := -lctr-led-brary -lcitro2d -lcitro3d -lctru -lm

LIBDIRS := $(CTRULIB) ctr-led-brary/lib

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES    := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD   := $(CC)
else
    export LD   := $(CXX)
endif

ifeq ($(GFXBUILD),$(BUILD))
export T3XFILES := $(GFXFILES:.t3s=.t3x)
else
export ROMFS_T3XFILES   := $(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES        := $(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))
endif

export OFILES_SOURCES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN   := $(addsuffix .o,$(BINFILES)) \
                       $(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
                       $(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES   := $(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
                   $(addsuffix .h,$(subst .,_,$(BINFILES))) \
                   $(GFXFILES:.t3s=.h)

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(CURDIR)/ctr-led-brary/lib

export _3DSXDEPS    := $(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
    icons := $(wildcard *.png)
    ifneq (,$(findstring $(TARGET).png,$(icons)))
        export APP_ICON := $(TOPDIR)/$(TARGET).png
    else
        ifneq (,$(findstring icon.png,$(icons)))
            export APP_ICON := $(TOPDIR)/icon.png
        endif
    endif
else
    export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
    export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

.PHONY: all clean cia led-library

#---------------------------------------------------------------------------------
# Reglas Ejecutadas desde la Raíz
#---------------------------------------------------------------------------------
all: led-library $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

led-library:
	@$(MAKE) --no-print-directory -C ctr-led-brary

cia: all
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).cia

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
clean:
	@echo Limpiando archivos generados...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(GFXBUILD) $(OUTPUT).bnr $(OUTPUT).cia

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x   $(BUILD)/%.h    :   %.t3s
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# Reglas Internas (se ejecutan dentro del directorio /build)
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx  :   $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf   :   $(OFILES)

# Banner para makerom
$(OUTPUT).bnr: $(TOPDIR)/banner.png $(TOPDIR)/banner.wav
	@echo "Generando banner (.bnr)..."
	@bannertool makebanner --image "$<" --audio "$(TOPDIR)/banner.wav" --output "$@"

# Generación final del archivo .cia
$(OUTPUT).cia: $(OUTPUT).elf $(OUTPUT).smdh $(OUTPUT).bnr $(TOPDIR)/app.rsf
	@echo "Empaquetando archivo .CIA con makerom..."
	@makerom -f cia -o "$@" -elf "$<" -rsf "$(TOPDIR)/app.rsf" -icon "$(OUTPUT).smdh" -banner "$(OUTPUT).bnr"

#---------------------------------------------------------------------------------
%.bin.o %_bin.h :   %.bin
	@echo $(notdir $<)
	@$(bin2o)

.PRECIOUS   :   %.t3x %.shbin
%.t3x.o %_t3x.h :   %.t3x
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

%.shbin.o %_shbin.h : %.shbin
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
