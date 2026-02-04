#!/bin/bash
# Complete workflow: Generate static coverage using CASP
#
# Usage: ./test_standalone.sh [source_file.c] [llvm_version]
#   source_file.c - Source file to analyze (default: simple.c)
#   llvm_version  - LLVM version to use (default: auto-detect or system default)
#
# Examples:
#   ./test_standalone.sh                    # Use simple.c with default LLVM
#   ./test_standalone.sh my_program.c       # Use my_program.c with default LLVM
#   ./test_standalone.sh my_program.c 20    # Use my_program.c with LLVM 20
#   ./test_standalone.sh simple.c ""        # Use simple.c with system default LLVM

set -e

SOURCE_FILE="${1:-simple.c}"
LLVM_VERSION="${2:-}"

if [ -z "$LLVM_VERSION" ]; then
    ver=20
    if command -v clang-$ver &> /dev/null; then
         LLVM_VERSION=$ver
         echo "Auto-detected LLVM version: $LLVM_VERSION"
    elif command -v clang &> /dev/null; then
        LLVM_VERSION=""
        echo "Using system default LLVM (no version suffix)"
    else
        echo "Error: No LLVM installation found"
        echo "Please install LLVM or specify the version explicitly"
        exit 1
    fi
fi

if [ -n "$LLVM_VERSION" ]; then
    CLANG="clang-${LLVM_VERSION}"
    LLVM_PROFDATA="llvm-profdata-${LLVM_VERSION}"
    LLVM_COV="llvm-cov-${LLVM_VERSION}"
else
    CLANG="clang"
    LLVM_PROFDATA="llvm-profdata"
    LLVM_COV="llvm-cov"
fi

for tool in "$CLANG" "$LLVM_PROFDATA" "$LLVM_COV"; do
    if ! command -v "$tool" &> /dev/null; then
        echo "Error: Required tool '$tool' not found"
        echo "Please install LLVM${LLVM_VERSION:+ $LLVM_VERSION} or adjust the version"
        exit 1
    fi
done

BASENAME=$(basename "$SOURCE_FILE" .c)
IR_FILE="${BASENAME}_instr.ll"
EXECUTABLE="${BASENAME}_cov"
PROFDATA="static_coverage.profdata"
HTML_REPORT="coverage_static.html"

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: Source file '$SOURCE_FILE' not found"
    exit 1
fi


echo "========================================"
echo "CASP Static Coverage Workflow"
echo "========================================"
echo "Source file:    $SOURCE_FILE"
echo "LLVM version:   ${LLVM_VERSION:-system default}"
echo "Clang:          $CLANG"
echo "llvm-profdata:  $LLVM_PROFDATA"
echo "llvm-cov:       $LLVM_COV"
echo "========================================"
echo ""

echo "=== Step 1: Compile with coverage instrumentation (to embed coverage mapping) ==="
$CLANG -fprofile-instr-generate -fcoverage-mapping -S -emit-llvm -O2 "$SOURCE_FILE" -o "$IR_FILE"

echo "" 
echo "=== Step 2: Compile executable with coverage mapping (for llvm-cov) ==="
$CLANG -fprofile-instr-generate -fcoverage-mapping -O2 -g "$SOURCE_FILE" -o "$EXECUTABLE"

echo "" 
echo "=== Step 3: Generate static profile from instrumented IR ==="
../build/llvm-sprofgen "$IR_FILE" "$PROFDATA"

echo "" 
echo "=== Step 4: View profile statistics ==="
$LLVM_PROFDATA show "$PROFDATA" --all-functions

echo ""
echo "=== Step 5: Generate coverage report with llvm-cov ==="
$LLVM_COV show "$EXECUTABLE" --instr-profile="$PROFDATA"

echo ""
echo "=== Step 6: Generate coverage summary ==="
$LLVM_COV report "$EXECUTABLE" --instr-profile="$PROFDATA"

echo ""
echo "=== Step 7: Generate HTML coverage report ==="
$LLVM_COV show "$EXECUTABLE" --instr-profile="$PROFDATA" --format=html > "$HTML_REPORT"
echo " HTML coverage report saved to: $HTML_REPORT"

echo ""
echo "========================================"
echo "Generated files:"
echo "  - $IR_FILE (instrumented LLVM IR)"
echo "  - $PROFDATA (static profile)"
echo "  - $EXECUTABLE (executable with coverage mapping)"
echo "  - $HTML_REPORT (HTML coverage report)"
echo ""
echo "View coverage in browser: file://$(pwd)/$HTML_REPORT"
echo "========================================"
