#ifndef CLANG_TUTOR_ArgStates_H
#define CLANG_TUTOR_ArgStates_H

#include <unordered_set>
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"

#define DEBUG_AST true

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
// ASTFinder callback
//-----------------------------------------------------------------------------
class ArgStatesMatcher
    : public MatchFinder::MatchCallback {
public:
  explicit ArgStatesMatcher(Rewriter &RewriterForArgStates)
      : ArgStatesRewriter(RewriterForArgStates)  {}

  void onEndOfTranslationUnit() override;

  void run(const MatchFinder::MatchResult &) override;

private:
  // To avoid renaming the same token several times
  // we maintain a set of all locations which have been modified
  std::unordered_set<std::string> renamedLocations = 
	  std::unordered_set<std::string>({});

  Rewriter ArgStatesRewriter;
  // NOTE: The matcher already knows *what* name to search for 
  // because it _matched_ an expression that corresponds to
  // the command line arguments.
};

//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class ArgStatesASTConsumer : public ASTConsumer {
public:
  ArgStatesASTConsumer(Rewriter &R, std::vector<std::string> Names);

  void HandleTranslationUnit(ASTContext &Ctx) override {
    Finder.matchAST(Ctx);
  }

private:
  MatchFinder Finder;
  ArgStatesMatcher ArgStatesHandler;
  std::vector<std::string> Names;
};

#endif
