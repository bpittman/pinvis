#pin stuff
include Makefile.pin.gnu.config
OPT=-O2
CXXFLAGS = -fomit-frame-pointer -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT) -MMD
TOOL_ROOTS = streamcount
TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))
GTEST_DIR=/home/brian/code/gtest-1.6.0
GTEST_INCLUDE=-I${GTEST_DIR} -I${GTEST_DIR}/include

all: tools runpin pinvis test

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
LDOSG = -L/home/brian/code/OpenSceneGraph-3.0.1/lib -losg -losgViewer -losgSim -lOpenThreads -losgGA -losgText
CC = g++

pinvis: pinvis.o
	cc -o pinvis pinvis.o $(INCLUDE) $(INCOSG) $(LDFLAGS) $(LDLIBS) $(LDOSG)

pinvis.o: pinvis.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) $(INCOSG) -o $@ $<

test: test_pinvis.cpp libgtest.a
	${CC} ${GTEST_INCLUDE} test_pinvis.cpp libgtest.a -o test_pinvis
	./test_pinvis

libgtest.a: gtest-all.o
	ar -rv libgtest.a gtest-all.o

gtest-all.o: ${GTEST_DIR}/src/gtest-all.cc
	${CC} ${GTEST_INCLUDE} -DGTEST_HAS_PTHREAD=0 -c ${GTEST_DIR}/src/gtest-all.cc

clean:
	-rm -rf $(OBJDIR) runpin *.o *.a pinvis test_pinvis
