#!/usr/bin/env python3
"""
Update the main operator_manual.tex file to include all generated chapters
"""

from pathlib import Path

def create_complete_manual():
    """Create complete manual with all chapters included"""

    main_file = Path(__file__).parent / "operator_manual.tex"
    original_file = Path(__file__).parent / "operator_manual_original.tex"

    # Read the original up to \mainmatter
    with open(original_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Find where \mainmatter starts
    mainmatter_line = None
    for i, line in enumerate(lines):
        if r'\mainmatter' in line:
            mainmatter_line = i
            break

    if mainmatter_line is None:
        print("Error: Could not find \\mainmatter in original file")
        return

    # Keep everything up to and including mainmatter
    new_lines = lines[:mainmatter_line+1]

    # Add chapter includes
    new_lines.append('\n')
    new_lines.append('% ============================================================================\n')
    new_lines.append('% CHAPTERS (Auto-generated from markdown)\n')
    new_lines.append('% ============================================================================\n')
    new_lines.append('\n')

    for i in range(1, 11):
        new_lines.append(f'% Chapter {i}\n')
        new_lines.append(f'\\input{{chapters/chapter{i:02d}}}\n')
        new_lines.append('\\clearpage\n')
        new_lines.append('\n')

    # Add backmatter
    new_lines.append('% ============================================================================\n')
    new_lines.append('% BACK MATTER\n')
    new_lines.append('% ============================================================================\n')
    new_lines.append('\\backmatter\n')
    new_lines.append('\n')

    # Add appendices
    new_lines.append('% ============================================================================\n')
    new_lines.append('% APPENDICES\n')
    new_lines.append('% ============================================================================\n')
    new_lines.append('\\appendix\n')
    new_lines.append('\n')

    for letter in ['A', 'B', 'C', 'D']:
        new_lines.append(f'% Appendix {letter}\n')
        new_lines.append(f'\\input{{appendices/appendix{letter}}}\n')
        new_lines.append('\\clearpage\n')
        new_lines.append('\n')

    # Add end of document
    new_lines.append('% ============================================================================\n')
    new_lines.append('% END OF DOCUMENT\n')
    new_lines.append('% ============================================================================\n')
    new_lines.append('\n')
    new_lines.append('\\end{document}\n')

    # Write the new file
    with open(main_file, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)

    print("✓ Updated operator_manual.tex")
    print(f"  - Includes {10} chapters")
    print(f"  - Includes {4} appendices")
    print("\nTo compile:")
    print("  make")
    print("  or")
    print("  ./build.sh")

if __name__ == '__main__':
    create_complete_manual()
