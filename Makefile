#pin stuff
include Makefile.pin.gnu.config
OPT=-O2
CXXFLAGS = -fomit-frame-pointer -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT) -MMD
TOOL_ROOTS = streamcount
TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))

all: tools runpin pinvis

tools: $(OBJDIR) $(TOOLS)

runpin:
	echo "$(PIN_HOME)/pin -injection child -t $(OBJDIR)/streamcount.so -- /bin/ls" >> runpin
	chmod u+x runpin

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(PIN_LIBNAMES)

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o
	$(PIN_LD) $(PIN_LDFLAGS) ${LINK_OUT}$@ $< $(PIN_LIBS) $(DBG)

#vis stuff
CFLAGS = -g -c -O2 -DLINUX
INCLUDE = -I. -I/usr/include/ -I/usr/include/X11/ -I/usr/local/include/GL
INCOSG = -I/home/brian/code/OpenSceneGraph-3.0.1/include
LDLIBS = -lm -ldl -lGL -lGLU -lpthread -lXext -lX11
LDFLAGS =  -L. -L/usr/lib -L/usr/X11R6/lib -L/usr/local/lib
LDOSG = -L/home/brian/code/OpenSceneGraph-3.0.1/lib -losg -losgViewer -losgSim -lOpenThreads -losgGA
CC = g++

pinvis: pinvis.o
	cc -o pinvis pinvis.o $(INCLUDE) $(INCOSG) $(LDFLAGS) $(LDLIBS) $(LDOSG)

pinvis.o: pinvis.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) $(INCOSG) -o $@ $<

clean:
	-rm -rf $(OBJDIR) runpin *.o pinvis
