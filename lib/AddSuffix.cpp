//==============================================================================
// DESCRIPTION: AddSuffix
//
// USAGE:
//    1. As a loadable Clang plugin:
//      clang -cc1 -load <BUILD_DIR>/lib/libAddSuffix.dylib  -plugin  '\'
//      AddSuffix -plugin-arg-AddSuffix -class-name '\'
//      -plugin-arg-AddSuffix Base  -plugin-arg-AddSuffix -old-name '\'
//      -plugin-arg-AddSuffix run  -plugin-arg-AddSuffix -new-name '\'
//      -plugin-arg-AddSuffix walk test/AddSuffix_Class.cpp
//    2. As a standalone tool:
//       <BUILD_DIR>/bin/ct-code-refactor --class-name=Base --new-name=walk '\'
//        --old-name=run test/AddSuffix_Class.cpp
//
//==============================================================================
#include "AddSuffix.hpp"

#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Tooling/Refactoring/Rename/RenamingAction.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <fstream>

using namespace clang;
using namespace ast_matchers;

//-----------------------------------------------------------------------------
// AddSuffixMatcher - implementation
// Add the suffix to matched items
//-----------------------------------------------------------------------------

void AddSuffixMatcher::replaceInDeclRefMatch(
    const MatchFinder::MatchResult &result, 
    std::string bindName) {

    const DeclRefExpr *node = result.Nodes
      .getNodeAs<clang::DeclRefExpr>(bindName);

    if (node) {
      SourceRange srcRange = node->getExprLoc();
      auto newName = node->getDecl()->getName().str() + this->Suffix;

      this->AddSuffixRewriter.ReplaceText(srcRange, newName);
    }
}

void AddSuffixMatcher::replaceInCallMatch(
    const MatchFinder::MatchResult &result, 
    std::string bindName) {

    const CallExpr *node = result.Nodes.getNodeAs<clang::CallExpr>(bindName);

    if (node) {
      SourceRange srcRange = node->getExprLoc();
      auto newName = node->getDirectCallee()->getName().str() + this->Suffix;

      this->AddSuffixRewriter.ReplaceText(srcRange, newName);
    }
}

void AddSuffixMatcher::replaceInDeclMatch(
  const MatchFinder::MatchResult &result, 
  std::string bindName) {

    const DeclaratorDecl *node = result.Nodes
      .getNodeAs<clang::DeclaratorDecl>(bindName);

    if (node) {
      // .getLocation() applies for Decl:: classes
      SourceRange srcRange = node->getLocation();
      std::string newName = node->getName().str() + this->Suffix;

      this->AddSuffixRewriter.ReplaceText(srcRange, newName);
    }
}

void AddSuffixMatcher::run(const MatchFinder::MatchResult &result) {
  this->replaceInDeclMatch(result, "FunctionDecl");
  this->replaceInDeclMatch(result, "VarDecl");
  this->replaceInDeclRefMatch(result, "DeclRefExpr");
  this->replaceInCallMatch(result, "CallExpr");
}

void AddSuffixMatcher::onEndOfTranslationUnit() {
  // Output to stdout
  AddSuffixRewriter
      .getEditBuffer(AddSuffixRewriter.getSourceMgr().getMainFileID())
      .write(llvm::outs());
}

//-----------------------------------------------------------------------------
// AddSuffixASTConsumer- implementation
// https://clang.llvm.org/docs/LibASTMatchersTutorial.html
// Specifies the node patterns that we want to analyze further in ::run()
//-----------------------------------------------------------------------------

AddSuffixASTConsumer::AddSuffixASTConsumer(
    Rewriter &R, std::vector<std::string> Names, std::string Suffix)
    : AddSuffixHandler(R, Suffix), Names(Names), Suffix(Suffix) {
  // The matcher needs to know the number of arguments
  // it recieves at compile time so we haft to rely
  // on a handful of hacky macros to define expressions
  // were we match agianst 1, 10, 100 or 200 different names
  //
  // We can't (and shouldn't) create to many layers of macros for this
  //   template instantiation depth exceeds maximum of 900 (use ‘-ftemplate-depth=’ to increase the maximum)
  // and therefore stop at 200 names
  int namesLeft = (int)this->Names.size();
  int batchCnt;
  
  while (namesLeft > 0) {
    // We read the names from LAST to FIRST in the vector
    // 'namesLeft' is used as the top value for each iteration
   
    if ( (batchCnt = namesLeft / 200) >= 1 ) { /* > 199 names left */

      //for (int _ = 0; _ < batchCnt; _++) {
      //  addMatchers( anyOf(hasNames200(this->Names, namesLeft)) );
      //  namesLeft -= 200;
      //}

    } else if ( (batchCnt = namesLeft / 100) >= 1 ) { /* > 99 names left */
      
      //for (int _ = 0; _ < batchCnt; _++) {
      //  addMatchers( anyOf(hasNames100(this->Names, namesLeft)) );
      //  namesLeft -= 100;
      //}

    } else if ( (batchCnt = namesLeft / 10) >= 1 ) { /* > 9 names left */
      
      for (int _ = 0; _ < batchCnt; _++) {
	addMatchers( anyOf(hasNames10(this->Names, namesLeft)) );
	namesLeft -= 10;
      }

    } else { /* 1-9 names left */
      llvm::errs() << "\033[33m!>\033[0m Adding suffix onto " << 
	Names[namesLeft - 1 ] << 
	  " ("<<  namesLeft << " to go)\n";

        // Note that we will not decrement correctly if 
	// we do it inside of a macro
	namesLeft--;
	addMatchers( hasName(Names[namesLeft]) );
    }
  }
}

//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class AddSuffixAddPluginAction : public PluginASTAction {
public:
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {

    DiagnosticsEngine &diagnostics = CI.getDiagnostics();
    
    unsigned namesDiagID = diagnostics.getCustomDiagID(
	DiagnosticsEngine::Error, "missing -names-file"
    );
    unsigned suffixDiagID = diagnostics.getCustomDiagID(
	DiagnosticsEngine::Error, "missing -suffix"
    );

    for (size_t i = 0, size = args.size(); i != size; ++i) {
      llvm::errs() << "AddSuffix arg = " << args[i] << "\n";

      if (args[i] == "-names-file") {
          if (parseArg(diagnostics, namesDiagID, size, args, i)){
                auto NamesFile = args[++i];
		this->readNamesFromFile(NamesFile);
	  } else {
                return false;
	  }
      }
      else if (args[i] == "-suffix") {
          if (parseArg(diagnostics, suffixDiagID, size, args, i)){
                this->Suffix = args[++i];
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
  std::unique_ptr<ASTConsumer> 
    CreateASTConsumer(CompilerInstance &CI, StringRef file) override {

    RewriterForAddSuffix.setSourceMgr(CI.getSourceManager(),
				      CI.getLangOpts());
    return std::make_unique<AddSuffixASTConsumer>(
	RewriterForAddSuffix, this->Names, this->Suffix);
  }

private:
  void readNamesFromFile(std::string filename) {
    std::ifstream file(filename);

    if (file.is_open()) {
      std::string line;

      while (std::getline(file,line)) {
	this->Names.push_back(line);
      }

      file.close();
    }
  }

  bool parseArg(DiagnosticsEngine &diagnostics, unsigned diagID, int size, 
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

  Rewriter RewriterForAddSuffix;
  std::vector<std::string> Names;
  std::string Suffix;
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static FrontendPluginRegistry::Add<AddSuffixAddPluginAction>
    X(/*NamesFile=*/"AddSuffix",
      /*Desc=*/"Add a suffix to a global symbol");
