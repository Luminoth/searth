# variables
OBJECTS = $(foreach object, $(basename $(wildcard *.cc)), $(OBJDIR)/$(object).o)

# compiling vars
INCLUDE += -I../$(INCDIR) -I../$(ENGINEDIR) -I../$(ENGINEDIR)/gui
SDL_FLAGS = `sdl-config --cflags`
LIB += `sdl-config --libs` -L/usr/X11R6/lib/ -L../$(ENGINEDIR)/$(LIBDIR)
LD_FLAGS += -lm -lSDL_image -lSDL_mixer -lSDL_ttf -lGL -lGLU -lengine -lenginegui


# profile options
ifeq ($(strip $(PROFILE)), true)
    LD_FLAGS += -pg
endif


# targets
all: $(PROGNAME)

$(PROGNAME): $(OBJDIR) $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LIB) $(LD_FLAGS)

$(OBJDIR):
	mkdir $@

clean:
	-rm -rf $(OBJDIR) $(PROGNAME) core a.out *.log tags

$(OBJDIR)/%.o: %.cc ../$(INCDIR)/%.h
	$(CXX) -o $@ $(INCLUDE) $(CFLAGS) $(SDL_FLAGS) -c $<
