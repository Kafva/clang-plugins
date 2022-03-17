#ifndef CLANG_TUTOR_AddSuffix_H
#define CLANG_TUTOR_AddSuffix_H

#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"

//-----------------------------------------------------------------------------
// ASTFinder callback
//-----------------------------------------------------------------------------
class AddSuffixMatcher
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  explicit AddSuffixMatcher(clang::Rewriter &RewriterForAddSuffix, 
      std::string Suffix)
      : AddSuffixRewriter(RewriterForAddSuffix), Suffix(Suffix) {}

  void onEndOfTranslationUnit() override;

  void run(const clang::ast_matchers::MatchFinder::MatchResult &) override;

private:
  clang::Rewriter AddSuffixRewriter;
  // NOTE: This matcher already knows *what* name to search for 
  // because it _matched_ an expression that corresponds to
  // the command line arguments.
  std::string Suffix;
};

//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class AddSuffixASTConsumer : public clang::ASTConsumer {
public:
  AddSuffixASTConsumer(clang::Rewriter &R, 
      std::string Name, std::string Suffix
  );

  void HandleTranslationUnit(clang::ASTContext &Ctx) override {
    Finder.matchAST(Ctx);
  }

private:
  clang::ast_matchers::MatchFinder Finder;
  AddSuffixMatcher AddSuffixHandler;
  std::string Name;
  std::string Suffix;
};

#endif
