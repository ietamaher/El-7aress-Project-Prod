#!/bin/bash
#
# Build script for El 7arress RCWS Operator Manual
# Handles compilation, cleanup, and PDF generation
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

MAIN_DOC="operator_manual"
BUILD_DIR="build"
OUTPUT_PDF="${MAIN_DOC}.pdf"

# Functions
print_header() {
    echo -e "${BLUE}=====================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}=====================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Check for required tools
check_dependencies() {
    print_header "Checking Dependencies"

    local missing_deps=0

    if ! command -v pdflatex &> /dev/null; then
        print_error "pdflatex not found. Please install TeX Live."
        missing_deps=1
    else
        print_success "pdflatex found"
    fi

    if ! command -v makeindex &> /dev/null; then
        print_warning "makeindex not found. Index generation will be skipped."
    else
        print_success "makeindex found"
    fi

    if [ $missing_deps -eq 1 ]; then
        echo ""
        print_error "Missing required dependencies. Please install:"
        echo "  Ubuntu/Debian: sudo apt-get install texlive-full"
        echo "  Fedora:        sudo dnf install texlive-scheme-full"
        echo "  macOS:         brew install --cask mactex"
        exit 1
    fi

    echo ""
}

# Create build directory
setup_build_dir() {
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
        print_info "Created build directory"
    fi
}

# Compile LaTeX document
compile_latex() {
    print_header "Compiling LaTeX Document"

    local pass=$1
    print_info "Compilation pass $pass/3..."

    if pdflatex -interaction=nonstopmode -output-directory="$BUILD_DIR" "${MAIN_DOC}.tex" > /dev/null 2>&1; then
        print_success "Pass $pass completed"
    else
        print_error "Pass $pass failed"
        print_warning "Check ${BUILD_DIR}/${MAIN_DOC}.log for details"
        echo ""
        echo "Last 20 lines of log:"
        tail -n 20 "${BUILD_DIR}/${MAIN_DOC}.log"
        exit 1
    fi
}

# Copy PDF to main directory
copy_pdf() {
    if [ -f "${BUILD_DIR}/${OUTPUT_PDF}" ]; then
        cp "${BUILD_DIR}/${OUTPUT_PDF}" "./${OUTPUT_PDF}"
        print_success "PDF copied to $(pwd)/${OUTPUT_PDF}"
    else
        print_error "PDF not found in build directory"
        exit 1
    fi
}

# Get PDF file size
get_pdf_size() {
    if [ -f "${OUTPUT_PDF}" ]; then
        local size=$(du -h "${OUTPUT_PDF}" | cut -f1)
        echo "$size"
    else
        echo "N/A"
    fi
}

# Count pages in PDF
count_pages() {
    if command -v pdfinfo &> /dev/null && [ -f "${OUTPUT_PDF}" ]; then
        pdfinfo "${OUTPUT_PDF}" 2>/dev/null | grep "Pages:" | awk '{print $2}'
    else
        echo "N/A"
    fi
}

# Clean build files
clean_build() {
    print_header "Cleaning Build Files"

    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory removed"
    fi

    # Remove any stray auxiliary files in current directory
    rm -f ./*.aux ./*.log ./*.out ./*.toc ./*.lof ./*.lot ./*.bbl ./*.blg ./*.idx ./*.ind ./*.ilg
    print_success "Auxiliary files cleaned"

    if [ "$1" == "all" ]; then
        if [ -f "${OUTPUT_PDF}" ]; then
            rm -f "${OUTPUT_PDF}"
            print_success "PDF removed"
        fi
    fi

    echo ""
}

# Main build process
build() {
    print_header "El 7arress RCWS Operator Manual - Build Script"
    echo ""

    check_dependencies
    setup_build_dir

    # Three-pass compilation
    compile_latex 1
    compile_latex 2
    compile_latex 3

    copy_pdf

    echo ""
    print_header "Build Summary"
    print_success "Compilation successful!"
    print_info "Output: ${OUTPUT_PDF}"
    print_info "Size: $(get_pdf_size)"
    print_info "Pages: $(count_pages)"
    echo ""
    print_info "To view the PDF:"
    echo "  Linux:   xdg-open ${OUTPUT_PDF}"
    echo "  macOS:   open ${OUTPUT_PDF}"
    echo "  Windows: start ${OUTPUT_PDF}"
    echo ""
}

# Show usage
show_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  build      - Compile the LaTeX document (default)"
    echo "  clean      - Remove build files (keep PDF)"
    echo "  cleanall   - Remove build files and PDF"
    echo "  view       - Build and open the PDF"
    echo "  help       - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0              # Build PDF"
    echo "  $0 build        # Build PDF"
    echo "  $0 view         # Build and open PDF"
    echo "  $0 clean        # Clean build files"
    echo ""
}

# Open PDF viewer
view_pdf() {
    if [ ! -f "${OUTPUT_PDF}" ]; then
        print_warning "PDF not found. Building..."
        build
    fi

    print_info "Opening PDF..."

    if command -v xdg-open &> /dev/null; then
        xdg-open "${OUTPUT_PDF}" &
    elif command -v open &> /dev/null; then
        open "${OUTPUT_PDF}"
    elif command -v evince &> /dev/null; then
        evince "${OUTPUT_PDF}" &
    else
        print_warning "No PDF viewer found. Please open ${OUTPUT_PDF} manually."
    fi
}

# Main script logic
case "${1:-build}" in
    build)
        build
        ;;
    clean)
        clean_build
        ;;
    cleanall)
        clean_build all
        ;;
    view)
        build
        view_pdf
        ;;
    help|--help|-h)
        show_usage
        ;;
    *)
        print_error "Unknown command: $1"
        echo ""
        show_usage
        exit 1
        ;;
esac
