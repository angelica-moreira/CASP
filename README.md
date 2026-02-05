# CASP
**C**overage **A**pproximation via **S**tatic **P**rofiles (CASP) is a static analysis tool that approximates code coverage without executing tests. CASP leverages LLVM's Block Frequency Information (BFI) to map static execution frequencies to program counters, producing LLVM-compatible profile data (.profdata).

BFI employs a mass distribution algorithm to compute block frequencies, using loop scaling formulas mathematically equivalent to those described in Wu and Larus's foundational work on static branch frequency analysis [Wu & Larus, MICRO '94]. CASP exports these statically inferred frequencies as execution counts, which can then be consumed by existing LLVM coverage tooling to estimate which regions of the program are likely to be exercised.

By reusing LLVM's profiling and coverage infrastructure, CASP enables coverage reasoning in environments where dynamic instrumentation is unavailable, incomplete, or prohibitively expensive, such as large systems code, kernels, and patched code paths.

## Features

- **BFI-Based Execution Counts**: Uses LLVM's Block Frequency Information to generate realistic execution count estimates
- **LLVM Tooling Integration**: Generates `.profdata` files compatible with `llvm-profdata` and `llvm-cov`
- **Dual Interface**: Available both as a standalone tool (`llvm-sprofgen`) and as an LLVM plugin
- **Instrumented IR Support**: Works with coverage-instrumented IR to produce profiles compatible with LLVM's coverage visualization tools

## Building

```bash
mkdir build && cd build
cmake ..
make
```

### Requirements
- LLVM 20.1.2
- CMake 3.28 or later
- C++17 compiler

## Usage

### Standalone Tool

```bash
llvm-sprofgen <input.ll> [output.profdata]
```

**Complete Workflow:**
```bash
# Step 1: Compile with coverage instrumentation (embeds coverage mapping)
clang -fprofile-instr-generate -fcoverage-mapping -S -emit-llvm -O2 program.c -o program.ll

# Step 2: Generate static profile from instrumented IR
llvm-sprofgen program.ll static.profdata

# Step 3: Compile executable with coverage mapping (for llvm-cov)
clang -fprofile-instr-generate -fcoverage-mapping -O2 -g program.c -o program

# Step 4: View coverage report
llvm-cov show program --instr-profile=static.profdata

# Step 5: Generate coverage summary
llvm-cov report program --instr-profile=static.profdata

# Step 6: Generate HTML coverage report
llvm-cov show program --instr-profile=static.profdata --format=html > coverage.html
```

See `examples/generate_static_coverage.sh` for a complete working example.

### Using with CMake Projects

For projects using CMake, you can generate a compilation database and use CASP with each compilation unit:

```bash
# Step 1: Configure CMake to generate compilation database with coverage flags
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
      -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
      -B build

# Step 2: Build the project to generate LLVM IR for each source file
# (Modify compile_commands.json to add -S -emit-llvm flags)
# Or manually compile each source to IR:
clang -fprofile-instr-generate -fcoverage-mapping -S -emit-llvm -O2 source.c -o source.ll

# Step 3: Generate static profile for each compilation unit
llvm-sprofgen source.ll source.profdata

# Step 4: Merge profiles if multiple compilation units
llvm-profdata merge -o merged.profdata source1.profdata source2.profdata ...

# Step 5: Build the final executable with coverage mapping
cmake --build build

# Step 6: Generate coverage report
llvm-cov show ./build/program --instr-profile=merged.profdata
```

## Examples

The `examples/` directory contains demonstrations of CASP's capabilities:

### Quick Start

**Basic Workflow** - Generate static coverage in 7 steps:
```bash
cd examples
./test_standalone.sh simple.c
```

This demonstrates the complete workflow from source code to coverage visualization using a simple factorial example.

**Test Cases:**

1. **Simple Factorial** (`simple.c`) - Control flow with branching:
   ```bash
   ./test_standalone.sh simple.c
   ```
   - Functions: 2 (main, factorial)
   - Branches: 4
   - **Coverage: 100%** (all branches statically reachable)
   - Demonstrates recursive calls and conditional branches

2. **Buffer Copy** (`test_buffer_copy.c`) - Error handling and loops:
   ```bash
   ./test_standalone.sh test_buffer_copy.c
   ```
   - Functions: 2 (buffer_copy, main)
   - Branches: 24
   - **Coverage: 95.83%** (high approximation accuracy)
   - Demonstrates null checks, loops, and assertions

### How CASP Approximates Coverage

CASP uses LLVM's Block Frequency Information (BFI) to estimate execution likelihood:

- **Static reachability**: All syntactically reachable code paths are considered
- **Branch probabilities**: Assigns default 50/50 split or uses heuristics (loops: 88%, returns: 72%, it reuses LLVM's Branch Probability Info)
- **Coverage estimation**: Reports which code regions *can* execute, not what *did* execute

**Example Output** (simple.c):
```
Filename        Regions  Missed  Cover   Branches  Missed  Cover
simple.c              8       0  100.00%         4       0  100.00%

Total functions: 2
Maximum function count: 100
Total count: 1050
```

The approximated coverage shows:
- All 4 branches as taken (100% coverage)
- Execution counts scaled by `DefaultEntryCount = 100`
- Recursive factorial shows amplified count (800) from loop analysis

### Available Test Scripts

**`test_standalone.sh [source.c] [llvm_version]`** - Automated static profiling
- Accepts custom source files (default: `simple.c`)
- Auto-detects LLVM version (tries 20, system default)
- Compiles with coverage instrumentation
- Generates static profile with CASP
- Creates HTML coverage report
- **Recommended for first-time users**

**`test/static_vs_dynamic_test.sh`** - Compare static vs dynamic profiling
- Part A: Static profiling with CASP (BFI-based estimates)
- Part B: Dynamic profiling (actual runtime execution)
- Part C: Side-by-side comparison with analysis
- Shows hash compatibility, coverage differences, and execution counts
- Generates comparison documentation

### Sample Output Files

After running the tests, you'll find:
- `*.profdata` - Static profile data (LLVM format)
- `coverage_static.html` - HTML coverage visualization
- `*_instr.ll` - Instrumented LLVM IR
- `*_cov` - Test executables with coverage mapping

---

**Reference:**

1. **LLVM Block Frequency Terminology**
   LLVM Project. (n.d.). *Block Frequency Terminology*.
   https://llvm.org/docs/BlockFrequencyTerminology.html

2. **Wu, Y., & Larus, J. R.** (1994).
   Static branch frequency and program profile analysis.
   In *Proceedings of the 27th Annual International Symposium on Microarchitecture (MICRO-27)*, 1–8.
   https://doi.org/10.1145/192724.192725
