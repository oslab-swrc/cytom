PLOT := $(patsubst %.gp,%.pdf,$(wildcard *.gp))
DEPS := $(PLOT)
SHELL:= $(shell echo $$SHELL)

.DEFAULT_GOAL := all

all: $(DEPS) ## generate plots

-include $(PLOT:.pdf=.d)

%.pdf: %.gp common.gnuplot ## generate plot
	OUT=$@ TARGET=pdf gnuplot $<
	./gnuplot_dependencies.py $< > $*.d

distclean:
	rm -f *.pdf *.d
