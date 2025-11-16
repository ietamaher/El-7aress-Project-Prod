#!/usr/bin/env python3
"""
Markdown to LaTeX Converter for El 7arress Operator Manual
Converts the condensed markdown manual to LaTeX format
"""

import re
import sys
from pathlib import Path

class MarkdownToLatexConverter:
    def __init__(self):
        self.in_table = False
        self.in_code_block = False

    def convert_file(self, input_file, output_file):
        """Convert markdown file to LaTeX"""
        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()

        latex_content = self.convert_content(content)

        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(latex_content)

        print(f"Converted {input_file} -> {output_file}")

    def convert_content(self, md_content):
        """Convert markdown content to LaTeX"""
        lines = md_content.split('\n')
        latex_lines = []

        for line in lines:
            converted = self.convert_line(line)
            if converted is not None:
                latex_lines.append(converted)

        return '\n'.join(latex_lines)

    def convert_line(self, line):
        """Convert a single markdown line to LaTeX"""

        # Skip certain markdown patterns
        if line.startswith('---') and len(line.strip()) == 3:
            return None  # Horizontal rules - skip

        # Code blocks
        if line.strip().startswith('```'):
            self.in_code_block = not self.in_code_block
            if self.in_code_block:
                return '\\begin{verbatim}'
            else:
                return '\\end{verbatim}'

        if self.in_code_block:
            return line

        # Headers
        if line.startswith('# '):
            title = line[2:].strip()
            return f'\\chapter{{{self.escape_latex(title)}}}'
        elif line.startswith('## '):
            title = line[3:].strip()
            return f'\\section{{{self.escape_latex(title)}}}'
        elif line.startswith('### '):
            title = line[4:].strip()
            return f'\\subsection{{{self.escape_latex(title)}}}'
        elif line.startswith('#### '):
            title = line[5:].strip()
            return f'\\subsubsection{{{self.escape_latex(title)}}}'

        # WARNING boxes
        if line.strip().startswith('**⚠️ WARNING'):
            return '\\WARNING{'
        elif line.strip().startswith('**⚠️ CAUTION'):
            return '\\CAUTION{'
        elif line.strip().startswith('**⚠️ NOTE'):
            return '\\NOTE{'

        # Bold and italic
        line = self.convert_inline_formatting(line)

        # Lists
        if re.match(r'^\s*[-*]\s+', line):
            indent = len(re.match(r'^\s*', line).group(0))
            content = re.sub(r'^\s*[-*]\s+', '', line)
            return f'\\item {self.escape_latex(content)}'

        # Numbered lists
        if re.match(r'^\s*\d+\.\s+', line):
            content = re.sub(r'^\s*\d+\.\s+', '', line)
            return f'\\item {self.escape_latex(content)}'

        # Tables (simplified - would need more sophisticated parsing)
        if '|' in line and line.strip().startswith('|'):
            # This is a table row - would need full table parsing
            return f'% TABLE: {line}'

        # Regular text
        return self.escape_latex(line)

    def convert_inline_formatting(self, text):
        """Convert inline markdown formatting"""
        # Bold
        text = re.sub(r'\*\*([^*]+)\*\*', r'\\textbf{\1}', text)

        # Italic
        text = re.sub(r'\*([^*]+)\*', r'\\textit{\1}', text)

        # Code
        text = re.sub(r'`([^`]+)`', r'\\texttt{\1}', text)

        # Buttons
        text = re.sub(r'Button (\d+)', r'\\button{Button \1}', text)

        # OSD indicators in quotes
        text = re.sub(r'"([A-Z\s]+)"', r'\\osd{\1}', text)

        return text

    def escape_latex(self, text):
        """Escape special LaTeX characters"""
        replacements = {
            '\\': r'\\textbackslash{}',
            '{': r'\{',
            '}': r'\}',
            '$': r'\$',
            '&': r'\&',
            '%': r'\%',
            '#': r'\#',
            '_': r'\_',
            '~': r'\textasciitilde{}',
            '^': r'\^{}',
        }

        for char, replacement in replacements.items():
            text = text.replace(char, replacement)

        # Degrees symbol
        text = text.replace('°', r'\degree{}')

        # Checkboxes
        text = text.replace('☐', r'$\square$')
        text = text.replace('✓', r'$\checkmark$')
        text = text.replace('✅', r'$\checkmark$')
        text = text.replace('❌', r'$\\times$')

        # Arrows
        text = text.replace('→', r'$\\rightarrow$')
        text = text.replace('←', r'$\\leftarrow$')

        return text

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 convert_md_to_latex.py <input.md> [output.tex]")
        print("Example: python3 convert_md_to_latex.py ../EL_HARRESS_OPERATOR_MANUAL_CONDENSED.md chapters_output.tex")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'converted_output.tex'

    converter = MarkdownToLatexConverter()
    converter.convert_file(input_file, output_file)

    print("\n" + "="*60)
    print("Conversion complete!")
    print("="*60)
    print("\nNOTE: This is a basic conversion. You may need to:")
    print("  - Manually convert tables to LaTeX table format")
    print("  - Adjust WARNING/CAUTION/NOTE box closings (add })")
    print("  - Wrap lists in \\begin{itemize} ... \\end{itemize}")
    print("  - Fine-tune formatting and spacing")
    print("  - Add \\clearpage commands for page breaks")
    print("\nThe converted file is meant as a starting point for manual editing.")

if __name__ == '__main__':
    main()
