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
  

  //std::array<internal::Matcher<NamedDecl>,100> nameMatcher{};
  //internal::Matcher<NamedDecl> nameMatcher[100];

  std::vector<internal::Matcher<NamedDecl>> nameMatcher;
  for (auto name : Names){
    nameMatcher.push_back(hasName(name));
  }

  // The matcher needs to know the number of arguments
  // it recieves at compile time so we haft to rely
  // on a handful of hacky macros to define expressions
  // were we match agianst 1, 10, 100 or 1000 different names
  // if there are more than a thousand names we will iterate, using the
  // most appropriate version, binding() to new names
  //
  // Or... Create 4 * 10 000 seperate matchers, that must have very bad performance...

  // Match any: 
  //  - Function declerations
  //  - Function calls
  //  - Variable declerations
  //  - References to variable declerations
  //  that have 'anyOf' the provided names
  const auto matcherForFunctionDecl = functionDecl(
				      anyOf( 
					  hasName(Names[0]), 
					  hasName(Names[1]) 
					))
	  				.bind("FunctionDecl");
  const auto matcherForFunctionCall = callExpr(callee(
	                                functionDecl(
				        anyOf( 
					  hasName(Names[0]), 
					  hasName(Names[1])
					))))
				        .bind("CallExpr");

  const auto matcherForVarDecl = varDecl(
				      anyOf( 
					  hasName(Names[0]), 
					  hasName(Names[1]) 
					))
	  				.bind("VarDecl");

  const auto matcherForDeclRefExpr = declRefExpr(to(varDecl(
				        anyOf( 
					  hasName(Names[0]), 
					  hasName(Names[1]) 
					))))
	  				.bind("DeclRefExpr");


  Finder.addMatcher(matcherForFunctionDecl, &AddSuffixHandler);
  Finder.addMatcher(matcherForVarDecl,      &AddSuffixHandler);
  Finder.addMatcher(matcherForFunctionCall, &AddSuffixHandler);
  Finder.addMatcher(matcherForDeclRefExpr,  &AddSuffixHandler);
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
