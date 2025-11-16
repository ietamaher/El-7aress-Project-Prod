# El 7arress RCWS Operator Manual - LaTeX Version

This directory contains the LaTeX source files for the El 7arress Remote Controlled Weapon Station Operator Manual (Condensed Version).

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Document Structure](#document-structure)
- [Compilation](#compilation)
- [Customization](#customization)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software

You need a LaTeX distribution installed on your system:

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install texlive-full
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install texlive-scheme-full
```

**macOS:**
```bash
brew install --cask mactex
```

**Windows:**
- Download and install [MiKTeX](https://miktex.org/download) or [TeX Live](https://www.tug.org/texlive/)

### Required LaTeX Packages

The document uses the following packages (included in texlive-full):
- `geometry` - Page layout
- `fancyhdr` - Headers and footers
- `xcolor` - Color support
- `longtable`, `booktabs`, `array` - Advanced tables
- `titlesec` - Section formatting
- `mdframed` - Colored boxes (WARNING, CAUTION, NOTE)
- `tikz` - Diagrams (optional)
- `hyperref` - PDF hyperlinks and bookmarks

## Quick Start

### Compile the Document

```bash
cd documentation/latex
make
```

This will generate `operator_manual.pdf`.

### View the PDF

```bash
make view
```

### Clean Build Files

```bash
make clean      # Remove auxiliary files only
make cleanall   # Remove all generated files including PDF
```

## Document Structure

### Main File

**`operator_manual.tex`** - Main LaTeX document containing:
- Preamble (packages, settings, custom commands)
- Front matter (title page, warnings, TOC)
- Main content (chapters 1-10)
- Back matter (appendices)

### Document Classes and Styling

The document uses the `book` class with military technical manual styling:

- **Page Size:** US Letter (8.5" × 11")
- **Margins:** 1.25" left, 1.0" right/top/bottom
- **Font:** 11pt base size
- **Layout:** Two-sided with chapters opening on right pages

### Custom Environments

The document defines three special callout environments:

#### WARNING Box
```latex
\WARNING{%
This is critical safety information that could result in death or serious injury.
}
```

#### CAUTION Box
```latex
\CAUTION{%
This indicates potential equipment damage or degraded performance.
}
```

#### NOTE Box
```latex
\NOTE{%
This provides additional helpful information or clarification.
}
```

#### Procedure Box
```latex
\begin{procedure}{Procedure Name}
\item Step 1
\item Step 2
\item Step 3
\end{procedure}
```

### Custom Commands

- `\button{X}` - Formats button notation: **[X]**
- `\osd{TEXT}` - Formats OSD indicators: **TEXT**
- `\degree` - Degrees symbol: °

### Color Scheme

Defined colors (military-appropriate):
- `warningred` - RGB(200,20,40) - Critical warnings
- `cautionorange` - RGB(255,140,0) - Cautions
- `notegray` - RGB(100,100,100) - Notes
- `militarygreen` - RGB(78,108,80) - Headers and accents
- `tablegray` - RGB(240,240,240) - Table row shading

## Compilation

### Manual Compilation

If you prefer to compile manually without `make`:

```bash
pdflatex operator_manual.tex
pdflatex operator_manual.tex  # Second pass for TOC
pdflatex operator_manual.tex  # Third pass for cross-references
```

### Why Three Passes?

LaTeX requires multiple passes to:
1. **First pass:** Generate auxiliary files (.aux, .toc)
2. **Second pass:** Populate table of contents and labels
3. **Third pass:** Resolve all cross-references and page numbers

### Output Files

After compilation, you'll see:
- `operator_manual.pdf` - **Final document** (distribute this)
- `operator_manual.aux` - Auxiliary file (can be deleted)
- `operator_manual.log` - Compilation log (useful for debugging)
- `operator_manual.out` - Hyperlink data (can be deleted)
- `operator_manual.toc` - Table of contents data (can be deleted)

## Customization

### Change Document Metadata

Edit the preamble section:

```latex
\title{%
    {\Huge\bfseries YOUR TITLE HERE}\\[0.5cm]
    {\LARGE YOUR SUBTITLE}
}
```

### Modify Page Layout

Adjust margins in the `\geometry` command:

```latex
\geometry{
    letterpaper,
    left=1.5in,    % Change these values
    right=1.0in,
    top=1.0in,
    bottom=1.0in
}
```

### Change Header/Footer

Modify the `fancyhdr` settings:

```latex
\fancyhead[LE]{\small\textit{YOUR LEFT HEADER}}
\fancyhead[RO]{\small\textit{YOUR RIGHT HEADER}}
\fancyfoot[C]{\small YOUR FOOTER TEXT}
```

### Add Logo/Images

1. Place image file in `documentation/latex/` directory
2. Reference in document:

```latex
\includegraphics[width=3in]{logo.png}
```

**Note:** For the title page logo, create `logo.png` in the latex directory, or comment out the line if not needed.

### Modify Colors

Edit color definitions in preamble:

```latex
\definecolor{warningred}{RGB}{200,20,40}  % Change RGB values
```

## Troubleshooting

### Common Errors

#### "File not found" errors

**Problem:** LaTeX can't find a package or image.

**Solution:**
- Ensure texlive-full is installed
- Verify image files are in the correct directory
- Check file paths in `\includegraphics` commands

#### "Undefined control sequence"

**Problem:** Custom command used before definition.

**Solution:**
- Ensure all `\newcommand` definitions are in the preamble
- Check for typos in command names

#### "Missing \$ inserted"

**Problem:** Math mode error.

**Solution:**
- Ensure degree symbols use `\degree` command, not raw `°`
- Wrap math expressions in `$...$`

#### Table doesn't fit on page

**Problem:** Table width exceeds page margins.

**Solution:**
- Use `\small` or `\footnotesize` before table
- Adjust column widths with `p{width}` column specifier
- Consider rotating table with `\begin{sidewaystable}` (requires `rotating` package)

### PDF Output Issues

#### Hyperlinks not working

**Solution:** Ensure `hyperref` package is loaded and recompile 3 times.

#### Table of Contents blank

**Solution:** Compile at least twice to generate TOC.

#### Page numbers incorrect

**Solution:** Compile three times to resolve all references.

### Compilation Warnings

Most warnings can be safely ignored. Critical ones to address:

- **Overfull/Underfull hbox:** Text doesn't fit well on line (cosmetic, usually acceptable)
- **Undefined references:** Need additional compilation pass
- **Missing fonts:** Install missing font packages

## Advanced Features

### Adding a Bibliography

1. Create `references.bib` file
2. Add to document:

```latex
\bibliographystyle{plain}
\bibliography{references}
```

3. Compile with:
```bash
pdflatex operator_manual
bibtex operator_manual
pdflatex operator_manual
pdflatex operator_manual
```

### Creating an Index

1. Enable index in preamble:
```latex
\usepackage{makeidx}
\makeindex
```

2. Mark index entries:
```latex
\index{Term to index}
```

3. Print index:
```latex
\printindex
```

4. Compile with:
```bash
pdflatex operator_manual
makeindex operator_manual
pdflatex operator_manual
```

### Splitting into Multiple Files

For easier management, split into separate files:

```latex
% In operator_manual.tex:
\include{chapters/chapter01}
\include{chapters/chapter02}
```

Create subdirectory `chapters/` with individual chapter files.

## Version Control

### Files to Track in Git

```
documentation/latex/
├── operator_manual.tex    # Track
├── Makefile              # Track
├── README.md            # Track
├── logo.png             # Track (if used)
└── chapters/            # Track (if using split files)
```

### Files to Ignore (.gitignore)

```
*.aux
*.log
*.out
*.toc
*.lof
*.lot
*.bbl
*.blg
*.idx
*.ind
*.ilg
*.pdf  # Optional: may want to track final PDF
```

## Support

### LaTeX Resources

- [Overleaf Documentation](https://www.overleaf.com/learn) - Excellent LaTeX tutorials
- [LaTeX Wikibook](https://en.wikibooks.org/wiki/LaTeX) - Comprehensive guide
- [TeX Stack Exchange](https://tex.stackexchange.com/) - Q&A community

### Package Documentation

View documentation for any package:
```bash
texdoc package-name
```

Example:
```bash
texdoc mdframed
texdoc booktabs
texdoc tikz
```

## License

This document structure and styling is provided for the El 7arress project. Modify as needed for your requirements.

---

**Last Updated:** 2025-01-16
**Document Version:** 1.0
**LaTeX Template Version:** 1.0
