#include "ArgStates.hpp"
#include "Util.hpp"

//#include "clang/AST/Expr.h"
//#include "clang/AST/ExprCXX.h"
//#include "clang/AST/RecursiveASTVisitor.h"
//#include "clang/ASTMatchers/ASTMatchers.h"
//#include "clang/Frontend/CompilerInstance.h"
//#include "clang/Frontend/FrontendPluginRegistry.h"
//#include "clang/Tooling/Refactoring/Rename/RenamingAction.h"
//#include "llvm/Support/raw_ostream.h"
//#include <fstream>
//#include <iostream>

// Indexed using the StateType enum to get the
// corresponding string for each enum
const char* LITERAL[] = {
  "CHR", "INT", "STR", "NONE"
};


const std::map<std::string,StateType> NodeTypes {
  {"CharacterLiteral", CHR},
  {"IntegerLiteral", INT},
  {"StringLiteral", STR},
  {"DeclRefExpr", NONE}
};

int FirstPassMatcher::getIndexOfParam(const CallExpr* call, std::string paramName){
  const auto fnc = call->getDirectCallee();

  for (uint i = 0; i < fnc->getNumParams(); i++){
      if (fnc->getParamDecl(i)->getName() == paramName){
        return int(i);
      }
  }
  return -1;
}

void FirstPassMatcher::getCallPath(DynTypedNode &parent, 
 std::string bindName, std::vector<DynTypedNode> &callPath){
    // Go up until we reach a call experssion
    callPath.push_back(parent); 

    // The ASTNodeKind returned from this method reflects the strings seen in
    //  clang -ast-dump
    if (parent.getNodeKind().asStringRef() == "CallExpr"){
      // There should be an enum for us to match agianst but I cannot find it...
      return;
    } else {
      auto parents = this->ctx->getParents(parent);
      if (parents.size()>0) {
        // We assume .getParents() only returns one entry
        auto newParent = parents[0];
        getCallPath(newParent, bindName, callPath);
      }
    }
}


std::string FirstPassMatcher::getParamName(const CallExpr* matchedCall, 
 std::vector<DynTypedNode>& callPath, 
 const char* bindName){
  std::string paramName = "";
  int  argumentIndex = -1;
  auto matchedNode = this->nodeMap.at(bindName);
  auto parents = this->ctx->getParents(matchedNode);
  
  if (parents.size()>0) {
    // We assume .getParents() only returns one entry
    auto parent = parents[0];
    // Move up recursivly until we reach a call expression
    // (we need to drop implicit casts etc.)
    // Since we save all of the nodes in the path we traverse
    // upwards, we can check which of the arguments our path corresponds to
    getCallPath(parent, bindName, callPath);

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

    if ( ourExpr->getID(*ctx) == matchedCall->getCallee()->getID(*ctx) ){
      // Check if our expression actually corresponds to the functionDecl node at index '-1'
      paramName = std::string(matchedCall->getDirectCallee()->getName());
    } 
    else if (argumentIndex < int(matchedCall->getNumArgs()) ){
      // No match found if the index reaches NumArgs

      // Fetch the name of the parameter from the function declaration
      const auto funcDecl   = matchedCall->getDirectCallee();
      const auto paramDecl  = funcDecl->getParamDecl(argumentIndex);
      paramName             = std::string(paramDecl->getName());
    } 
  }

  return paramName;
}

void FirstPassMatcher::handleLiteralMatch(std::variant<char,uint64_t,std::string> value,
StateType matchedType, const CallExpr* call){
  // Determine which parameter this argument has been given to
  auto callPath = std::vector<DynTypedNode>();
  const auto paramName = this->getParamName(call, callPath, LITERAL[matchedType]); 
  
  auto paramIndex = getIndexOfParam(call, paramName);
  
  // Add a new ArgState entry for the function if one
  // does not exist already
  if (paramIndex == -1) {
    struct ArgState states = {
      .states = std::set<std::variant<char,uint64_t,std::string>>()
    }; 
    states.type = matchedType;

    //this->argumentStates.push_back(states);
    paramIndex = getIndexOfParam(call, paramName);
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
  //const auto alreadyNonDet = this->argumentStates[paramIndex].isNonDet;

  //if (callPath.size() >= 2 && callPath[callPath.size()-2].getNodeKind().asStringRef() \
  //    == "ImplicitCastExpr"  && !alreadyNonDet) {
  //  // Insert the encountered state for the given param
  //  assert(paramIndex >= 0);
  //  this->argumentStates[paramIndex].states.insert(value);

  //  PRINT_INFO(LITERAL[matchedType] << " param (det) " << paramName  << ": ");
  //} else {
  //  // If a  parameter should be considered nondet, we will set a flag
  //  // for the argument object (preventing further uneccessary analysis,
  //  // if other calls are made with det values does not matter if at least
  //  // one call uses a nondet argument)
  //  this->argumentStates[paramIndex].isNonDet = true;
  //  PRINT_INFO(LITERAL[matchedType] << " param (nondet) " << paramName);
  //}
}

//-----------------------------------------------------------------------------
// FirstPassASTConsumer- implementation
// FirstPassMatcher-     implementation
//-----------------------------------------------------------------------------
  
void FirstPassASTConsumer::HandleTranslationUnit(ASTContext &ctx) {
  this->finder.matchAST(ctx);
}

FirstPassASTConsumer::
FirstPassASTConsumer(std::string symbolName): matchHandler() {
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

  // We need the literals to be direct descendents (barring the implicitCastExpr)
  // otherwise they could be part of larger experssions, e.g. 'foo + 6'
  // For now, any literal that is not a direct descendent of the call will be
  // considered an undet() value
  const auto intMatcher     = integerLiteral(isArgumentOfCall).bind(LITERAL[INT]);
  const auto stringMatcher  = stringLiteral(isArgumentOfCall).bind(LITERAL[STR]);
  const auto charMatcher    = characterLiteral(isArgumentOfCall).bind(LITERAL[CHR]);
  
  // Note, for a sound solution we cannot exclude any calls (except those that
  // do not use the return value). We must therefore have a liberal matcher
  // that matches any argument formation for each parameter, if this matcher
  // is ever filled with something that we cannot handle (i.e. not a literal)
  // then the parameter is nondet().
  const auto anyMatcher     = expr(isArgumentOfCall).bind("ANY");

  // Matchers are executed in the order that they are added to the finder
  // By executing the ANY matcher first we start by enumerating all possible
  // arguments to the provided function
  //
  // If an argument not on the 'whitelist' is encountered at this stage
  // we can set it as nondet() without further analysis
  this->finder.addMatcher(anyMatcher, &(this->matchHandler));

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
  
  auto matchId=0;
  //auto matchId = rand() % 100000;
  //if (matchId%2==1) matchId += 1;

  // Holds information on the actual source code
  this->srcMgr = result.SourceManager;

  // Holds contxtual information about the AST, this allows
  // us to determine e.g. the parents of a matched node
  this->ctx = result.Context;
  this->nodeMap = result.Nodes.getMap();

  // ::run() is invoked anew for every match, the getNodes() calls
  // that get populated depend on the binds() defined for each matcher
  // i.e. all of the matches will have CALL available since the
  // 'isArgumentOfCall' is included in every matcher
  const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
  const auto *fnc        = result.Nodes.getNodeAs<FunctionDecl>("FNC");
  assert(call && fnc);

  const auto *declRef    = result.Nodes.getNodeAs<DeclRefExpr>("REF");
  const auto *anyArg     = result.Nodes.getNodeAs<Expr>("ANY");

  const auto *intLiteral = result.Nodes.getNodeAs<IntegerLiteral>(LITERAL[INT]);
  const auto *strLiteral = result.Nodes.getNodeAs<StringLiteral>(LITERAL[STR]);
  const auto *chrLiteral = result.Nodes.getNodeAs<CharacterLiteral>(LITERAL[CHR]);
  
  // To correlate the arguments that we match agianst to parameters in the function call
  // we need to traverse the call experssion and pair the arguments with the Parms from the FNC

  // Extract the filename (basename) of the current TU so that
  // the outer consumer knows what filename to use for the output file
  auto filepath = srcMgr->getFilename(call->getEndLoc()); 
  this->filename = filepath.substr(filepath.find_last_of("/\\") + 1);


  if (anyArg) {
    const auto name = anyArg->getStmtClassName();
    util::dumpMatch("ANY", name, matchId+1, this->srcMgr, anyArg->getEndLoc());

    // 1. Determine which parameter the leaf node corresponds to
    auto callPath = std::vector<DynTypedNode>();
    const auto paramName = this->getParamName(call, callPath, "ANY"); 

    // Skip matches which correspond to the called function name ('-1'th node of every call)
    if (paramName == fnc->getName()){
      return;
    }

    // 2. Save the nodeID of the leaf stmt for this match
    if (paramName.size()==0){
      PRINT_ERR("Failed to determine param for: ");
      anyArg->dumpColor();
    } else {
      auto leafStmt = util::getFirstLeaf(anyArg, ctx);

      if (this->argumentStates.count(paramName) == 0) {
        // Add a new ArgState entry does not exist already
        struct ArgState argState = {
          .ids = std::set<uint64_t>(),
          .states = std::set<std::variant<char,uint64_t,std::string>>()
        }; 
        this->argumentStates.insert(std::make_pair(paramName, argState));
      }

      // Set the argument type
      const auto className = leafStmt->getStmtClassName();
      if (NodeTypes.find(className) != NodeTypes.end()){
        this->argumentStates.at(paramName).type = NodeTypes.at(className);
      } else {
        PRINT_ERR("Unhandled leaf node type: " << className);
      }

      PRINT_INFO(paramName << ": " << leafStmt->getID(*ctx) << " " << className);

      uint64_t stmtID = leafStmt->getID(*ctx);
      this->argumentStates.at(paramName).ids.insert(stmtID);
    }




    // A ::Tree object is needed to properly find children...
    //  https://clang.llvm.org/doxygen/classclang_1_1syntax_1_1Tree.html#details


    //auto exprNode = this->nodeMap.at("ANY");
    //for (auto child : anyArg->children() ){
    //  
    //  // If the final child ID matches that of a literal (that we match later)
    //  // we can remove it, if the list of matched IDs for a specific argument
    //  // is non-empty at the end of matching => nondet
    //  PRINT_WARN( child->getID(*ctx) );
    //}
    


  }


  if(true){} //TODO
  else if (declRef) {
    // This includes a match for the actual function token (index -1)
    const auto name = declRef->getDecl()->getName();
    util::dumpMatch("REF", name, matchId+1, this->srcMgr, declRef->getEndLoc());
    
    // During the second pass we must be able to identify 
    //  * the enclosing function
    //  * the callee
    //  * the argument name
    //  for every reference that we encounter in the 1st pass
    
    auto callPath = std::vector<DynTypedNode>();
    auto paramName = this->getParamName(call, callPath, "REF"); 
  }
  else if (intLiteral) {
    const auto value =  intLiteral->getValue().getLimitedValue();
    util::dumpMatch(LITERAL[INT], value, matchId+1, this->srcMgr, intLiteral->getLocation());
    this->handleLiteralMatch(value, INT, call);
  }
  else if (strLiteral) {
    const auto value =  std::string(strLiteral->getString());
    util::dumpMatch(LITERAL[STR], value, matchId+1, this->srcMgr, strLiteral->getEndLoc());
    this->handleLiteralMatch(value, STR, call);
  }
  else if (chrLiteral) {
    const auto value =  chrLiteral->getValue();
    util::dumpMatch(LITERAL[CHR], value, matchId+1, this->srcMgr, chrLiteral->getLocation());
    this->handleLiteralMatch(value, CHR, call);
  }
}

