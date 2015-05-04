# general variables
CXX = g++

# application variables
PROGNAME = searth

# source directory variables
SRCDIR = src
INCDIR = include

# creatable directories
BINDIR = bin
OBJDIR = obj

# other directories
LIBDIR = lib
ENGINEDIR = engine
DATADIR = data


# compiler options
BASE_CFLAGS = -Wall -Wstrict-prototypes -Woverloaded-virtual -pipe
DEBUG_CFLAGS = $(BASE_CFLAGS) -DDEBUG -g -fno-default-inline
PROFILE_CFLAGS = $(DEBUG_CFLAGS) -pg
RELEASE_CFLAGS = $(BASE_CFLAGS) -DNDEBUG -O3 -finline-functions -fomit-frame-pointer -ffast-math -fno-common -funroll-loops
PROFILE_RELEASE_CFLAGS = $(BASE_CFLAGS) -DNDEBUG -O3 -finline-functions -ffast-math -fno-common -funroll-loops -pg


# targets
all: $(PROGNAME)

$(PROGNAME) : release

release: $(BINDIR)
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" INCDIR="$(INCDIR)" OBJDIR="$(OBJDIR)" ENGINEDIR="$(ENGINEDIR)" LIBDIR="$(LIBDIR)" PROGNAME="$(PROGNAME)" CFLAGS="$(RELEASE_CFLAGS)"
	mv $(SRCDIR)/$(PROGNAME) $(BINDIR)

profile_release: $(BINDIR)
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" INCDIR="$(INCDIR)" OBJDIR="$(OBJDIR)" ENGINEDIR="$(ENGINEDIR)" LIBDIR="$(LIBDIR)" PROGNAME="$(PROGNAME)" CFLAGS="$(PROFILE_RELEASE_CFLAGS)" PROFILE="true"
	mv $(SRCDIR)/$(PROGNAME) $(BINDIR)/$(PROGNAME).profile

debug: $(BINDIR)
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" INCDIR="$(INCDIR)" OBJDIR="$(OBJDIR)" ENGINEDIR="$(ENGINEDIR)" LIBDIR="$(LIBDIR)" PROGNAME="$(PROGNAME)" CFLAGS="$(DEBUG_CFLAGS)"
	mv $(SRCDIR)/$(PROGNAME) $(BINDIR)/$(PROGNAME).debug

profile_debug: $(BINDIR)
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" INCDIR="$(INCDIR)" OBJDIR="$(OBJDIR)" ENGINEDIR="$(ENGINEDIR)" LIBDIR="$(LIBDIR)" PROGNAME="$(PROGNAME)" CFLAGS="$(PROFILE_CFLAGS)" PROFILE="true"
	mv $(SRCDIR)/$(PROGNAME) $(BINDIR)/$(PROGNAME).debug_profile

$(BINDIR):
	mkdir $(BINDIR)

strip: release
	strip -sv $(BINDIR)/*

clean:
	$(MAKE) -C $(SRCDIR) OBJDIR="$(OBJDIR)" PROGNAME="$(PROGNAME)" $@
	rm -rf $(BINDIR) *.log
