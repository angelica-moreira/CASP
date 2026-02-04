//===- StaticProfileExporter.h - Export Static Profile Info ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the StaticProfileExporterPass which generates statically
// inferred profile data from LLVM IR using Block Frequency Information (BFI).
// This pass analyzes control flow and generates execution count estimates that
// can be used with LLVM's profiling and coverage tools (llvm-profdata, llvm-cov).
//
// Key Features:
// - Extracts coverage metadata from instrumented IR when available
// - Scales BFI frequencies to realistic execution counts
// - Generates .profdata files compatible with llvm-cov
// - Supports both instrumented and non-instrumented IR the later does not
// produce a .profdata that is compatible with llvm-cov
//
//===----------------------------------------------------------------------===//

#ifndef CASP_STATICPROFILEEXPORTER_H
#define CASP_STATICPROFILEEXPORTER_H

#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include <string>

namespace llvm {

class Module;

// Command-line option for Wu-Larus heuristics
extern cl::opt<bool> UseWuLarusHeuristics;

class StaticProfileExporterPass : public PassInfoMixin<StaticProfileExporterPass> {
  std::string ProfilePath;

public:
  explicit StaticProfileExporterPass(std::string Path = "")
      : ProfilePath(std::move(Path)) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

} // namespace llvm

#endif // CASP_STATICPROFILEEXPORTER_H
