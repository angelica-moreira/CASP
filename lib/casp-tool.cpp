//===- casp-tool.cpp - CASP Standalone Tool ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Standalone tool that reads LLVM IR and generates static profile data.
//
//===----------------------------------------------------------------------===//

#include "StaticProfileExporter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static void printHelp(const char *ProgName) {
  outs() << "OVERVIEW: CASP - Coverage Approximation via Static Profiles\n\n"
         << "DESCRIPTION:\n"
         << "  This tool generates static profile data from LLVM IR using block\n"
         << "  frequency analysis. The output is compatible with llvm-profdata and\n"
         << "  can be used with llvm-cov for coverage visualization.\n\n"
         << "USAGE: " << ProgName << " <input.ll> [output.profdata]\n\n"
         << "ARGUMENTS:\n"
         << "  <input.ll>        LLVM IR input file (.ll or .bc)\n"
         << "  [output.profdata] Output profile file (default: output.profdata)\n\n"
         << "EXAMPLES:\n"
         << "  # Generate static profile from IR\n"
         << "  " << ProgName << " program.ll profile.profdata\n\n"
         << "  # Use with default output filename\n"
         << "  " << ProgName << " program.ll\n\n"
         << "  # View coverage with llvm-cov\n"
         << "  llvm-cov show program -instr-profile=profile.profdata\n\n";
}

int main(int argc, char **argv) {
  // Check for help before InitLLVM processes arguments
  if (argc == 2 && (StringRef(argv[1]) == "-h" || 
                    StringRef(argv[1]) == "--help" || 
                    StringRef(argv[1]) == "-help")) {
    printHelp(argv[0]);
    return 0;
  }

  InitLLVM X(argc, argv);

  if (argc < 2 || argc > 3) {
    errs() << "Usage: " << argv[0] << " <input.ll> [output.profdata]\n";
    errs() << "Run '" << argv[0] << " --help' for more information.\n";
    return 1;
  }

  std::string InputFilename = argv[1];
  std::string OutputFilename = argc == 3 ? argv[2] : "output.profdata";

  LLVMContext Context;
  SMDiagnostic Err;

  // Load the input module
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  // Create analysis managers
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  // Create pass builder and register analyses
  PassBuilder PB;
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  // Run the static profile exporter pass
  ModulePassManager MPM;
  MPM.addPass(StaticProfileExporterPass(OutputFilename));
  MPM.run(*M, MAM);

  outs() << "Static profile written to: " << OutputFilename << "\n";
  return 0;
}
