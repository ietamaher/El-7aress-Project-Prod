# Combined LaTeX Template Guide

## Overview

The `operator_manual_combined.tex` template is the **BEST OF BOTH WORLDS** - combining professional military formatting with operator-friendly features.

## What's Included

### From Professional Military Template ✅

1. **tcolorbox Environments** - Superior to mdframed
   - Breakable across pages
   - Enhanced styling options
   - Better rendering

2. **FontAwesome Icons**
   - `\warningsign` → ⚠ (triangle with exclamation)
   - `\cautionsign` → ⚠ (circle with exclamation)
   - `\notesign` → ℹ (info circle)

3. **Classification Markings**
   - **RESTRICTED** headers on left/right
   - Professional footer: `RCWS-OM-001-EN v1.0 | page | October 2025`

4. **Professional Document Structure**
   - Title page with coat of arms placeholder
   - Document Control page
   - Revision History table
   - Distribution List
   - Destruction Notice
   - Program of Instruction (POI)

5. **Military Color Scheme**
   - `milred` - RGB(153,0,0)
   - `milyellow` - RGB(204,153,0)
   - `milblue` - RGB(0,51,102)
   - `milgreen` - RGB(0,102,51)

6. **Better Typography**
   - `\setstretch{0.95}` - Compact line spacing
   - `inconsolata` - Professional monospace font
   - Compact list spacing: `itemsep=0pt,parsep=0pt,topsep=4pt`

### From Operator-Friendly Template ✅

1. **Quick Reference Card Section**
   - Emergency Actions table
   - Essential Joystick Controls
   - Pre-Operation Checklist
   - OSD Indicators reference

2. **Operator Commands**
   ```latex
   \button{Button 5}      % Displays: [Button 5]
   \osd{ARM}              % Displays: ARM (monospace bold)
   \degree                % Displays: ° symbol
   ```

3. **Procedure Environment**
   ```latex
   \begin{procedure}{Starting the System}
   \item Turn Main Power ON
   \item Wait 30 seconds for boot
   \item Enable Station
   \end{procedure}
   ```

4. **Command Shortcuts**
   ```latex
   \WARNING{This is dangerous!}
   \CAUTION{This could damage equipment!}
   \NOTE{This is helpful information.}
   ```

5. **Checklist Environment**
   ```latex
   \begin{checklist}
   \item Power ON
   \item Cameras operational
   \item Gimbal test complete
   \end{checklist}
   ```

6. **Specific Safety Warnings**
   - Lethal weapon system
   - Electrical hazards
   - Moving parts
   - Laser hazard
   - Equipment damage warnings
   - Environmental protection

## Usage Instructions

### Building the Manual

```bash
cd documentation/latex

# Method 1: Using Makefile (if pdflatex available)
make

# Method 2: Using build script
./build.sh

# Method 3: Manual compilation
pdflatex operator_manual_combined.tex
pdflatex operator_manual_combined.tex  # Run twice for TOC
pdflatex operator_manual_combined.tex  # Run third time for references
```

### Using the Operator Commands in Chapters

When writing chapter content, use these commands for consistency:

**Joystick Buttons:**
```latex
Press \button{Button 4} to start tracking.
```

**On-Screen Display Elements:**
```latex
Verify the \osd{ARM} indicator is visible.
```

**Degrees:**
```latex
Gimbal elevation range: -20\degree~to +60\degree
```

**Procedures:**
```latex
\begin{procedure}{Emergency Stop Procedure}
\item Strike E-STOP button
\item Turn Station Power OFF
\item Turn Main Power OFF
\item Report incident to command
\end{procedure}
```

**Warnings/Cautions/Notes:**
```latex
\WARNING{%
Never point the weapon at friendly forces!
}

\CAUTION{%
Operating beyond gimbal limits may damage servos.
}

\NOTE{%
The system automatically saves settings every 5 minutes.
}
```

**Checklists:**
```latex
\section{Pre-Flight Checklist}

\begin{checklist}
\item Main Power ON
\item Station Enable ON
\item Cameras operational
\item Gimbal motion test
\item LRF test
\end{checklist}
```

## File Structure

```
documentation/latex/
├── operator_manual_combined.tex    # Main template file (USE THIS ONE!)
├── chapters/
│   ├── chapter01.tex               # Auto-generated chapters
│   ├── chapter02.tex
│   └── ...
├── appendices/
│   ├── appendixA.tex               # Auto-generated appendices
│   ├── appendixB.tex
│   └── ...
├── build/                          # Build artifacts (auto-generated)
└── operator_manual_combined.pdf    # Final output
```

## Key Differences from Original Templates

### vs. Original Professional Template
- ✅ Added Quick Reference Card section
- ✅ Added operator commands (`\button`, `\osd`, `\degree`)
- ✅ Added procedure environment
- ✅ Added command shortcuts for boxes
- ✅ Added more comprehensive safety warnings
- ✅ Added OSD indicators reference

### vs. Original Operator-Friendly Template
- ✅ Uses tcolorbox instead of mdframed (better quality)
- ✅ Uses FontAwesome icons instead of Unicode (more professional)
- ✅ Adds classification markings
- ✅ Adds Document Control page
- ✅ Adds POI section
- ✅ Better color scheme (military colors)
- ✅ Better typography

## Customization

### Adding Logo/Coat of Arms

Uncomment line 316 in the template and add your image:
```latex
\includegraphics[width=3in]{tunisia_coat_of_arms.png}
```

### Changing Classification

Change all instances of "RESTRICTED" to your classification level:
- UNCLASSIFIED
- CONFIDENTIAL
- SECRET
- TOP SECRET

### Modifying Colors

Edit the color definitions in lines 66-72:
```latex
\definecolor{milred}{RGB}{153,0,0}      % Change RGB values
\definecolor{milyellow}{RGB}{204,153,0}
\definecolor{milblue}{RGB}{0,51,102}
\definecolor{milgreen}{RGB}{0,102,51}
```

### Updating Document Metadata

Edit lines 367-382 for document control information:
- Document Number
- Version
- Date
- Classification
- Author
- Approver

## Advanced Features

### Table Column Types

Three custom column types for better table formatting:
```latex
\begin{tabular}{L{4cm}C{3cm}R{2cm}}
Left aligned & Centered & Right aligned \\
\end{tabular}
```

### Listings for Code

Box-drawing characters are properly supported:
```latex
\begin{lstlisting}
├── System Status
│   ├── Device Status
│   └── Fault Codes
\end{lstlisting}
```

### Cross-References

Use labels for cross-referencing:
```latex
See Chapter \ref{ch:lesson5} for target engagement.
See Appendix \ref{app:a} for quick reference cards.
```

## Troubleshooting

### Missing Packages

If you get package errors, install:
```bash
# Ubuntu/Debian
sudo apt-get install texlive-full

# Or install specific packages
sudo apt-get install texlive-fonts-extra  # For fontawesome5
```

### Build Errors

1. **Run 3 times** - LaTeX needs multiple passes for TOC and references
2. **Check .log file** - Look in `build/operator_manual_combined.log`
3. **Clean build** - `make clean` or `./build.sh clean`

### Missing Chapters

Ensure all chapter files exist:
```bash
ls chapters/chapter*.tex
ls appendices/appendix*.tex
```

## Benefits of Combined Template

1. ✅ **Professional appearance** - Military-grade document formatting
2. ✅ **Operator-friendly** - Easy-to-use commands and quick references
3. ✅ **Comprehensive** - All necessary front/back matter included
4. ✅ **Flexible** - Easy to customize colors, classification, metadata
5. ✅ **Well-documented** - Clear examples and usage instructions
6. ✅ **Production-ready** - Suitable for official military documentation

## Recommendation

**USE THIS TEMPLATE** for all future operator manuals. It combines the best features of both templates and provides a professional, comprehensive foundation for military technical documentation.
