#include "ArgStates.hpp"
#include "Util.hpp"

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

void FirstPassMatcher::handleLiteralMatch(variants value,
StateType matchedType, const CallExpr* call, const Expr* matchedExpr){
  // Determine which parameter this argument corresponds to
  auto callPath = std::vector<DynTypedNode>();
  const auto paramName = this->getParamName(call, callPath, LITERAL[matchedType]); 
  
  // An argState entry should already exist from the ANY-matching stage for each param
  assert(this->argumentStates.count(paramName) > 0 && 
      this->argumentStates.at(paramName).type == matchedType);

  // The callPath always contains at least one element: <match> [ callexpr ]
  assert(callPath.size() >= 1);

  const auto argState         = this->argumentStates.at(paramName);
  const auto firtstParentKind = callPath[0].getNodeKind().asStringRef();
  
  bool matchIsDet = false;
 
  if (argState.isNonDet){
    // Already identified as nondet()
  }
  else if (callPath.size() == 1 && firtstParentKind == "CallExpr") {
    // If the callPath only contains 1 element we have an exact call, e.g.
    //  foo(int x) -> foo(1)
    matchIsDet = true;
  }
  else if (callPath.size() >= 2){
    // If we have #define statements akin to
    //  #define XML_FALSE ((XML_Bool)0)
    //  We get:
    //   `-ParenExpr 'XML_Bool':'unsigned char'
    //     `-CStyleCastExpr 'XML_Bool':'unsigned char' <IntegralCast>
    //       `-IntegerLiteral 'int' 0
    //
    // We can exclude all types of 'NOOP' casts and '()' like this one 
    // from an expression using built-in functionality in clang and 
    // then check if the simplified top argument
    // corresponds to the integral type that we matched to handle these cases
    //
    // If the callPath only contains 2 elements:
    //  <match>: [ implicitCast, callExpr ]
    // Then we have a 'clean' value besides an implicit cast, e.g.
    //  foo(MY_INT x) -> foo(1)
    //  This case is also covered by this check
    const auto topArg = callPath[callPath.size()-2].get<Expr>();
    const auto simplifiedTopArg = topArg->IgnoreParenNoopCasts(*ctx) \
                                  ->IgnoreImplicit()->IgnoreCasts();
    const auto type = simplifiedTopArg->getStmtClassName();
    if (NodeTypes.count(type) > 0 &&  NodeTypes.at(type) == matchedType){
        matchIsDet = true;
    }
  }

  if (matchIsDet){
    this->argumentStates.at(paramName).states.insert(value);

    // We remove the ids for every match that corresponds to a det() case 
    // At the final write-to-disk stage, the params with an empty ids[] set
    // are those that can be considered det()
    // Exactly one element should be erased with this operation
    assert(
      this->argumentStates.at(paramName).ids.erase(matchedExpr->getID(*ctx)) 
      == 1
    );

    PRINT_INFO(LITERAL[matchedType] << "> " << paramName << " (det): "
        << matchedExpr->getID(*ctx) << " (" 
        << this->argumentStates.at(paramName).ids.size() << ")" );
  } else {
    // Unmatched base case: nondet()
    this->argumentStates.at(paramName).isNonDet = true;
    PRINT_INFO(LITERAL[matchedType] << "> " << paramName << " (nondet): " 
        << matchedExpr->getID(*ctx) << " (" 
        << this->argumentStates.at(paramName).ids.size() << ")" );
  }
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

  // Matchers are executed in the _order that they are added to the finder_
  // This does not infer that the anyMatcher will go through ALL nodes before
  // any matches are handled by the other matchers, but it does mean that
  // any node visited by the other matchers will always first have been 
  // visited by the anyMatcher.
  //
  // With this in mind we can always assume that an argState entry exists
  // for a literal match since the anyMatcher will have created one during its visit
  this->finder.addMatcher(anyMatcher,     &(this->matchHandler));

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


  /***** First matching stage ****/
  // Creates a set of all node IDs that need to be inspected for
  // each argument
  if (anyArg) {
    const auto name = anyArg->getStmtClassName();
    util::dumpMatch("ANY", name, 1, this->srcMgr, anyArg->getEndLoc());

    // Determine which parameter the leaf node corresponds to
    auto callPath = std::vector<DynTypedNode>();
    const auto paramName = this->getParamName(call, callPath, "ANY"); 

    // Skip matches which correspond to the called function name ('-1'th node of every call)
    if (paramName == fnc->getName()){
      return;
    }

    if (paramName.size()==0){
      PRINT_ERR("ANY> Failed to determine param for: ");
      anyArg->dumpColor();
    } else {
      auto leafStmt = util::getFirstLeaf(anyArg, ctx);

      if (this->argumentStates.count(paramName) == 0) {
        // Add a new ArgState entry if one does not exist already
        struct ArgState argState = {
          .ids = std::set<uint64_t>(),
          .states = std::set<variants>()
        }; 
        this->argumentStates.insert(std::make_pair(paramName, argState));
      }

      // Set the argument type
      const auto className = leafStmt->getStmtClassName();
      if (NodeTypes.find(className) != NodeTypes.end()){
        this->argumentStates.at(paramName).type = NodeTypes.at(className);
      } else {
        PRINT_ERR("ANY> Unhandled leaf node type: " << className);
      }
      
      // Save the nodeID of the leaf stmt for this match
      uint64_t stmtID = leafStmt->getID(*ctx);
      this->argumentStates.at(paramName).ids.insert(stmtID);

      PRINT_INFO("ANY> " << paramName << " "<< className << ": " << leafStmt->getID(*ctx) \
          << " (" << this->argumentStates.at(paramName).ids.size() << ")" );
    }
  }
  /***** Second matching stage ****/
  else if (declRef) {
    // This includes a match for the actual function token (index -1)
    const auto name = declRef->getDecl()->getName();
    util::dumpMatch("REF", name, 1, this->srcMgr, declRef->getEndLoc());
    
    // During the second pass we must be able to identify 
    //  * the enclosing function
    //  * the callee
    //  * the argument name
    //  for every reference that we encounter in the 1st pass
    
    auto callPath = std::vector<DynTypedNode>();
    auto paramName = this->getParamName(call, callPath, "REF"); 

    if (paramName == fnc->getName()){
      return;
    }

    // Set all declrefs as nondet()
    if (this->argumentStates.count(paramName) == 0) {
      // Add a new ArgState entry if one does not exist already
      struct ArgState argState = {
        .isNonDet = true,
        .ids = std::set<uint64_t>(),
        .states = std::set<variants>(),
      }; 
      this->argumentStates.insert(std::make_pair(paramName, argState));
    } else {
      this->argumentStates.at(paramName).isNonDet = true;
    }
  }
  else if (intLiteral) {
    const auto value =  intLiteral->getValue().getLimitedValue();
    util::dumpMatch(LITERAL[INT], value, 1, this->srcMgr, intLiteral->getLocation());
    this->handleLiteralMatch(value, INT, call, intLiteral);
  }
  else if (strLiteral) {
    const auto value =  std::string(strLiteral->getString());
    util::dumpMatch(LITERAL[STR], value, 1, this->srcMgr, strLiteral->getEndLoc());
    this->handleLiteralMatch(value, STR, call, strLiteral);
  }
  else if (chrLiteral) {
    const auto value =  chrLiteral->getValue();
    util::dumpMatch(LITERAL[CHR], value, 1, this->srcMgr, chrLiteral->getLocation());
    this->handleLiteralMatch(value, CHR, call, chrLiteral);
  }
}

