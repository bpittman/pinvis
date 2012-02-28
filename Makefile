##
## PIN tools
##
include Makefile.pin.gnu.config

OPT=-O2
CXXFLAGS = -fomit-frame-pointer -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT) -MMD

TOOL_ROOTS = streamcount

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))

all: tools runpin
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

## cleaning
clean:
	-rm -rf $(OBJDIR) runpin *.out *.log *.tested *.failed
