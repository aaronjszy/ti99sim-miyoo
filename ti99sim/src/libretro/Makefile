# TI-99/sim src/libretro Makefile

PROJECT := ti99sim_libretro

SOURCES := \
	libretro-core.cpp \
	osk-retro.cpp \
	ti994a-retro.cpp \
	tms9918a-retro.cpp \
	tms9919-retro.cpp

include ../../rules.mak

ifeq ($(OS),OS_MACOSX)
EXT        := dylib
SHARED_FLG := -dynamiclib -Wl,-exported_symbols_list,exports.txt
else
EXT        := so
SHARED_FLG := -shared -Wl,--version-script=link.T -Wl,--no-undefined
endif

BINDIR  := ../../bin
TARGET  := $(BINDIR)/$(PROJECT).$(EXT)

LIBDIRS := ../core/$(CFG)
LIBS    := ../core/$(CFG)/ti-core.a $(XLIBS) -lm

OBJECTS := $(addprefix $(CFG)/,$(SOURCES:.cpp=.o))

all: $(CFG) $(BINDIR) $(TARGET)

$(BINDIR):
	@mkdir -p $(BINDIR)

clean:
	@rm -Rf $(CFG)
	@rm -f $(BINDIR)/ti99sim_libretro.so $(BINDIR)/ti99sim_libretro.dylib
	@rm -f *.bk *~

dep:
	@makedepend $(INCLUDES) $(SOURCES) 2> /dev/null

$(TARGET): $(OBJECTS) $(filter %.a,$(LIBS))
	@echo Linking $(PROJECT).$(EXT)
	@$(CXX) -o $@ $(SHARED_FLG) $(LFLAGS) $(OBJECTS) $(LIBS)

../core/$(CFG)/ti-core.a:
	@$(MAKE) -C ../core

-include $(OBJECTS:.o=.dep)
