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

using namespace clang;
using namespace ast_matchers;

//-----------------------------------------------------------------------------
// AddSuffixMatcher - implementation
//-----------------------------------------------------------------------------
void AddSuffixMatcher::run(const MatchFinder::MatchResult &Result) {
  const MemberExpr *MemberAccess =
      Result.Nodes.getNodeAs<clang::MemberExpr>("MemberAccess");

  if (MemberAccess) {
    SourceRange CallExprSrcRange = MemberAccess->getMemberLoc();
    AddSuffixRewriter.ReplaceText(CallExprSrcRange, Suffix);
  }

  const NamedDecl *MemberDecl =
      Result.Nodes.getNodeAs<clang::NamedDecl>("MemberDecl");

  if (MemberDecl) {
    SourceRange MemberDeclSrcRange = MemberDecl->getLocation();
    AddSuffixRewriter.ReplaceText(
        CharSourceRange::getTokenRange(MemberDeclSrcRange), Suffix);
  }
}

void AddSuffixMatcher::onEndOfTranslationUnit() {
  // Output to stdout
  AddSuffixRewriter
      .getEditBuffer(AddSuffixRewriter.getSourceMgr().getMainFileID())
      .write(llvm::outs());
}

//-----------------------------------------------------------------------------
// AddSuffixASTConsumer- implementation
//-----------------------------------------------------------------------------
AddSuffixASTConsumer::AddSuffixASTConsumer(Rewriter &R, std::string Name, std::string Suffix)
    : AddSuffixHandler(R, Suffix), Name(Name), Suffix(Suffix) {

  const auto MatcherForMemberAccess = cxxMemberCallExpr(
      callee(memberExpr(member(hasName(Name))).bind("MemberAccess")),
      thisPointerType(cxxRecordDecl(isSameOrDerivedFrom(hasName(Name)))));

  Finder.addMatcher(MatcherForMemberAccess, &AddSuffixHandler);

  const auto MatcherForMemberDecl = cxxRecordDecl(
      allOf(isSameOrDerivedFrom(hasName("None")),
            hasMethod(decl(namedDecl(hasName(Name))).bind("MemberDecl"))));

  Finder.addMatcher(MatcherForMemberDecl, &AddSuffixHandler);
}

//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class AddSuffixAddPluginAction : public PluginASTAction {
public:
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &Args) override {
    // Example error handling.
    DiagnosticsEngine &D = CI.getDiagnostics();
    for (size_t I = 0, E = Args.size(); I != E; ++I) {
      llvm::errs() << "AddSuffix arg = " << Args[I] << "\n";

      if (Args[I] == "-name") {
        if (I + 1 >= E) {
          D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
                                     "missing -name argument"));
          return false;
        }
        ++I;
        Name = Args[I];
      } else if (Args[I] == "-suffix") {
        if (I + 1 >= E) {
          D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
                                     "missing -suffix"));
          return false;
        }
        ++I;
        Suffix = Args[I];
      }
      if (!Args.empty() && Args[0] == "help")
        PrintHelp(llvm::errs());
    }


    if (Suffix.empty()) {
      D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
                                 "missing -suffix argument"));
      return false;
    }
    if (Name.empty()) {
      D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
                                 "missing -name argument"));
      return false;
    }

    return true;
  }
  static void PrintHelp(llvm::raw_ostream &ros) {
    ros << "Help for AddSuffix plugin goes here\n";
  }

  // Returns our ASTConsumer per translation unit.
  std::unique_ptr<ASTConsumer> 
    CreateASTConsumer(CompilerInstance &CI, StringRef file) override {

    RewriterForAddSuffix.setSourceMgr(CI.getSourceManager(),
				      CI.getLangOpts());
    return std::make_unique<AddSuffixASTConsumer>(
	RewriterForAddSuffix, Name, Suffix);
  }

private:
  Rewriter RewriterForAddSuffix;
  std::string Name;
  std::string Suffix;
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static FrontendPluginRegistry::Add<AddSuffixAddPluginAction>
    X(/*Name=*/"AddSuffix",
      /*Desc=*/"Add a suffix to a global symbol");
