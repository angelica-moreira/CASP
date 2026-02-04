//===- StaticProfileExporter.cpp - Export Static Profile Info -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the StaticProfileExporterPass which exports statically
// inferred profile information from BlockFrequencyInfo in .profdata format 
// for use with llvm-cov and other profile tools.
//
//===----------------------------------------------------------------------===//

#include "StaticProfileExporter.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/ProfileData/InstrProf.h"
#include "llvm/ProfileData/InstrProfWriter.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

#define DEBUG_TYPE "static-profile-export"

using namespace llvm;

namespace llvm {

// Default entry count for scaling static frequencies to absolute counts.
// This value is chosen to be large enough to provide granularity in frequency
// ratios while avoiding overflow in typical calculations. Based on VESPA paper
// and LLVM's sample profile conventions.
static constexpr uint64_t DefaultEntryCount = 100;//1000000;

/// Extract the structural hash from coverage mapping metadata.
/// Coverage records (__covrec_) contain the hash needed for llvm-cov compatibility.
/// Structure: { name_hash (i64), data_size (i32), struct_hash (i64), ... }
static std::optional<uint64_t> tryExtractCoverageHash(const Module &M, const Function &F) {
  // We need to look for coverage record global like so __covrec_<name_hash>u
  std::string FuncName = getPGOFuncName(F);
  uint64_t NameHash = IndexedInstrProf::ComputeHash(FuncName);
  
  // The coverage records are named __covrec_<hexhash>u where hash is the function name hash
  SmallString<64> CovRecName;
  raw_svector_ostream OS(CovRecName);
  OS << "__covrec_" << format_hex_no_prefix(NameHash, 16, /*Upper=*/true) << "u";
  
  if (const GlobalVariable *CovRec = M.getNamedGlobal(OS.str())) {
    // We extract structural hash from the coverage record
    // Field 0: name hash (i64) -> for naming the record
    // Field 1: data size (i32) -> size of encoded mapping data
    // Field 2: structural hash (i64) -> THIS is what we need for profile compatibility
    if (const ConstantStruct *CS = dyn_cast_or_null<ConstantStruct>(CovRec->getInitializer())) {
      if (CS->getNumOperands() >= 3) {
        if (const ConstantInt *StructHashVal = dyn_cast<ConstantInt>(CS->getOperand(2))) {
          uint64_t StructHash = StructHashVal->getZExtValue();
          LLVM_DEBUG(dbgs() << "Extracted coverage struct hash for " << F.getName() 
                           << ": " << format_hex(StructHash, 18) << "\n");
          return StructHash;
        }
      }
    }
  }
  
  return std::nullopt;
}

/// Compute the function hash for profile compatibility.
/// 
/// Note: Always prefer the structural hash from coverage mapping metadata, if present,
/// because it ensures compatibility with llvm-cov. This function falls back to PGO name hash
/// for non-instrumented functions.
static uint64_t computeFunctionHash(const Module &M, const Function &F) {
  // First try to extract hash from coverage mapping (if IR was instrumented)
  if (auto CovHash = tryExtractCoverageHash(M, F)) {
    LLVM_DEBUG(dbgs() << "Using coverage struct hash for " << F.getName() 
                      << ": " << format_hex(*CovHash, 18) << "\n");
    return *CovHash;
  }
  
  // Fallback: compute MD5 hash of function name (this is the standard PGO method)
  // This will work for PGO but not for coverage visualization with llvm-cov
  std::string FuncName = getPGOFuncName(F);
  uint64_t Hash = IndexedInstrProf::ComputeHash(FuncName);
  LLVM_DEBUG(dbgs() << "Using PGO name hash for " << F.getName() 
                    << ": " << format_hex(Hash, 18) << " (no coverage metadata)\n");
  return Hash;
}

/// Try to extract the number of counters from PGO instrumentation metadata.
/// The __profd_ global contains profiling metadata including counter count.
/// Structure: { name_hash (i64), cfg_hash (i64), counter_ptr_offset (i64), 
///              function_ptr (i64), values (ptr), num_value_sites (ptr),
///              num_counters (i32), ... }
static std::optional<unsigned> tryExtractCounterCount(const Module &M, const Function &F) {
  std::string FuncName = getPGOFuncName(F);
  std::string ProfdName = "__profd_" + FuncName;
  
  if (const GlobalVariable *Profd = M.getNamedGlobal(ProfdName)) {
    if (const ConstantStruct *CS = dyn_cast_or_null<ConstantStruct>(Profd->getInitializer())) {
      // num_counters is the 7th field (index 6) in the __profd_ struct
      if (CS->getNumOperands() > 6) {
        if (const ConstantInt *NumCounters = dyn_cast<ConstantInt>(CS->getOperand(6))) {
          unsigned Count = NumCounters->getZExtValue();
          LLVM_DEBUG(dbgs() << "Extracted counter count for " << F.getName() 
                           << ": " << Count << "\n");
          return Count;
        }
      }
    }
  }
  
  return std::nullopt;
}

/// Convert BlockFrequencyInfo frequencies to execution counts.
/// 
/// This function scales BFI relative frequencies to absolute execution counts.
/// If the IR contains instrumentation metadata, it matches the expected counter
/// layout from the coverage mapping.
///
/// Note:
/// - For instrumented IR: We match the counter count from __profd_, use scaled BFI
/// - For non-instrumented IR: We create one counter per basic block with BFI frequencies
/// 
/// All frequencies are scaled relative to the entry block frequency to produce
/// realistic execution count estimates.
static bool convertBFIToCounts(const Module &M, const Function &F, const BlockFrequencyInfo &BFI,
                                std::vector<uint64_t> &Counts) {
  const BasicBlock &EntryBB = F.getEntryBlock();
  BlockFrequency EntryFreq = BFI.getBlockFreq(&EntryBB);
  
  if (EntryFreq.getFrequency() == 0) {
    LLVM_DEBUG(dbgs() << "Warning: Entry block has zero frequency for "
                      << F.getName() << ", skipping\n");
    return false;
  }

  Counts.clear();
  
  auto InstrCounterCount = tryExtractCounterCount(M, F);
  
  if (InstrCounterCount) {
    // IR is instrumented - match the counter layout from coverage mapping
    // 
    // Note: Proper implementation would parse the coverage mapping to understand
    // which counters correspond to which regions, then map BFI blocks to those
    // regions. For now, we use a simplified approach:
    // - Counter 0: Entry block execution count
    // - Remaining counters: Scaled based on average block frequency
    //
    // This is a heuristic that works reasonably well for basic coverage estimation
    // but doesn't capture the precise counter-to-region mapping.
    
    LLVM_DEBUG(dbgs() << "Function " << F.getName() << " has " << *InstrCounterCount 
                      << " instrumented counters\n");
    
    // Collect all block frequencies and sort them
    std::vector<uint64_t> BlockFreqs;
    BlockFreqs.reserve(std::distance(F.begin(), F.end()));
    
    for (const BasicBlock &BB : F) {
      BlockFrequency BBFreq = BFI.getBlockFreq(&BB);
      uint64_t ScaledCount = (DefaultEntryCount * BBFreq.getFrequency()) / 
                             EntryFreq.getFrequency();
      BlockFreqs.push_back(ScaledCount);
    }
    
    // Sort in descending order to assign higher counts to early counters
    std::sort(BlockFreqs.rbegin(), BlockFreqs.rend());
    
    // Assign counts to instrumentation counters
    // Counter 0 always gets entry count
    Counts.push_back(DefaultEntryCount);
    
    // Distribute remaining block frequencies to counters
    // If we have more counters than blocks, pad with progressively lower counts
    // If we have fewer counters than blocks, use the highest frequency blocks
    for (unsigned i = 1; i < *InstrCounterCount; ++i) {
      if (i < BlockFreqs.size()) {
        Counts.push_back(BlockFreqs[i]);
      } else {
        // We pad with scaled-down entry count for regions beyond our block count
        Counts.push_back(DefaultEntryCount / (i + 1));
      }
    }
    
    LLVM_DEBUG({
      dbgs() << "Counter assignment for " << F.getName() << ":\n";
      for (unsigned i = 0; i < Counts.size(); ++i) {
        dbgs() << "  Counter[" << i << "] = " << Counts[i] << "\n";
      }
    });
    
  } else {
    // If no instrumentation, then we  use one counter per basic block.
    // Note: This mode is useful for understanding static control flow but won't work
    // with llvm-cov (which requires coverage mapping metadata)!
    
    LLVM_DEBUG(dbgs() << "Function " << F.getName() 
                      << " has no instrumentation, using per-block counters\n");
    
    for (const BasicBlock &BB : F) {
      BlockFrequency BBFreq = BFI.getBlockFreq(&BB);
      uint64_t Count = (DefaultEntryCount * BBFreq.getFrequency()) / 
                       EntryFreq.getFrequency();
      Counts.push_back(Count);
      
      LLVM_DEBUG(dbgs() << "  BB " << BB.getName() << ": freq=" 
                        << BBFreq.getFrequency() << " → count=" << Count << "\n");
    }
  }
  
  return !Counts.empty();
}

PreservedAnalyses StaticProfileExporterPass::run(Module &M,
                                                  ModuleAnalysisManager &MAM) {
  if (ProfilePath.empty()) {
    errs() << "Warning: No profile output path specified\n";
    return PreservedAnalyses::all();
  }

  auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  InstrProfWriter Writer;
  unsigned FunctionsProcessed = 0;
  unsigned FunctionsSkipped = 0;

  for (Function &F : M) {
    if (F.isDeclaration()) {
      LLVM_DEBUG(dbgs() << "Skipping declaration: " << F.getName() << "\n");
      continue;
    }

    // Get block frequency analysis for this function
    BlockFrequencyInfo &BFI = FAM.getResult<BlockFrequencyAnalysis>(F);

    // Convert BFI frequencies into execution counts
    std::vector<uint64_t> Counts;
    if (!convertBFIToCounts(M, F, BFI, Counts)) {
      LLVM_DEBUG(dbgs() << "Failed to convert BFI to counts for " 
                        << F.getName() << ", skipping\n");
      ++FunctionsSkipped;
      continue;
    }
    
    // Get function name and hash for profile record
    std::string FuncName = getIRPGOFuncName(F);
    uint64_t FuncHash = computeFunctionHash(M, F);
    
    // Create and add profile record
    NamedInstrProfRecord Record(FuncName, FuncHash, Counts);
    
    Writer.addRecord(std::move(Record), 1, [&](Error Err) {
      errs() << "Warning: Failed to add profile record for " << F.getName() 
             << ": " << toString(std::move(Err)) << "\n";
      ++FunctionsSkipped;
    });
    
    ++FunctionsProcessed;
    LLVM_DEBUG(dbgs() << "Added profile for " << F.getName() << " (" 
                      << Counts.size() << " counters)\n");
  }

  if (FunctionsProcessed == 0) {
    errs() << "Warning: No functions processed for static profile generation\n";
    if (FunctionsSkipped > 0) {
      errs() << "  " << FunctionsSkipped << " function(s) were skipped due to errors\n";
    }
    return PreservedAnalyses::all();
  }

  std::error_code EC;
  raw_fd_ostream Output(ProfilePath, EC, sys::fs::OF_None);
  if (EC) {
    errs() << "Error: Cannot open profile output file '" << ProfilePath
           << "': " << EC.message() << "\n";
    return PreservedAnalyses::all();
  }

  if (auto Err = Writer.write(Output)) {
    errs() << "Error: Failed to write profile data: " 
           << toString(std::move(Err)) << "\n";
    return PreservedAnalyses::all();
  }

  LLVM_DEBUG(dbgs() << "Successfully wrote static profile to '" << ProfilePath 
                    << "'\n");
  LLVM_DEBUG(dbgs() << "  Functions processed: " << FunctionsProcessed << "\n");
  LLVM_DEBUG(dbgs() << "  Functions skipped: " << FunctionsSkipped << "\n");

  return PreservedAnalyses::all();
}

} // namespace llvm
