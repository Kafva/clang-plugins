//==============================================================================
// DESCRIPTION: ArgStates
//
// USAGE: TBD
//==============================================================================
#include "ArgStates.hpp"

#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Tooling/Refactoring/Rename/RenamingAction.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>

using namespace clang;
using namespace ast_matchers;

// Indexed using the StateType enum to get the
// corresponding string for each enum
const char* LITERAL[] = {
  "CHR", "INT", "STR", "NONE"
};

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

template<typename T>
static void dumpMatch(std::string type, T msg, int pass,
    SourceManager* srcMgr, SourceLocation srcLocation) {
    #if DEBUG_AST
      const auto location = srcMgr->getFileLoc(srcLocation);
      llvm::errs() << "(\033[35m" << pass << "\033[0m) " << type << "> " 
        << location.printToString(*srcMgr)
        << " " << msg
        << "\n";
    #endif
    return;
}

static void getCallPath(ASTContext* ctx, BoundNodes::IDToNodeMap &nodeMap, 
  DynTypedNode &parent, std::string bindName, std::vector<DynTypedNode> &callPath){
    // Go up until we reach a call experssion
    callPath.push_back(parent); 

    // The ASTNodeKind returned from this method reflects the strings seen in
    //  clang -ast-dump
    if (parent.getNodeKind().asStringRef() == "CallExpr"){
      // There should be an enum for us to match agianst but I cannot find it...
      return;
    } else {
      auto parents = ctx->getParents(parent);
      if (parents.size()>0) {
        // We assume .getParents() only returns one entry
        auto newParent = parents[0];
        getCallPath(ctx, nodeMap, newParent, bindName, callPath);
      }
    }
}

static int getIndexOfParam(FirstPassMatcher& matcher, std::string paramName){
  auto statesVectorSize = matcher.functionStates.size();

  for (uint i = 0; i < statesVectorSize; i++){
    if (matcher.functionStates[i].ParamName == paramName){
      return int(i);
    }
  }

  return -1;
}

static std::string getParamName(const CallExpr* matchedCall, ASTContext* ctx, 
 BoundNodes::IDToNodeMap &nodeMap, std::vector<DynTypedNode>& callPath, 
 const char* bindName){
  std::string paramName = "";
  int  argumentIndex = -1;
  auto matchedNode = nodeMap.at(bindName);
  auto parents = ctx->getParents(matchedNode);
  
  if (parents.size()>0) {
    // We assume .getParents() only returns one entry
    auto parent = parents[0];
    // Move up recursivly until we reach a call expression
    // (we need to drop implicit casts etc.)
    // Since we save all of the nodes in the path we traverse
    // upwards, we can check which of the arguments our path corresponds to
    getCallPath(ctx, nodeMap, parent, bindName, callPath);

    // We use .push_back() so the last item will be the actual call,
    // we are intrested in the direct child from the call that is on the
    // path towards our match, which will be at the penultimate position, 
    // if the callPath only contains one item, then our matched node is 
    // a direct child of the callexpr
    auto ourPath = callPath.size() > 1 ? 
      callPath[callPath.size()-2] : 
      matchedNode; 

    auto ourExpr = ourPath.get<Expr>();

    argumentIndex = 0;
    for (auto call_arg : matchedCall->arguments()){
      // Break once we find an argument in the matched call expression
      // that matches the node we found traversing the AST upwards from
      // our match
      if (call_arg->getID(*ctx) == ourExpr->getID(*ctx) ) {
        break;
      }
      argumentIndex++;
    }

    // No match found if the index reaches NumArgs
    if (argumentIndex < int(matchedCall->getNumArgs()) ){
      // Fetch the name of the parameter from the function declaration
      const auto funcDecl   = matchedCall->getDirectCallee();
      const auto paramDecl  = funcDecl->getParamDecl(argumentIndex);
      paramName             = std::string(paramDecl->getName());
    } 
  }

  return paramName;
}

void handleLiteralMatch(FirstPassMatcher& matcher, ASTContext* ctx,
BoundNodes::IDToNodeMap& nodeMap,
std::variant<char,uint64_t,std::string> value,
StateType matchedType, const CallExpr* call
){
  // Determine which parameter this argument has been given to
  auto callPath = std::vector<DynTypedNode>();
  const auto paramName = getParamName(call, ctx, nodeMap, callPath, LITERAL[matchedType]); 
  
  auto paramIndex = getIndexOfParam(matcher, paramName);
  
  // Add a new ArgState entry for the function if one
  // does not exist already
  if (paramIndex == -1) {
    struct ArgState states = {
      .ParamName = paramName, 
      .ArgName = "",
      .States = std::set<std::variant<char,uint64_t,std::string>>()
    }; 
    states.Type = matchedType;

    matcher.functionStates.push_back(states);
    paramIndex = getIndexOfParam(matcher, paramName);
  }
  
  // If the callPath only contains 2 elements:
  //  <match>: [ implicitCast, callExpr ]
  // Then we have a 'clean' value and not something akin to 'x + 7'
  // If we have #define statements akin to
  //  #define XML_FALSE ((XML_Bool)0)
  //  We get:
  //
  //   `-ParenExpr 0x564907af8a08 <lib/expat.h:48:19, col:31> 'XML_Bool':'unsigned char'
  //     `-CStyleCastExpr 0x564907af89e0 <col:20, col:30> 'XML_Bool':'unsigned char' <IntegralCast>
  //       `-IntegerLiteral 0x564907af89b0 <col:30> 'int' 0
  //
  //  It would be opimtal if we could whitelist all patterns that are effectivly just NOOPs
  //  wrapping a literal
  // All other cases are considered nodet for now
  const auto alreadyNonDet = matcher.functionStates[paramIndex].IsNonDet;

  if (callPath.size() >= 2 && callPath[callPath.size()-2].getNodeKind().asStringRef() \
      == "ImplicitCastExpr"  && !alreadyNonDet) {
    // Insert the encountered state for the given param
    assert(paramIndex >= 0);
    matcher.functionStates[paramIndex].States.insert(value);

    PRINT_INFO(LITERAL[matchedType] << " param (det) " << paramName  << ": ");
  } else {
    // If a  parameter should be considered nondet, we will set a flag
    // for the argument object (preventing further uneccessary analysis,
    // if other calls are made with det values does not matter if at least
    // one call uses a nondet argument)
    matcher.functionStates[paramIndex].IsNonDet = true;
    PRINT_INFO(LITERAL[matchedType] << " param (nondet) " << paramName);
  }
}

//-----------------------------------------------------------------------------
// FirstPassASTConsumer- implementation
// FirstPassMatcher-     implementation
//-----------------------------------------------------------------------------

/// Specifies the node patterns that we want to analyze further in ::run()
FirstPassASTConsumer::
FirstPassASTConsumer(std::string symbolName): matchHandler() {
  // PRINT_WARN("First pass!");

  // We want to match agianst all variable refernces which are later passed
  // to one of the changed functions in the Names array
  //
  // As a starting point, we want to match the FunctionDecl nodes of the 
  // enclosing functions for any call to a changed function. From this node we 
  // can then continue downwards until we reach the actual call of the changed 
  // function, while recording all declared variables and saving the state of 
  // those which end up being used
  //
  // If we match the call experssions directly we would need to backtrack in the 
  // AST to find information on what each variable holds

  // The first child of a call expression is a declRefExpr to the 
  // function being invoked 
  //
  // Provided that we are not checking pointer params during the verification, 
  // we can actually make our lives easier by only matching function calls
  // were the return value is actually used for something
  //
  // A basic hack to detect if a return value goes unused would be to exclude
  // Nodes on the form
  //
  //  FUNCTION_DECL
  //    COMPOUND_STMT
  //      CALL_EXPR
  //
  //  Determining if the return value of a "nested" call is used would be a lot
  //  more complex: https://stackoverflow.com/a/56415042/9033629
  //
  //  FUNCTION_DECL
  //    COMPOUND_STMT
  //      <...>
  //        CALL_EXPR
  //
  // Testcase: XML_SetBase in xmlwf/xmlfile.c
  const auto isArgumentOfCall = hasAncestor(
      callExpr(callee(
          functionDecl(hasName(symbolName)
          ).bind("FNC")
          ),
      unless(hasParent(compoundStmt(hasParent(functionDecl()))))
  ).bind("CALL"));

  // Note that we exclude DeclRefExpr nodes which have a MemberExpr as an
  // ancenstor, e.g. arguments on the form 'dtd->pool'. These experssions
  // are matched seperatly as MemberExpr to retrieve '->pool' rather than 'dtd'
  const auto declRefMatcher = declRefExpr(to(
    declaratorDecl()), 
    unless(hasAncestor(memberExpr())),
    isArgumentOfCall
  ).bind("REF");


  // Note that we match the top level member if there are several indirections
  // i.e. we match 'c' in  'a->b->c'.
  // There are a lot of edge cases to consider for this, see 
  //  lib/xmlparse.c:3166 poolStoreString()
  //const auto memMatcher = memberExpr(
  //  unless(hasAncestor(memberExpr())),
  //  isArgumentOfCall
  //).bind("MEM");

  // We need the literals to be direct descendents (barring the implicitCastExpr)
  // otherwise they could be part of larger experssions, e.g. 'foo + 6'
  // For now, any literal that is not a direct descendent of the call will be
  // considered an undet() value
  const auto intMatcher = integerLiteral(isArgumentOfCall).bind(LITERAL[INT]);
  const auto stringMatcher = stringLiteral(isArgumentOfCall).bind(LITERAL[STR]);
  const auto charMatcher = characterLiteral(isArgumentOfCall).bind(LITERAL[CHR]);

  this->finder.addMatcher(declRefMatcher, &(this->matchHandler));
  this->finder.addMatcher(intMatcher,     &(this->matchHandler));
  this->finder.addMatcher(stringMatcher,  &(this->matchHandler));
  this->finder.addMatcher(charMatcher,    &(this->matchHandler));
}

void FirstPassMatcher::
run(const MatchFinder::MatchResult &result) {
  // The idea:
  // Determine what types of arguments are passed to the function
  // For literal and NULL arguments, we add their value to the state space
  // For declrefs, we save the names of each argument and query for all references
  // to them before the call (in the same enclosing function) in the next pass
  // The key cases we want to detect are
  //   1. When literals are passed
  //   2. When an unintialized (null) variable is passed
  //   3. When a variable is assigned a literal value (and remains unchanged)
  // We skip considering struct fields (MemberExpr) for now

  // Holds information on the actual source code
  const auto srcMgr = result.SourceManager;

  // Holds contxtual information about the AST, this allows
  // us to determine e.g. the parents of a matched node
  auto ctx = result.Context;

  auto nodeMap = result.Nodes.getMap();

  const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
  const auto *func       = result.Nodes.getNodeAs<FunctionDecl>("FNC");

  const auto *declRef    = result.Nodes.getNodeAs<DeclRefExpr>("REF");

  const auto *intLiteral = result.Nodes.getNodeAs<IntegerLiteral>(LITERAL[INT]);
  const auto *strLiteral = result.Nodes.getNodeAs<StringLiteral>(LITERAL[STR]);
  const auto *chrLiteral = result.Nodes.getNodeAs<CharacterLiteral>(LITERAL[CHR]);
  
  // To correlate the arguments that we match agianst to parameters in the function call
  // we need to traverse the call experssion and pair the arguments with the Parms from the FNC

  if (!func){
    PRINT_ERR("No FunctionDecl matched");
    return;
  }   
  
  if (call) {
    // Extract the filename (basename) of the current TU so that
    // the outer consumer knows what filename to use for the output file
    auto filepath = srcMgr->getFilename(call->getEndLoc()); 
    this->filename = filepath.substr(filepath.find_last_of("/\\") + 1);
  }

  if (declRef) {
    // This includes a match for the actual function token (index -1)
    const auto name = declRef->getDecl()->getName();
    dumpMatch("REF", name, 1, srcMgr, declRef->getEndLoc());
    
    // During the second pass we must be able to identify 
    //  * the enclosing function
    //  * the callee
    //  * the argument name
    //  for every reference that we encounter in the 1st pass
    
    auto callPath = std::vector<DynTypedNode>();
    auto paramName = getParamName(call, ctx, nodeMap, callPath, "REF"); 
    // PRINT_INFO("REF param arg " << paramName);
  }
  if (intLiteral) {
    const auto value =  intLiteral->getValue().getLimitedValue();
    dumpMatch(LITERAL[INT], value, 1, srcMgr, intLiteral->getLocation());
    handleLiteralMatch(*this, ctx, nodeMap, value, INT, call);
  }
  if (strLiteral) {
    const auto value =  std::string(strLiteral->getString());
    dumpMatch(LITERAL[STR], value, 1, srcMgr, strLiteral->getEndLoc());
    handleLiteralMatch(*this, ctx, nodeMap, value, STR, call);
  }
  if (chrLiteral) {
    const auto value =  chrLiteral->getValue();
    dumpMatch(LITERAL[CHR], value, 1, srcMgr, chrLiteral->getLocation());
    handleLiteralMatch(*this, ctx, nodeMap, value, CHR, call);
  }
}

//-----------------------------------------------------------------------------
// SecondPassASTConsumer- implementation
// SecondPassMatcher-     implementation
//-----------------------------------------------------------------------------

SecondPassASTConsumer::
SecondPassASTConsumer(std::string symbolName) : matchHandler() {
  // PRINT_WARN("Second pass!");

  const auto isArgumentOfCall = hasAncestor(
      callExpr(callee(
          functionDecl(hasName(symbolName))
            .bind("FNC")
          ),
      unless(hasParent(compoundStmt(hasParent(functionDecl()))))
  ).bind("CALL"));

  // Note that we exclude DeclRefExpr nodes which have a MemberExpr as an
  // ancenstor, e.g. arguments on the form 'dtd->pool'. These experssions
  // are matched seperatly as MemberExpr to retrieve '->pool' rather than 'dtd'
  const auto declRefMatcher = declRefExpr(to(
    declaratorDecl()), 
    unless(hasAncestor(memberExpr())),
    isArgumentOfCall
  ).bind("REF");


  this->finder.addMatcher(declRefMatcher, &(this->matchHandler));
}

void SecondPassMatcher::
run(const MatchFinder::MatchResult &result) {
    // Holds information on the actual sourc code
    const auto srcMgr = result.SourceManager;

    // Holds contxtual information about the AST, this allows
    // us to determine e.g. the parents of a matched node
    //const auto ctx = result.Context;

    //const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
    const auto *declRef    = result.Nodes.getNodeAs<DeclRefExpr>("REF");

    if (declRef) {
      const auto name = declRef->getDecl()->getName();
      dumpMatch("REF", name, 2, srcMgr, declRef->getEndLoc());
    }
}

//-----------------------------------------------------------------------------
// FrontendAction and Registration
//-----------------------------------------------------------------------------

class ArgStatesAddPluginAction : public PluginASTAction {
public:
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    DiagnosticsEngine &diagnostics = CI.getDiagnostics();
 
    uint namesDiagID = diagnostics.getCustomDiagID(
      DiagnosticsEngine::Error, "missing -symbol-name"
    );

    for (size_t i = 0, size = args.size(); i != size; ++i) {
      if (args[i] == "-symbol-name") {
         if (parseArg(diagnostics, namesDiagID, size, args, i)){
             this->symbolName = args[++i];
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
  // This is essentially our entrypoint
  //  https://clang.llvm.org/docs/RAVFrontendAction.html
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, 
  StringRef file) override {
    return std::make_unique<ArgStatesASTConsumer>(this->symbolName);
  }

private:
  bool parseArg(DiagnosticsEngine &diagnostics, uint diagID, int size,
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

  Rewriter RewriterForArgStates;
  std::string symbolName;
};

static FrontendPluginRegistry::Add<ArgStatesAddPluginAction>
    X(/*NamesFile=*/"ArgStates",
      /*Desc=*/"Enumerate the possible states for arguments to calls of the functions given in the -names-file argument.");
