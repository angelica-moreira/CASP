# CASP
**C**overage **A**pproximation via **S**tatic **P**rofiles (CASP) is a static analysis tool that approximates code coverage without executing tests. CASP leverages LLVM's Block Frequency Information (BFI) to map static execution frequencies to program counters, producing LLVM-compatible profile data (.profdata).

BFI employs a mass distribution algorithm to compute block frequencies, using loop scaling formulas mathematically equivalent to those described in Wu and Larus's foundational work on static branch frequency analysis [Wu & Larus, MICRO '94]. CASP exports these statically inferred frequencies as execution counts, which can then be consumed by existing LLVM coverage tooling to estimate which regions of the program are likely to be exercised.

By reusing LLVM's profiling and coverage infrastructure, CASP enables coverage reasoning in environments where dynamic instrumentation is unavailable, incomplete, or prohibitively expensive, such as large systems code, kernels, and patched code paths.

---

**Reference:**

1. **LLVM Block Frequency Terminology**
   LLVM Project. (n.d.). *Block Frequency Terminology*.
   https://llvm.org/docs/BlockFrequencyTerminology.html

2. **Wu, Y., & Larus, J. R.** (1994).
   Static branch frequency and program profile analysis.
   In *Proceedings of the 27th Annual International Symposium on Microarchitecture (MICRO-27)*, 1–8.
   https://doi.org/10.1145/192724.192725
