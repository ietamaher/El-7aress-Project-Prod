# Logo Instructions

The LaTeX document requires a logo file for the title page.

## Option 1: Provide Your Own Logo

Place your logo file in this directory as `logo.png`:

```bash
cp /path/to/your/logo.png documentation/latex/logo.png
```

**Recommended specifications:**
- Format: PNG (with transparency) or PDF
- Resolution: 300 DPI minimum
- Size: 3-4 inches wide at 300 DPI (900-1200 pixels)
- Aspect ratio: Any (will be scaled to 3 inches width)

## Option 2: Generate Placeholder Logo

If you have ImageMagick installed, run this command to generate a simple placeholder:

```bash
cd documentation/latex
convert -size 1200x400 xc:white \
    -pointsize 72 -font Arial-Bold \
    -fill '#4E6C50' -gravity center \
    -annotate +0+0 'EL 7ARRESS\nRCWS' \
    logo.png
```

## Option 3: Comment Out Logo

If you don't need a logo, edit `operator_manual.tex` and comment out the logo line:

```latex
\title{%
    \vspace{-2cm}
    % \includegraphics[width=3in]{logo.png}\\[1cm]  % <-- Comment this line
    {\Huge\bfseries EL 7ARRESS}\\[0.5cm]
    ...
}
```

## Option 4: Use SVG Logo

If you have an SVG logo, you can:

1. Convert to PDF:
```bash
inkscape logo.svg --export-filename=logo.pdf
```

2. Update the LaTeX file to use PDF:
```latex
\includegraphics[width=3in]{logo.pdf}
```

## Troubleshooting

### "File 'logo.png' not found" error

**Solution:** Either provide the logo file or comment out the `\includegraphics` line as shown in Option 3.

### Logo appears too large/small

**Solution:** Adjust the width parameter:
```latex
\includegraphics[width=2in]{logo.png}  % Smaller
\includegraphics[width=4in]{logo.png}  % Larger
```

### Logo has wrong aspect ratio

**Solution:** Specify both width and height:
```latex
\includegraphics[width=3in,height=1.5in,keepaspectratio]{logo.png}
```

The `keepaspectratio` option prevents distortion.
