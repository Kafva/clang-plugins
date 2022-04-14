#ifndef ArgStates_H
#define ArgStates_H

#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"

#include "Base.hpp"

using namespace clang;
using namespace ast_matchers;
  
// The plugin receives ONE global symbol as input
// Since we only need to look at changed entities (and not all
// symbols as with clang-suffix) the overhead of doing a new
// run per name is not going to be notable a problem
//
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
//    }
//  }
//
//  The params which are only used with finite values as arguments can
//  be restriced during harness generation
//
//  https://clang.llvm.org/docs/LibASTMatchersTutorial.html
//

//-----------------------------------------------------------------------------
// Argument state structures
// We will need a seperate struct for passing values to the second pass
//-----------------------------------------------------------------------------


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

  std::vector<ArgState> functionStates;
  std::string filename;

};

class FirstPassASTConsumer : public ASTConsumer {
public:
  FirstPassASTConsumer(std::string symbolName);

  void HandleTranslationUnit(ASTContext &ctx) override {
    this->finder.matchAST(ctx);
  }

  friend int getIndexOfParam(FirstPassMatcher& matcher, 
      std::string funcName, std::string paramName);

  friend void handleLiteralMatch(FirstPassMatcher& matcher, const ASTContext* ctx,
      BoundNodes::IDToNodeMap& nodeMap,
      std::variant<char,uint64_t,std::string> value, std::string funcName,
      std::string bindName, CallExpr* call
  );
  
  FirstPassMatcher matchHandler;

private:
  MatchFinder finder;
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
  
  std::vector<ArgState> functionStates;
};

class SecondPassASTConsumer : public ASTConsumer {
public:
  SecondPassASTConsumer(std::string symbolName);

  void HandleTranslationUnit(ASTContext &ctx) override {
    this->finder.matchAST(ctx);
  }

  SecondPassMatcher matchHandler;

private:
  MatchFinder finder;
};

//-----------------------------------------------------------------------------
// ASTConsumer driver for each pass
//  https://stackoverflow.com/a/46738273/9033629
//-----------------------------------------------------------------------------
class ArgStatesASTConsumer : public ASTConsumer {
public:
  ArgStatesASTConsumer(std::string symbolName) {
    this->symbolName = symbolName;
  }
  ~ArgStatesASTConsumer(){
    this->dumpArgStates();

  }

  void HandleTranslationUnit(ASTContext &ctx) override {
    
    auto firstPass = std::make_unique<FirstPassASTConsumer>(this->symbolName);
    firstPass->HandleTranslationUnit(ctx);

    // The TU name is most easily read from within the match handler
    this->filename = firstPass->matchHandler.filename;

    auto secondPass = std::make_unique<SecondPassASTConsumer>(this->symbolName);

    // Copy over the function states
    // Note that the first pass only adds literals and the second adds declrefs
    secondPass->matchHandler.functionStates = firstPass->matchHandler.functionStates;
    // secondPass->HandleTranslationUnit(ctx);

    // Overwrite the states
    this->functionStates = secondPass->matchHandler.functionStates;
  }

private:
  void dumpArgStates();
  std::string getOutputPath();
  std::string symbolName;
  std::string filename;
  std::vector<ArgState> functionStates;
};




#endif
