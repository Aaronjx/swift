//===-- SILOpt.cpp - SIL Optimization Driver ------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This is the entry point.
//
//===----------------------------------------------------------------------===//

#include "swift/Subsystems.h"
#include "swift/Frontend/DiagnosticVerifier.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"
#include "swift/SILPasses/Passes.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Signals.h"
using namespace swift;

static llvm::cl::opt<std::string>
InputFilename(llvm::cl::desc("input file"), llvm::cl::init("-"),
              llvm::cl::Positional);

static llvm::cl::opt<std::string>
OutputFilename("o", llvm::cl::desc("output filename"), llvm::cl::init("-"));


static llvm::cl::opt<bool>
EmitVerboseSIL("emit-verbose-sil",
               llvm::cl::desc("Emit locations during sil emission."));

static llvm::cl::opt<std::string>
FunctionName("func", llvm::cl::desc("Function name to extract."));

static llvm::cl::list<std::string>
ImportPaths("I", llvm::cl::desc("add a directory to the import search path"));

// This function isn't referenced outside its translation unit, but it
// can't use the "static" keyword because its address is used for
// getMainExecutable (since some platforms don't support taking the
// address of main, and some platforms can't implement getMainExecutable
// without being given the address of a function in the main executable).
void anchorForGetMainExecutable() {}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc, argv);

  llvm::cl::ParseCommandLineOptions(argc, argv, "Swift SIL Extractor\n");

  // Call llvm_shutdown() on exit to print stats and free memory.
  llvm::llvm_shutdown_obj Y;

  CompilerInvocation Invocation;

  Invocation.setMainExecutablePath(
      llvm::sys::fs::getMainExecutable(argv[0],
          reinterpret_cast<void *>(&anchorForGetMainExecutable)));

  // Give the context the list of search paths to use for modules.
  Invocation.setImportSearchPaths(ImportPaths);

  Invocation.setModuleName("main");
  Invocation.setInputKind(SourceFileKind::SIL);

  Invocation.addInputFilename(InputFilename);

  CompilerInstance CI;
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);

  if (CI.setup(Invocation))
    return 1;
  CI.performParse();

  // If parsing produced an error, don't run any passes.
  if (CI.getASTContext().hadError())
    return 1;

  assert(CI.hasSILModule() && "CI must have a sil module to extract from.\n");

  SILModule *M = CI.getSILModule();
  SILModule::FunctionListType &Functions = M->getFunctionList();
  for (auto FI = Functions.begin(), FE = Functions.end(); FI != FE;) {
    SILModule::FunctionListType::iterator Iter = FI++;
    if (FunctionName != Iter->getName().str())
      Functions.remove(Iter);
  }

  std::string ErrorInfo;
  llvm::raw_fd_ostream OS(OutputFilename.c_str(), ErrorInfo);
  if (!ErrorInfo.empty()) {
    llvm::errs() << "while opening '" << OutputFilename << "': "
                 << ErrorInfo << '\n';
    return 1;
  }
  CI.getSILModule()->print(OS, EmitVerboseSIL);

  return CI.getASTContext().hadError();
}
