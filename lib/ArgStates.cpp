//==============================================================================
// DESCRIPTION: ArgStates
//
// USAGE: TBD
//==============================================================================

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Tooling/CommonOptionsParser.h"

#include "ArgStates.hpp"
//-----------------------------------------------------------------------------
// ArgStatesASTConsumer: Outer wrapper
//-----------------------------------------------------------------------------
ArgStatesASTConsumer::ArgStatesASTConsumer(std::string symbolName) {
  this->symbolName = symbolName;
}

ArgStatesASTConsumer::~ArgStatesASTConsumer(){
  this->dumpArgStates();
}

void ArgStatesASTConsumer::HandleTranslationUnit(ASTContext &ctx) {
    auto firstPass = std::make_unique<FirstPassASTConsumer>(this->symbolName);
    firstPass->HandleTranslationUnit(ctx);

    // The TU name is most easily read from within the match handler
    this->filename = firstPass->matchHandler.filename;

    auto secondPass = std::make_unique<SecondPassASTConsumer>(this->symbolName);

    // Copy over the function states
    // Note that the first pass only adds literals and the second adds declrefs
    secondPass->matchHandler.functionStates = firstPass->matchHandler.functionStates;
    //secondPass->HandleTranslationUnit(ctx);

    // Overwrite the states
    this->functionStates = secondPass->matchHandler.functionStates;
}

//-----------------------------------------------------------------------------
// FrontendAction and Registration
//-----------------------------------------------------------------------------

class ArgStatesAddPluginAction : public PluginASTAction {
public:
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {

    srand(time(NULL));
    DiagnosticsEngine &diagnostics = CI.getDiagnostics();
 
    uint namesDiagID = diagnostics.getCustomDiagID(
      DiagnosticsEngine::Error, "missing -symbol-name"
    );

    for (size_t i = 0, size = args.size(); i != size; ++i) {
      if (args[i] == "-symbol-name") {
         if (parseArg(diagnostics, namesDiagID, size, args, i)){
             this->symbolName = args[++i];
         } else {
             return false;
         }
      }
      if (!args.empty() && args[0] == "help") {
        llvm::errs() << "No help available";
      }
    }

    return true;
  }

  // Returns our ASTConsumer per translation unit.
  // This is essentially our entrypoint
  //  https://clang.llvm.org/docs/RAVFrontendAction.html
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, 
  StringRef file) override {
    return std::make_unique<ArgStatesASTConsumer>(this->symbolName);
  }

private:
  bool parseArg(DiagnosticsEngine &diagnostics, uint diagID, int size,
      const std::vector<std::string> &args, int i) {

      if (i + 1 >= size) {
        diagnostics.Report(diagID);
        return false;
      }
      if (args[i+1].empty()) {
        diagnostics.Report(diagID);
        return false;
      }
      return true;
  }

  std::string symbolName;
};

static FrontendPluginRegistry::Add<ArgStatesAddPluginAction>
    X(/*NamesFile=*/"ArgStates",
      /*Desc=*/"Enumerate the possible states for arguments to calls of the functions given in the -names-file argument.");
