# El 7arress RCWS - Complete LaTeX Manual Guide

## Overview

This directory contains a **complete, professional LaTeX version** of the El 7arress RCWS Operator Manual, with all 10 lessons and 4 appendices automatically generated from the condensed markdown manual.

## Quick Start

### Compile the Complete Manual

```bash
cd documentation/latex
make
```

This generates `operator_manual.pdf` with all 10 chapters and 4 appendices (~90-95 pages).

### View the PDF

```bash
make view   # Compile and open PDF
```

### Clean Build Files

```bash
make clean      # Remove auxiliary files
make cleanall   # Remove everything including PDF
```

## Directory Structure

```
documentation/latex/
├── operator_manual.tex              # Main LaTeX file (includes all chapters)
├── operator_manual_original.tex      # Backup of original template
├── chapters/                        # Auto-generated chapter files
│   ├── chapter01.tex                # Lesson 1: Safety & System Overview
│   ├── chapter02.tex                # Lesson 2: Basic Operation
│   ├── chapter03.tex                # Lesson 3: Menu System & Settings
│   ├── chapter04.tex                # Lesson 4: Motion Modes & Surveillance
│   ├── chapter05.tex                # Lesson 5: Target Engagement Process
│   ├── chapter06.tex                # Lesson 6: Ballistics & Fire Control
│   ├── chapter07.tex                # Lesson 7: System Status & Monitoring
│   ├── chapter08.tex                # Lesson 8: Emergency Procedures
│   ├── chapter09.tex                # Lesson 9: Operator Maintenance
│   └── chapter10.tex                # Lesson 10: Practical Training
├── appendices/                      # Auto-generated appendix files
│   ├── appendixA.tex                # Quick Reference Cards
│   ├── appendixB.tex                # Technical Specifications
│   ├── appendixC.tex                # Acronyms & Glossary
│   └── appendixD.tex                # Maintenance Log Template
├── build/                           # Build directory (auto-created)
│   └── *.aux, *.log, etc.          # Build artifacts
├── Makefile                         # Build automation
├── build.sh                         # Advanced build script with progress
├── convert_md_to_latex.py           # Markdown to LaTeX converter tool
├── generate_complete_manual.py       # Auto-generate all chapters
├── update_main_file.py              # Update main file to include chapters
├── README.md                        # General documentation
├── COMPLETE_MANUAL_GUIDE.md         # This file
├── LOGO_INSTRUCTIONS.md             # Logo setup instructions
└── .gitignore                       # Exclude build artifacts
```

## Features

### ✅ Complete Content
- **10 Lessons**: All operator training content
- **4 Appendices**: Quick references, specs, glossary, templates
- **Front Matter**: Title page, warnings, TOC, quick reference
- **~90-95 pages**: Professional military technical manual

### ✅ Professional Styling
- Military technical manual format
- Two-sided layout (print-ready)
- Color-coded safety callouts (WARNING/CAUTION/NOTE)
- Professional headers and footers
- Hyperlinked table of contents
- Custom LaTeX environments

### ✅ Auto-Generated
- All chapters generated from markdown source
- Consistent formatting throughout
- Easy to regenerate if source changes
- Modular structure for easy editing

## Build Methods

### Method 1: Makefile (Simple)

```bash
make          # Build PDF
make view     # Build and open PDF
make clean    # Clean aux files
```

### Method 2: Build Script (Advanced)

```bash
./build.sh            # Build with progress indicators
./build.sh view       # Build and open
./build.sh clean      # Clean build files
./build.sh cleanall   # Remove PDF too
```

### Method 3: Manual Compilation

```bash
pdflatex operator_manual.tex
pdflatex operator_manual.tex   # Second pass for TOC
pdflatex operator_manual.tex   # Third pass for refs
```

## Regenerating Content

If the markdown source changes, regenerate all chapters:

```bash
# Regenerate all chapters from markdown
./generate_complete_manual.py

# Update main file to include new chapters
./update_main_file.py

# Rebuild PDF
make
```

## Customization

### Change Colors

Edit `operator_manual.tex` lines 47-51:

```latex
\definecolor{warningred}{RGB}{200,20,40}
\definecolor{cautionorange}{RGB}{255,140,0}
\definecolor{notegray}{RGB}{100,100,100}
\definecolor{militarygreen}{RGB}{78,108,80}
```

### Modify Page Layout

Edit `operator_manual.tex` lines 34-42:

```latex
\geometry{
    letterpaper,
    left=1.25in,
    right=1.0in,
    top=1.0in,
    bottom=1.0in
}
```

### Change Headers/Footers

Edit `operator_manual.tex` lines 56-70:

```latex
\fancyhead[LE]{\small\textit{EL 7ARRESS RCWS OPERATOR MANUAL}}
\fancyhead[RO]{\small\textit{\leftmark}}
\fancyfoot[C]{\small CONDENSED VERSION 1.0}
```

### Edit Individual Chapters

Chapters are modular! Edit any file in `chapters/` or `appendices/`:

```bash
# Edit a chapter
nano chapters/chapter01.tex

# Rebuild PDF
make
```

## Tools and Scripts

### 1. `generate_complete_manual.py`

Automatically generates all chapter and appendix `.tex` files from the markdown source.

**Usage:**
```bash
./generate_complete_manual.py
```

**What it does:**
- Reads `../EL_HARRESS_OPERATOR_MANUAL_CONDENSED.md`
- Extracts all 10 lessons
- Extracts all 4 appendices
- Converts markdown to LaTeX
- Generates individual `.tex` files in `chapters/` and `appendices/`

**Features:**
- Converts headers, lists, formatting
- Escapes special LaTeX characters
- Handles WARNING/CAUTION/NOTE boxes
- Converts buttons and OSD indicators

### 2. `update_main_file.py`

Updates the main `operator_manual.tex` to include all generated chapters.

**Usage:**
```bash
./update_main_file.py
```

**What it does:**
- Reads `operator_manual_original.tex` (template)
- Finds `\mainmatter` location
- Adds `\input{}` commands for all chapters
- Adds `\appendix` section with all appendices
- Writes updated `operator_manual.tex`

### 3. `convert_md_to_latex.py`

General-purpose markdown to LaTeX converter tool.

**Usage:**
```bash
./convert_md_to_latex.py input.md output.tex
```

**Use case:** Convert custom markdown files to LaTeX

### 4. `build.sh`

Advanced build script with colored output and progress indicators.

**Usage:**
```bash
./build.sh           # Build PDF
./build.sh view      # Build and open
./build.sh clean     # Clean aux files
./build.sh cleanall  # Remove all generated files
./build.sh help      # Show help
```

**Features:**
- Dependency checking
- Three-pass compilation with progress
- Build directory management
- PDF page count and size reporting
- Auto-open PDF after build

## Logo Setup

The title page requires a logo file. **Choose one option:**

### Option 1: Comment Out Logo (No Logo)

Edit `operator_manual.tex` line ~208:

```latex
% \includegraphics[width=3in]{logo.png}\\[1cm]
```

### Option 2: Provide Your Logo

```bash
cp /path/to/your/logo.png documentation/latex/logo.png
```

Recommended: PNG with transparency, 300 DPI, 3-4 inches wide

### Option 3: Generate Placeholder

```bash
cd documentation/latex
convert -size 1200x400 xc:white \
    -pointsize 72 -fill '#4E6C50' -gravity center \
    -annotate +0+0 'EL 7ARRESS\nRCWS' \
    logo.png
```

See `LOGO_INSTRUCTIONS.md` for more details.

## Troubleshooting

### "File not found" Errors

**Problem:** LaTeX can't find chapter files

**Solution:**
```bash
# Regenerate chapters
./generate_complete_manual.py

# Verify files exist
ls chapters/
ls appendices/
```

### "Undefined references"

**Problem:** Cross-references not resolved

**Solution:** Compile 3 times:
```bash
make clean
make
```

### "Package not found"

**Problem:** Missing LaTeX packages

**Solution:** Install full TeX Live:
```bash
# Ubuntu/Debian
sudo apt-get install texlive-full

# macOS
brew install --cask mactex

# Fedora
sudo dnf install texlive-scheme-full
```

### Tables Not Formatting Correctly

**Problem:** Auto-generated tables need manual formatting

**Solution:** Edit chapter files and convert markdown tables to LaTeX:

```latex
\begin{table}[H]
\centering
\caption{Table Title}
\begin{tabular}{lll}
\toprule
Header 1 & Header 2 & Header 3 \\
\midrule
Data 1   & Data 2   & Data 3   \\
\bottomrule
\end{tabular}
\end{table}
```

### Build Fails

**Check the log:**
```bash
cat build/operator_manual.log | tail -50
```

**Common issues:**
- Missing `\end{itemize}` or `\end{enumerate}`
- Unescaped special characters
- Missing package
- Syntax error in chapter file

## Advanced Usage

### Build with Custom Output Directory

```bash
pdflatex -output-directory=custom_dir operator_manual.tex
```

### Generate Draft Version (Faster)

```bash
pdflatex -draftmode operator_manual.tex
```

### Enable Shell Escape (For Advanced Graphics)

```bash
pdflatex -shell-escape operator_manual.tex
```

### Create Separate PDFs per Chapter

```bash
# Edit operator_manual.tex and comment out unwanted chapters
# Or use \includeonly{}:

% In preamble:
\includeonly{chapters/chapter01,chapters/chapter02}
```

## Quality Checks

After building, verify:

- [ ] Table of contents complete and accurate
- [ ] All chapters numbered correctly (1-10)
- [ ] All appendices lettered correctly (A-D)
- [ ] Page numbers sequential
- [ ] Headers show correct chapter names
- [ ] No "Undefined reference" warnings in log
- [ ] All tables formatted properly
- [ ] WARNING/CAUTION/NOTE boxes render correctly
- [ ] Hyperlinks work (click TOC entries)
- [ ] PDF bookmarks present (left sidebar in PDF viewer)

## Workflow for Updates

### If Markdown Manual Changes:

```bash
# 1. Regenerate all chapters
./generate_complete_manual.py

# 2. Update main file
./update_main_file.py

# 3. Review changes
git diff chapters/

# 4. Manually fix any tables or formatting issues
nano chapters/chapter02.tex

# 5. Rebuild PDF
make clean
make

# 6. Review PDF
make view

# 7. Commit changes
git add .
git commit -m "Update LaTeX manual from markdown source"
```

### For Manual Edits:

```bash
# 1. Edit chapter file directly
nano chapters/chapter05.tex

# 2. Rebuild
make

# 3. Review
make view

# 4. Commit
git add chapters/chapter05.tex
git commit -m "Fix: Correct table formatting in Chapter 5"
```

## Production Checklist

Before finalizing for distribution:

- [ ] Logo added or logo line commented out
- [ ] Document metadata correct (version, date, etc.)
- [ ] All chapters reviewed and formatted
- [ ] All tables converted from markdown to proper LaTeX
- [ ] All WARNING/CAUTION/NOTE boxes reviewed
- [ ] Page count within target (~90-95 pages)
- [ ] Print test on physical printer (two-sided)
- [ ] PDF file size reasonable (<10 MB)
- [ ] Hyperlinks functional
- [ ] No compilation warnings

## Output Specifications

**Expected Results:**
- **File:** `operator_manual.pdf`
- **Pages:** ~90-95 pages
- **File Size:** ~2-5 MB (depends on graphics)
- **Print-Ready:** Yes (two-sided, US Letter)
- **Hyperlinked:** Yes (TOC, cross-references)
- **Searchable:** Yes (embedded text)

## Contributing

### Adding a New Chapter

```bash
# 1. Create chapter file
nano chapters/chapter11.tex

# 2. Add to operator_manual.tex:
\input{chapters/chapter11}
\clearpage

# 3. Rebuild
make
```

### Adding Custom Environments

Edit `operator_manual.tex` preamble to add new environments similar to WARNING/CAUTION/NOTE.

### Changing Fonts

```latex
% Add to preamble:
\usepackage{palatino}  % Or any font package
```

## Support

For LaTeX issues:
- [Overleaf Documentation](https://www.overleaf.com/learn)
- [LaTeX Wikibook](https://en.wikibooks.org/wiki/LaTeX)
- [TeX Stack Exchange](https://tex.stackexchange.com/)

For package documentation:
```bash
texdoc <package-name>
```

Example:
```bash
texdoc booktabs
texdoc mdframed
texdoc hyperref
```

## License

This LaTeX template and automation scripts are provided for the El 7arress project.

---

**Version:** 1.0
**Last Updated:** 2025-01-16
**Status:** Complete - All 10 chapters + 4 appendices auto-generated
