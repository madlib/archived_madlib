all: madlib_cmdspec.pdf

.PHONY: clean

clean:
	rm -f *.ps *.pdf *.dvi *~ *.toc *.aux *.log *.out

%.pdf: %.tex
	pdflatex $<
	pdflatex $<
	pdflatex $<
