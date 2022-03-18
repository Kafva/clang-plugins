#ifndef CLANG_TUTOR_AddSuffix_H
#define CLANG_TUTOR_AddSuffix_H

#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"


#define hasNames10(arr,end) hasName(arr[end]), hasName(arr[end-1]), \
  hasName(arr[end-2]), hasName(arr[end-3]), hasName(arr[end-4]), \
  hasName(arr[end-5]), hasName(arr[end-6]), hasName(arr[end-7]), \
  hasName(arr[end-8]), hasName(arr[end-9]) 

#define hasNames100(arr,end) hasNames10(arr,end), \
  hasNames10(arr,end - 10*1), hasNames10(arr,end - 10*2), \
  hasNames10(arr,end - 10*3), hasNames10(arr,end - 10*4), \
  hasNames10(arr,end - 10*5), hasNames10(arr,end - 10*6), \
  hasNames10(arr,end - 10*7), hasNames10(arr,end - 10*8), \
  hasNames10(arr,end - 10*9) 

#define hasNames200(arr,end) hasNames100(arr,end), \
  hasNames100(arr,end - 100*1)

// We cannot define this as a regular class method since the prototype for the 
// 'hasNames' function depends on how many elements lie within the object
//
// Match any: 
//  - Function declerations
//  - Function calls
//  - Variable declerations
//  - References to variable declerations
//  that have 'anyOf' the provided names
//
// Adding several matchers with the same .bind() string does 
// not cause problems
#define addMatchers(hasNames) do { \
    const auto matcherForFunctionDecl = functionDecl(hasNames) \
					  .bind("FunctionDecl"); \
 \
    const auto matcherForFunctionCall = callExpr(callee( \
					  functionDecl(hasNames))) \
					  .bind("CallExpr"); \
 \
    const auto matcherForVarDecl = varDecl(hasNames) \
					  .bind("VarDecl"); \
 \
    const auto matcherForDeclRefExpr = declRefExpr(to(varDecl( \
					  hasNames))) \
					  .bind("DeclRefExpr"); \
 \
 \
    Finder.addMatcher(matcherForFunctionDecl, &(this->AddSuffixHandler)); \
    Finder.addMatcher(matcherForVarDecl,      &(this->AddSuffixHandler)); \
    Finder.addMatcher(matcherForFunctionCall, &(this->AddSuffixHandler)); \
    Finder.addMatcher(matcherForDeclRefExpr,  &(this->AddSuffixHandler)); \
 \
} while (0)


using namespace clang;
using namespace ast_matchers;

//-----------------------------------------------------------------------------
// ASTFinder callback
//-----------------------------------------------------------------------------
class AddSuffixMatcher
    : public MatchFinder::MatchCallback {
public:
  explicit AddSuffixMatcher(Rewriter &RewriterForAddSuffix, 
      std::string Suffix)
      : AddSuffixRewriter(RewriterForAddSuffix), Suffix(Suffix) {}

  void onEndOfTranslationUnit() override;

  void run(const MatchFinder::MatchResult &) override;

private:
  void replaceInDeclRefMatch(
    const MatchFinder::MatchResult &result, 
    std::string bindName);
  void replaceInCallMatch(
      const MatchFinder::MatchResult &result, 
      std::string bindName);

  void replaceInDeclMatch(
    const MatchFinder::MatchResult &result, 
    std::string bindName);

  Rewriter AddSuffixRewriter;
  // NOTE: This matcher already knows *what* name to search for 
  // because it _matched_ an expression that corresponds to
  // the command line arguments.
  std::string Suffix;
};

//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class AddSuffixASTConsumer : public ASTConsumer {
public:
  AddSuffixASTConsumer(Rewriter &R, 
      std::vector<std::string> Names, std::string Suffix
  );

  void HandleTranslationUnit(ASTContext &Ctx) override {
    Finder.matchAST(Ctx);
  }

private:
  MatchFinder Finder;
  AddSuffixMatcher AddSuffixHandler;
  std::vector<std::string> Names;
  std::string Suffix;
};

#endif
