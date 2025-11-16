#!/usr/bin/env python3
"""
Complete LaTeX Manual Generator for El 7arress RCWS
Generates all chapters and appendices from the condensed markdown manual
"""

import re
from pathlib import Path

def read_markdown_file():
    """Read the condensed markdown manual"""
    md_file = Path(__file__).parent.parent / "EL_HARRESS_OPERATOR_MANUAL_CONDENSED.md"
    with open(md_file, 'r', encoding='utf-8') as f:
        return f.read()

def escape_latex(text):
    """Escape special LaTeX characters"""
    if not text:
        return ''

    # First, protect existing LaTeX commands
    text = text.replace('\\', '\\textbackslash{}')

    # Escape special characters
    replacements = {
        '{': r'\{',
        '}': r'\}',
        '$': r'\$',
        '&': r'\&',
        '%': r'\%',
        '#': r'\#',
        '_': r'\_',
        '~': r'\textasciitilde{}',
        '^': r'\textasciicircum{}',
    }

    for char, replacement in replacements.items():
        text = text.replace(char, replacement)

    # Convert special symbols
    text = text.replace('°', r'\degree{}')
    text = text.replace('☐', r'$\square$')
    text = text.replace('✓', r'$\checkmark$')
    text = text.replace('✅', r'$\checkmark$')
    text = text.replace('❌', r'$\times$')
    text = text.replace('⚠️', '')
    text = text.replace('→', r'$\rightarrow$')
    text = text.replace('←', r'$\leftarrow$')
    text = text.replace('↓', r'$\downarrow$')
    text = text.replace('↑', r'$\uparrow$')
    text = text.replace('⟷', r'$\leftrightarrow$')

    return text

def convert_inline_formatting(text):
    """Convert markdown inline formatting to LaTeX"""
    # Bold
    text = re.sub(r'\*\*(.+?)\*\*', r'\\textbf{\1}', text)

    # Italic (but not if already in bold)
    text = re.sub(r'(?<!\\textbf{)\*(.+?)\*', r'\\textit{\1}', text)

    # Code/monospace
    text = re.sub(r'`(.+?)`', r'\\texttt{\1}', text)

    # Button references
    text = re.sub(r'Button (\d+)', r'\\button{Button \1}', text)

    # OSD indicators (text in quotes that's all caps)
    text = re.sub(r'"([A-Z][A-Z\s]+)"', r'\\osd{\1}', text)

    return text

def extract_chapters(content):
    """Extract chapters from markdown content"""
    chapters = {}

    # Split by main headers (# LESSON X:)
    lesson_pattern = re.compile(r'# LESSON (\d+): ([^\n]+)')
    appendix_pattern = re.compile(r'# APPENDIX ([A-D]): ([^\n]+)')

    lessons = list(lesson_pattern.finditer(content))
    appendices = list(appendix_pattern.finditer(content))

    # Extract lesson content
    for i, match in enumerate(lessons):
        lesson_num = int(match.group(1))
        lesson_title = match.group(2).strip()

        start = match.start()
        end = lessons[i+1].start() if i+1 < len(lessons) else (appendices[0].start() if appendices else len(content))

        lesson_content = content[start:end]
        chapters[f'lesson{lesson_num}'] = {
            'number': lesson_num,
            'title': lesson_title,
            'content': lesson_content
        }

    # Extract appendix content
    for i, match in enumerate(appendices):
        app_letter = match.group(1)
        app_title = match.group(2).strip()

        start = match.start()
        end = appendices[i+1].start() if i+1 < len(appendices) else len(content)

        app_content = content[start:end]
        chapters[f'appendix{app_letter}'] = {
            'letter': app_letter,
            'title': app_title,
            'content': app_content
        }

    return chapters

def convert_markdown_to_latex(md_text):
    """Convert markdown section to LaTeX"""
    lines = md_text.split('\n')
    latex_lines = []
    in_code_block = False
    in_list = False
    in_table = False
    list_type = None
    skip_next_empty_lines = False
    skip_learning_objectives = False
    objectives_line_count = 0

    for line in lines:
        # Handle code blocks
        if line.strip().startswith('```'):
            if in_code_block:
                latex_lines.append('\\end{verbatim}')
                in_code_block = False
            else:
                latex_lines.append('\\begin{verbatim}')
                in_code_block = True
            continue

        if in_code_block:
            latex_lines.append(line)
            continue

        # Skip horizontal rules
        if line.strip() == '---':
            latex_lines.append('')
            continue

        # Skip main lesson/appendix headers (these are chapter titles in main file)
        if line.startswith('# LESSON') or line.startswith('# APPENDIX'):
            skip_next_empty_lines = True
            continue

        # Skip "Lesson Duration:" lines that follow lesson headers
        if 'Lesson Duration:' in line and '**' in line:
            continue

        # Skip "Learning Objectives:" section entirely (it's added in header)
        if line.strip() == '**Learning Objectives:**':
            skip_learning_objectives = True
            objectives_line_count = 0
            continue

        # Count lines in learning objectives section
        if skip_learning_objectives:
            objectives_line_count += 1
            # Check if we've reached the end (typically ends with a header or separator)
            if line.startswith('##') or line.startswith('---') or (objectives_line_count > 20):
                skip_learning_objectives = False
                # Don't skip the current line if it's a header
                if not (line.startswith('##') or line.startswith('---')):
                    continue
            else:
                continue

        # Skip empty lines right after skipped headers
        if skip_next_empty_lines and line.strip() == '':
            continue
        else:
            skip_next_empty_lines = False

        # Headers (skip # LESSON and # APPENDIX as they're chapter titles)
        if line.startswith('# ') and not ('LESSON' in line or 'APPENDIX' in line):
            latex_lines.append(f'\\chapter{{{escape_latex(line[2:].strip())}}}')
            continue
        elif line.startswith('## '):
            title = line[3:].strip()
            latex_lines.append(f'\\section{{{escape_latex(title)}}}')
            latex_lines.append('')  # Add blank line after section
            continue
        elif line.startswith('### '):
            title = line[4:].strip().replace('**', '')
            latex_lines.append(f'\\subsection{{{escape_latex(title)}}}')
            latex_lines.append('')  # Add blank line after subsection
            continue
        elif line.startswith('#### '):
            title = line[5:].strip().replace('**', '')
            latex_lines.append(f'\\subsubsection{{{escape_latex(title)}}}')
            latex_lines.append('')  # Add blank line after subsubsection
            continue

        # WARNING/CAUTION/NOTE boxes
        if '**⚠️ WARNING' in line or'**WARNING' in line:
            # Find the warning text
            warning_text = line.split(':', 1)[-1].strip() if ':' in line else ''
            latex_lines.append(f'\\WARNING{{{escape_latex(warning_text)}}}')
            continue
        elif '**⚠️ CAUTION' in line or '**CAUTION' in line:
            caution_text = line.split(':', 1)[-1].strip() if ':' in line else ''
            latex_lines.append(f'\\CAUTION{{{escape_latex(caution_text)}}}')
            continue
        elif '**⚠️ NOTE' in line or '**NOTE' in line:
            note_text = line.split(':', 1)[-1].strip() if ':' in line else ''
            latex_lines.append(f'\\NOTE{{{escape_latex(note_text)}}}')
            continue

        # Lists
        if re.match(r'^\s*[-*]\s+', line):
            if not in_list or list_type != 'itemize':
                if in_list:
                    latex_lines.append(f'\\end{{{list_type}}}')
                latex_lines.append('\\begin{itemize}')
                in_list = True
                list_type = 'itemize'
            content = re.sub(r'^\s*[-*]\s+', '', line)
            latex_lines.append(f'\\item {convert_inline_formatting(escape_latex(content))}')
            continue
        elif re.match(r'^\s*\d+\.\s+', line):
            if not in_list or list_type != 'enumerate':
                if in_list:
                    latex_lines.append(f'\\end{{{list_type}}}')
                latex_lines.append('\\begin{enumerate}')
                in_list = True
                list_type = 'enumerate'
            content = re.sub(r'^\s*\d+\.\s+', '', line)
            latex_lines.append(f'\\item {convert_inline_formatting(escape_latex(content))}')
            continue
        else:
            if in_list:
                latex_lines.append(f'\\end{{{list_type}}}')
                in_list = False
                list_type = None

        # Tables (basic conversion)
        if line.strip().startswith('|') and '|' in line:
            # This is a table row - simplified conversion
            latex_lines.append(f'% TABLE ROW: {line}')
            continue

        # Skip "END OF LESSON/APPENDIX" markers
        if line.strip().startswith('**END OF LESSON') or line.strip().startswith('**END OF APPENDIX'):
            continue

        # Regular text
        if line.strip():
            converted = convert_inline_formatting(escape_latex(line))
            latex_lines.append(converted)
        else:
            latex_lines.append('')

    # Close any open environments
    if in_list:
        latex_lines.append(f'\\end{{{list_type}}}')
    if in_code_block:
        latex_lines.append('\\end{verbatim}')

    # Clean up multiple consecutive blank lines
    cleaned_lines = []
    prev_blank = False
    for line in latex_lines:
        is_blank = line.strip() == ''
        if is_blank and prev_blank:
            continue  # Skip consecutive blank lines
        cleaned_lines.append(line)
        prev_blank = is_blank

    return '\n'.join(cleaned_lines)

def extract_metadata(content):
    """Extract lesson duration and learning objectives from content"""
    metadata = {
        'duration': None,
        'objectives': []
    }

    # Extract duration
    duration_match = re.search(r'\*\*Lesson Duration:\*\*\s*(.+?)(?:\n|$)', content)
    if duration_match:
        metadata['duration'] = duration_match.group(1).strip()

    # Extract learning objectives
    obj_section = re.search(r'\*\*Learning Objectives:\*\*(.+?)(?=\n##|\n---|\Z)', content, re.DOTALL)
    if obj_section:
        obj_text = obj_section.group(1).strip()
        # Extract bullet points
        for line in obj_text.split('\n'):
            line = line.strip()
            if line.startswith('- '):
                metadata['objectives'].append(line[2:].strip())

    return metadata

def generate_chapter_file(chapter_data, output_dir):
    """Generate a chapter .tex file"""
    if 'number' in chapter_data:
        filename = f"chapter{chapter_data['number']:02d}.tex"
        chapter_title = chapter_data['title']
    else:
        filename = f"appendix{chapter_data['letter']}.tex"
        chapter_title = chapter_data['title']

    # Extract metadata
    metadata = extract_metadata(chapter_data['content'])

    # Generate chapter header
    header_lines = []
    header_lines.append(f'% {chapter_title}')
    header_lines.append('')

    # Add lesson metadata if it's a lesson chapter
    if 'number' in chapter_data and metadata['duration']:
        header_lines.append('\\noindent')
        header_lines.append(f'\\textbf{{Lesson Duration:}} {escape_latex(metadata["duration"])}')
        header_lines.append('')
        header_lines.append('\\vspace{0.3cm}')
        header_lines.append('')

        if metadata['objectives']:
            header_lines.append('\\noindent')
            header_lines.append('\\textbf{Learning Objectives:}')
            header_lines.append('')
            header_lines.append('Upon completion of this lesson, you will be able to:')
            header_lines.append('')
            header_lines.append('\\begin{itemize}[leftmargin=2em,itemsep=3pt]')
            for obj in metadata['objectives']:
                header_lines.append(f'\\item {escape_latex(obj)}')
            header_lines.append('\\end{itemize}')
            header_lines.append('')
            header_lines.append('\\vspace{0.5cm}')
            header_lines.append('')

    # Convert main content
    latex_content = convert_markdown_to_latex(chapter_data['content'])

    # Combine header and content
    full_content = '\n'.join(header_lines) + '\n' + latex_content

    output_path = output_dir / filename
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(full_content)

    print(f"Generated: {filename}")
    return filename

def main():
    print("="*60)
    print("El 7arress RCWS - LaTeX Manual Generator")
    print("="*60)
    print()

    # Read markdown
    print("Reading markdown file...")
    content = read_markdown_file()
    print(f"✓ Loaded {len(content)} characters")

    # Extract chapters
    print("\nExtracting chapters...")
    chapters = extract_chapters(content)
    print(f"✓ Found {len([k for k in chapters if k.startswith('lesson')])} lessons")
    print(f"✓ Found {len([k for k in chapters if k.startswith('appendix')])} appendices")

    # Generate chapter files
    print("\nGenerating chapter files...")
    chapters_dir = Path(__file__).parent / "chapters"
    appendices_dir = Path(__file__).parent / "appendices"

    chapters_dir.mkdir(exist_ok=True)
    appendices_dir.mkdir(exist_ok=True)

    chapter_files = []
    appendix_files = []

    for key, data in sorted(chapters.items()):
        if key.startswith('lesson'):
            filename = generate_chapter_file(data, chapters_dir)
            chapter_files.append(filename)
        elif key.startswith('appendix'):
            filename = generate_chapter_file(data, appendices_dir)
            appendix_files.append(filename)

    print("\n" + "="*60)
    print("Generation Complete!")
    print("="*60)
    print(f"\nChapters: {len(chapter_files)}")
    print(f"Appendices: {len(appendix_files)}")
    print("\nNext steps:")
    print("  1. Review generated .tex files in chapters/ and appendices/")
    print("  2. Manually convert tables to LaTeX format")
    print("  3. Fine-tune formatting and spacing")
    print("  4. Update operator_manual.tex to include all chapters")
    print("  5. Run 'make' to compile the PDF")
    print()

if __name__ == '__main__':
    main()
