# Makefile for El 7arress RCWS Operator Manual
# ============================================================================

# Variables
MAIN = operator_manual
LATEX = pdflatex
BIBTEX = bibtex
MAKEINDEX = makeindex

# Targets
.PHONY: all clean cleanall view

all: $(MAIN).pdf

$(MAIN).pdf: $(MAIN).tex
	@echo "Compiling LaTeX document (pass 1)..."
	$(LATEX) -interaction=nonstopmode $(MAIN).tex
	@echo "Compiling LaTeX document (pass 2 - for TOC)..."
	$(LATEX) -interaction=nonstopmode $(MAIN).tex
	@echo "Compiling LaTeX document (pass 3 - for cross-references)..."
	$(LATEX) -interaction=nonstopmode $(MAIN).tex
	@echo "PDF generated: $(MAIN).pdf"

view: $(MAIN).pdf
	@echo "Opening PDF..."
	@if command -v xdg-open > /dev/null; then \
		xdg-open $(MAIN).pdf; \
	elif command -v open > /dev/null; then \
		open $(MAIN).pdf; \
	elif command -v evince > /dev/null; then \
		evince $(MAIN).pdf & \
	else \
		echo "No PDF viewer found. Please open $(MAIN).pdf manually."; \
	fi

clean:
	@echo "Cleaning auxiliary files..."
	@rm -f $(MAIN).aux $(MAIN).log $(MAIN).out $(MAIN).toc $(MAIN).lof $(MAIN).lot
	@rm -f $(MAIN).bbl $(MAIN).blg $(MAIN).idx $(MAIN).ind $(MAIN).ilg
	@echo "Clean complete."

cleanall: clean
	@echo "Removing PDF..."
	@rm -f $(MAIN).pdf
	@echo "Clean all complete."

help:
	@echo "Available targets:"
	@echo "  make          - Compile the PDF (default)"
	@echo "  make all      - Compile the PDF"
	@echo "  make view     - Compile and open the PDF"
	@echo "  make clean    - Remove auxiliary files"
	@echo "  make cleanall - Remove all generated files including PDF"
	@echo "  make help     - Show this help message"
