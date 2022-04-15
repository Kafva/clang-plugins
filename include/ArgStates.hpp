#ifndef ArgStates_H
#define ArgStates_H
#include "Base.hpp"

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
// First pass:
// In the first pass we will determine every call site to
// a changed function and what arguments the invocations use
//-----------------------------------------------------------------------------
class FirstPassMatcher : public MatchFinder::MatchCallback {
public:
  explicit FirstPassMatcher() {}
  // Defines what types of nodes we we want to match
  void run(const MatchFinder::MatchResult &) override;
  void onEndOfTranslationUnit() override {};

  std::unordered_map<std::string,ArgState> argumentStates;
  std::string filename;
private:
  int getIndexOfParam(const CallExpr* call, std::string paramName);
  void getCallPath(DynTypedNode &parent, std::string bindName, 
    std::vector<DynTypedNode> &callPath);
  void handleLiteralMatch(std::variant<char,uint64_t,std::string> value,
    StateType matchedType, const CallExpr* call, const Expr* matchedExpr);
  std::string getParamName(const CallExpr* matchedCall, 
   std::vector<DynTypedNode>& callPath, 
   const char* bindName);

  SourceManager* srcMgr;
  BoundNodes::IDToNodeMap nodeMap;

  // Holds contxtual information about the AST, this allows
  // us to determine e.g. the parents of a matched node
  ASTContext* ctx;
};

class FirstPassASTConsumer : public ASTConsumer {
public:
  FirstPassASTConsumer(std::string symbolName);
  void HandleTranslationUnit(ASTContext &ctx) override ;

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
  
  std::unordered_map<std::string,ArgState> argumentStates;
};

class SecondPassASTConsumer : public ASTConsumer {
public:
  SecondPassASTConsumer(std::string symbolName);
  void HandleTranslationUnit(ASTContext &ctx) override ;

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
  ArgStatesASTConsumer(std::string symbolName) ;
  ~ArgStatesASTConsumer();
  void HandleTranslationUnit(ASTContext &ctx) override;

private:
  void dumpArgStates();
  std::string getOutputPath();
  std::string symbolName;
  std::string filename;
  std::unordered_map<std::string,ArgState> argumentStates;
};

#endif
