#ifndef CLANG_TUTOR_AddSuffix_H
#define CLANG_TUTOR_AddSuffix_H

#include <unordered_set>
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"

#define DEBUG_AST false

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
//  - Function references (this includes function calls())
//  - Variable declerations
//  - Variable references
//  that have 'anyOf' the provided (global) names
//
// Adding several matchers with the same .bind() string does 
// not cause problems
#define addMatchers(hasNames) do { \
    const auto matcherForFunctionDecl = functionDecl(hasNames) \
					  .bind("FunctionDecl"); \
 \
    const auto matcherForVarDecl = varDecl(hasNames) \
					  .bind("VarDecl"); \
 \
    const auto matcherForRefExpr = declRefExpr(to(declaratorDecl( \
					  hasNames))) \
					  .bind("DeclRefExpr"); \
 \
 \
    Finder.addMatcher(matcherForFunctionDecl, &(this->AddSuffixHandler)); \
    Finder.addMatcher(matcherForVarDecl,      &(this->AddSuffixHandler)); \
    Finder.addMatcher(matcherForRefExpr,      &(this->AddSuffixHandler)); \
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
      : AddSuffixRewriter(RewriterForAddSuffix), Suffix(Suffix)  {}

  void onEndOfTranslationUnit() override;

  void run(const MatchFinder::MatchResult &) override;

private:
  void replaceInDeclRefMatch(
    const MatchFinder::MatchResult &result, 
    std::string bindName);
  void replaceInDeclMatch(
    const MatchFinder::MatchResult &result, 
    std::string bindName);
  void replaceInMatch(
    const MatchFinder::MatchResult &result, std::string bindName,
    SourceRange srcRange, std::string nodeName);

  // To avoid renaming the same token several times
  // we maintain a set of all locations which have been modified
  std::unordered_set<std::string> renamedLocations = 
	  std::unordered_set<std::string>({});

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
