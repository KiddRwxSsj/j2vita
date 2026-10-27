TITLE_ID := JTVA00001
TITLE := J2Vita
VERSION ?= 0.9.0
APP_VER ?= 00.90
# Timidity patch set, see ../scripts/fetch-freepats.sh
FREEPATS ?= ../build/freepats
# splash-free vitaGL, see ../scripts/build-vitagl.sh
VITAGL ?= ../build/vitagl

VITASDK ?= /usr/local/vitasdk
PREFIX := $(VITASDK)/bin/arm-vita-eabi
CC := $(PREFIX)-gcc

ROOT := ..
JCOUT := $(ROOT)/javacall/configuration/phoneMEFeature/vita_arm/output
MIDP_OUT := $(ROOT)/midp/build/javacall_vita/output
RUNTIME := $(MIDP_OUT)

CFLAGS := -O2 -Wall -D__vita__ -DARM -DJ2V_VERSION=\"$(VERSION)\" \
	-I$(VITAGL)/src/source -I$(JCOUT)/inc -I$(VITASDK)/arm-vita-eabi/include/SDL2
LDFLAGS := -Wl,-q -L$(VITAGL) \
	-L$(MIDP_OUT)/bin/arm \
	-L$(ROOT)/cldc/build/javacall_vita_gcc/target/bin \
	-L$(ROOT)/pcsl/output-vita/javacall_arm/lib \
	-L$(JCOUT)/lib
LIBS := -Wl,--start-group \
	-lmidp -lcldc_vm \
	-lpcsl_file -lpcsl_print -lpcsl_memory -lpcsl_string -lpcsl_network \
	-ljavacall \
	-Wl,--end-group \
	-lSDL2_mixer -lopusfile -lopus -lvorbisfile -lvorbis -logg -lmodplug -lxmp-lite -lSDL2 \
	-lfreetype -lpng16 -lz -lbz2 \
	-lvitaGL -lvitashark -lSceShaccCgExt -lmathneon -ltaihen_stub \
	-lSceShaccCg_stub -lSceKernelDmacMgr_stub -lSceGxm_stub -lSceDisplay_stub \
	-lSceCtrl_stub -lSceTouch_stub -lScePower_stub -lSceAudio_stub -lSceAudioIn_stub \
	-lSceAppMgr_stub -lSceAppUtil_stub -lSceCommonDialog_stub -lSceSysmodule_stub \
	-lSceIme_stub -lSceHid_stub -lSceMotion_stub -lSceIofilemgr_stub \
	-lpthread -lstdc++ -lm -lc

OBJS := main.o config.o video.o input.o menu.o theme.o compat.o

all: j2vita.vpk

%.o: %.c j2vita.h
	$(CC) $(CFLAGS) -c $< -o $@

LIBDEPS := $(MIDP_OUT)/bin/arm/libmidp.a $(JCOUT)/lib/libjavacall.a \
	$(ROOT)/cldc/build/javacall_vita_gcc/target/bin/libcldc_vm.a $(VITAGL)/libvitaGL.a

j2vita.elf: $(OBJS) $(LIBDEPS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

j2vita.velf: j2vita.elf
	$(VITASDK)/bin/vita-elf-create $< $@

eboot.bin: j2vita.velf
	$(VITASDK)/bin/vita-make-fself -s $< $@

param.sfo: Makefile
	$(VITASDK)/bin/vita-mksfoex -s TITLE_ID=$(TITLE_ID) -s APP_VER=$(APP_VER) "$(TITLE)" $@

j2vita.vpk: eboot.bin param.sfo j2vita.ini
	@test -d $(RUNTIME)/lib && test -d $(RUNTIME)/appdb || { echo "run ../build.sh first"; exit 1; }
	@test -f $(FREEPATS)/timidity.cfg || { echo "missing $(FREEPATS), run ../scripts/fetch-freepats.sh $(FREEPATS)"; exit 1; }
	@set -e; args=""; \
	for d in lib appdb; do \
		for f in $$(cd $(RUNTIME) && find $$d -type f); do args="$$args -a $(RUNTIME)/$$f=runtime/$$f"; done; \
	done; \
	for f in $$(cd $(FREEPATS) && find . -type f); do args="$$args -a $(FREEPATS)/$$f=timidity/$$f"; done; \
	for f in $$(find sce_sys -type f); do args="$$args -a $$f=$$f"; done; \
	$(VITASDK)/bin/vita-pack-vpk -s param.sfo -b eboot.bin -a j2vita.ini=runtime/j2vita.ini $$args $@

clean:
	rm -f $(OBJS) j2vita.elf j2vita.velf eboot.bin param.sfo j2vita.vpk

.PHONY: all clean
