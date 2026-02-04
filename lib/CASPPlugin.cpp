//===- CASPPlugin.cpp - CASP Plugin Registration -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the plugin registration for CASP (Coverage
// Approximation via Static Profiles).
//
//===----------------------------------------------------------------------===//

#include "StaticProfileExporter.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<std::string> StaticProfileDumpPath(
    "static-profile-dump-path",
    cl::desc("Path to write static profile data"),
    cl::value_desc("filename"),
    cl::init(""));

static cl::opt<bool> StaticProfileDump(
    "static-profile-dump",
    cl::desc("Enable static profile dump"),
    cl::init(false));

static void registerCASPCallbacks(PassBuilder &PB) {
  // Register the pass as an optimizer-last callback
  PB.registerOptimizerLastEPCallback(
      [](ModulePassManager &MPM, OptimizationLevel Level, 
         ThinOrFullLTOPhase Phase) {
        // Determine the output path
        std::string OutputPath = StaticProfileDumpPath;
        if (OutputPath.empty() && StaticProfileDump) {
          OutputPath = "default.profdata";
        }
        
        // We only add the pass if we have an output path
        if (!OutputPath.empty()) {
          MPM.addPass(StaticProfileExporterPass(OutputPath));
        }
      });
}

// Plugin registration
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CASP", LLVM_VERSION_STRING,
          registerCASPCallbacks};
}
