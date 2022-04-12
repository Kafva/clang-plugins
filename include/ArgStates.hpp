#ifndef CLANG_TUTOR_ArgStates_H
#define CLANG_TUTOR_ArgStates_H

#include <unordered_set>
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"

#define DEBUG_AST true

#define PRINT_ERR(msg) llvm::errs()  << "\033[31m!>\033[0m " << msg << "\n"
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

using namespace clang;
using namespace ast_matchers;

//-----------------------------------------------------------------------------
// ASTFinder callback for both passes
//-----------------------------------------------------------------------------
//class ArgStatesMatcher : public MatchFinder::MatchCallback {
//public:
//  //explicit ArgStatesMatcher(Rewriter &RewriterForArgStates)
//  //    : ArgStatesRewriter(RewriterForArgStates)  {}
//  explicit ArgStatesMatcher() {}
//
//  void onEndOfTranslationUnit() override;
//  void run(const MatchFinder::MatchResult &) override;
//
//private:
//  //Rewriter ArgStatesRewriter;
//};



//-----------------------------------------------------------------------------
// First pass
//-----------------------------------------------------------------------------

class FirstPassMatcher : public MatchFinder::MatchCallback {
public:
  explicit FirstPassMatcher() {}
  void run(const MatchFinder::MatchResult &) override;
  void onEndOfTranslationUnit() override {};
};

class FirstPassASTConsumer : public ASTConsumer {
public:
  FirstPassASTConsumer(std::vector<std::string> Names);

  void HandleTranslationUnit(ASTContext &ctx) override {
    // --- First pass ---
    // In the first pass we will determine every call site to
    // a changed function and what arguments the invocations use
    this->Finder.matchAST(ctx);
  }
private:
  MatchFinder Finder;
  FirstPassMatcher MatchHandler;
  std::vector<std::string> Names;
};



//-----------------------------------------------------------------------------
// Second pass
//-----------------------------------------------------------------------------
class SecondPassMatcher : public MatchFinder::MatchCallback {
public:
  explicit SecondPassMatcher() {}
  void run(const MatchFinder::MatchResult &) override;
  void onEndOfTranslationUnit() override {};
};

class SecondPassASTConsumer : public ASTConsumer {
public:
  SecondPassASTConsumer(std::vector<std::string> Names);

  void HandleTranslationUnit(ASTContext &ctx) override {
    // --- Second pass ---
    // In the second pass we will consider all of the DECLREF arguments
    // found from the previous pass and determine their state space
    // before the function call occurs
    this->Finder.matchAST(ctx);
  }
private:
  MatchFinder Finder;
  SecondPassMatcher MatchHandler;
  std::vector<std::string> Names;
};






//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class ArgStatesASTConsumer : public ASTConsumer {
public:
  //ArgStatesASTConsumer(Rewriter &R, std::vector<std::string> Names);
  ArgStatesASTConsumer(std::vector<std::string> Names) {
  }

  void HandleTranslationUnit(ASTContext &ctx) override {
    // https://stackoverflow.com/questions/46723614/running-multiple-clang-passes
    
    auto firstPass = std::make_unique<FirstPassASTConsumer>(this->Names);
    firstPass->HandleTranslationUnit(ctx);
  
    auto secondPass = std::make_unique<SecondPassASTConsumer>(this->Names);
    secondPass->HandleTranslationUnit(ctx);

    //auto firstPass =  FirstPassASTConsumer(this->Names);
    //auto secondPass = SecondPassASTConsumer(this->Names);
    //consumers.push_back(&secondPass);
    //consumers.push_back(&firstPass);

    //for(auto consumer : consumers) {
    //  consumer->HandleTranslationUnit(ctx);
    //}

    // --- First pass ---
    // In the first pass we will determine every call site to
    // a changed function and what arguments the invocations use
    //this->FirstPassFinder.matchAST(ctx);
    
    // Set global flag to denote second pass!
    // Clear matchers on entry to the matcher
    //this->ArgStatesHandler.SecondPass = true;
    //PRINT_WARN("Starting...");

    // --- Second pass ---
    // In the second pass we will consider all of the DECLREF arguments
    // found from the previous pass and determine their state space
    // before the function call occurs
    //this->SecondPassFinder.matchAST(ctx);
  }

private:
  //std::vector<ASTConsumer*> consumers;

  // (Unused)
  //MatchFinder Finder;
  //ArgStatesMatcher ArgStatesHandler;
  std::vector<std::string> Names;
};

#endif
