###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=v90s make)
endif

###########################################################

# The V90S stock OS ships SDL2 2.30 (+_image/_ttf) with its custom "fbgl"
# EGL-framebuffer driver, so unlike h700 there is no in-tree SDL2 build:
# binaries link against the toolchain's SDL2 and run against stock's.

REQUIRES_EVTEST=other/evtest
REQUIRES_JSTEST=other/jstest
REQUIRES_UNZIP60=other/unzip60
REQUIRES_FILEMGR=other/NextCommander
FILEMGR_PATCH=patches/NextCommander-v90s.patch
FILEMGR_STAMP=.patched-v90s-v3

all: readmes

early: $(REQUIRES_EVTEST) $(REQUIRES_JSTEST) $(REQUIRES_UNZIP60) $(REQUIRES_FILEMGR)
	mkdir -p other
	cd $(REQUIRES_EVTEST) && $(CROSS_COMPILE)gcc -o evtest evtest.c
	cd $(REQUIRES_JSTEST) && $(CROSS_COMPILE)gcc -o jstest jstest.c
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	cd $(REQUIRES_FILEMGR) && ([ -f $(FILEMGR_STAMP) ] || (git checkout -- . && git apply --unidiff-zero ../../$(FILEMGR_PATCH) && touch $(FILEMGR_STAMP)))
	cd $(REQUIRES_FILEMGR) && ([ -f output/.platform-v90s ] || make clean PLATFORM=v90s)
	cd $(REQUIRES_FILEMGR) && make -j PLATFORM=v90s && touch output/.platform-v90s

clean:
	cd $(REQUIRES_FILEMGR) && make clean PLATFORM=v90s
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart clean

###########################################################

$(REQUIRES_FILEMGR):
	git clone --depth 1 https://github.com/LoveRetro/NextCommander.git $(REQUIRES_FILEMGR)

$(REQUIRES_EVTEST):
	git clone --depth 1 https://github.com/freedesktop-unofficial-mirror/evtest.git $(REQUIRES_EVTEST)

$(REQUIRES_JSTEST):
	git clone --depth 1 https://github.com/datrh/joyutils.git $(REQUIRES_JSTEST)

$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

include ../all/readmes/makefile
