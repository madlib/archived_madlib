all: madlib_pkgspec.pdf madlib_install_tools.pdf

.PHONY: clean

clean:
	rm -f *.ps *.pdf *.dvi *~ *.toc *.aux *.log *.out

%.pdf: %.tex
	pdflatex $<
	pdflatex $<
	pdflatex $<
