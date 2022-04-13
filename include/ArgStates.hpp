#ifndef CLANG_TUTOR_ArgStates_H
#define CLANG_TUTOR_ArgStates_H

#include <unordered_set>
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"

#define DEBUG_AST true

#define PRINT_ERR(msg)  llvm::errs() << "\033[31m!>\033[0m " << msg << "\n"
#define PRINT_WARN(msg) llvm::errs() << "\033[33m!>\033[0m " << msg << "\n"
#define PRINT_INFO(msg) llvm::errs() << "\033[34m!>\033[0m " << msg << "\n"

// The plugin receives a list of global symbols as input.
// We want to determine what arguments are used to call each of these
// functions. Our record of this data will be on the form
//
//  {
//    "XML_ExternalEntityParserCreate": {
//        "param1": [
//          "0", "1"
//        ],
//        "param2": [
//          "getchar()", "0"
//        ]
//      }
//  }
//
//  The params which are only used with finite values as arguments can
//  be restriced during harness generation
//
//  https://clang.llvm.org/docs/LibASTMatchersTutorial.html


using namespace clang;
using namespace ast_matchers;

//-----------------------------------------------------------------------------
// Argument state structures
//-----------------------------------------------------------------------------
struct ArgState {
  // The ArgName will be empty for literals
  std::string ParamName;
  std::string ArgName;
};

struct ChrArgState : public ArgState {
  std::vector<char> States;
};
struct IntArgState : public ArgState {
  std::vector<int> States;
};
struct StrArgState : public ArgState {
  std::vector<std::string> States;
};

struct FunctionState {
  std::string Spelling;
  std::vector<ArgState> Arguments;
};

//-----------------------------------------------------------------------------
// First pass:
// In the first pass we will determine every call site to
// a changed function and what arguments the invocations use
//-----------------------------------------------------------------------------

/// Callback matcher for the consumer
/// The ::run() method defines what types of nodes we
/// we want to match
class FirstPassMatcher : public MatchFinder::MatchCallback {
public:
  explicit FirstPassMatcher() {}
  void run(const MatchFinder::MatchResult &) override;
  void onEndOfTranslationUnit() override {};

  std::vector<FunctionState> FunctionStates;
};

class FirstPassASTConsumer : public ASTConsumer {
public:
  FirstPassASTConsumer(std::vector<std::string> Names);

  void HandleTranslationUnit(ASTContext &ctx) override {
    this->Finder.matchAST(ctx);
  }
  
  FirstPassMatcher MatchHandler;

private:
  MatchFinder Finder;
  std::vector<std::string> Names;
};


//-----------------------------------------------------------------------------
// Second pass:
// In the second pass we will consider all of the DECLREF arguments
// found from the previous pass and determine their state space
// before the function call occurs
//-----------------------------------------------------------------------------
class SecondPassMatcher : public MatchFinder::MatchCallback {
public:
  explicit SecondPassMatcher() {}
  void run(const MatchFinder::MatchResult &) override;
  void onEndOfTranslationUnit() override {};

  std::vector<FunctionState> FunctionStates;
};

class SecondPassASTConsumer : public ASTConsumer {
public:
  SecondPassASTConsumer(std::vector<std::string> Names);

  void HandleTranslationUnit(ASTContext &ctx) override {
    this->Finder.matchAST(ctx);
  }

  SecondPassMatcher MatchHandler;

private:
  MatchFinder Finder;
  std::vector<std::string> Names;
};

//-----------------------------------------------------------------------------
// ASTConsumer driver for each pass
//  https://stackoverflow.com/a/46738273/9033629
//-----------------------------------------------------------------------------
class ArgStatesASTConsumer : public ASTConsumer {
public:
  ArgStatesASTConsumer(std::vector<std::string> Names) {}

  void HandleTranslationUnit(ASTContext &ctx) override {
    
    auto firstPass = std::make_unique<FirstPassASTConsumer>(this->Names);
    firstPass->HandleTranslationUnit(ctx);

    auto secondPass = std::make_unique<SecondPassASTConsumer>(this->Names);

    secondPass->MatchHandler.FunctionStates = firstPass->MatchHandler.FunctionStates;

    secondPass->HandleTranslationUnit(ctx);

    PRINT_WARN("Time to write states");
  }

private:
  std::vector<std::string> Names;
};

#endif
