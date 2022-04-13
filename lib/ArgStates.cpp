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
#include <string>
#include <fstream>
#include <unordered_set>

using namespace clang;
using namespace ast_matchers;

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
}

//-----------------------------------------------------------------------------
// FirstPassASTConsumer- implementation
// FirstPassMatcher-     implementation
//-----------------------------------------------------------------------------

/// Specifies the node patterns that we want to analyze further in ::run()
FirstPassASTConsumer::FirstPassASTConsumer(
    std::vector<std::string> Names
    ) : MatchHandler(), Names(Names) {

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
          functionDecl(hasName("XML_ExternalEntityParserCreate")
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
  const auto memMatcher = memberExpr(
    unless(hasAncestor(memberExpr())),
    isArgumentOfCall
  ).bind("MEM");
  const auto intMatcher = integerLiteral(isArgumentOfCall).bind("INT");
  const auto stringMatcher = stringLiteral(isArgumentOfCall).bind("STR");
  const auto charMatcher = characterLiteral(isArgumentOfCall).bind("CHR");

  this->Finder.addMatcher(declRefMatcher, &(this->MatchHandler));
  this->Finder.addMatcher(memMatcher,     &(this->MatchHandler));
  this->Finder.addMatcher(intMatcher,     &(this->MatchHandler));
  this->Finder.addMatcher(stringMatcher,  &(this->MatchHandler));
  this->Finder.addMatcher(charMatcher,    &(this->MatchHandler));
}

static void getCallPath(ASTContext* ctx, BoundNodes::IDToNodeMap &nodeMap, 
    DynTypedNode &parent, std::string bindName, std::vector<DynTypedNode> &callPath){
    // Go up until we reach a call experssion
    callPath.push_back(parent); 

    // The ASTNodeKind returned from this method reflects the strings seen in
    // clang -ast-dump
    if (parent.getNodeKind().asStringRef() == "CallExpr"){
      // There should be an enum for us to match agianst but I cannot find it
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

void FirstPassMatcher::run(const MatchFinder::MatchResult &result) {
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

    // PRINT_WARN("First pass! (run)");

    // Holds information on the actual source code
    const auto srcMgr = result.SourceManager;

    // Holds contxtual information about the AST, this allows
    // us to determine e.g. the parents of a matched node
    const auto ctx = result.Context;
    
    auto nodeMap = result.Nodes.getMap();

    const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
    const auto *func       = result.Nodes.getNodeAs<FunctionDecl>("FNC");
    

    // To correlate the arguments that we match agianst to parameters in the function call
    // we need to traverse the call experssion and pair the arguments with the Parms from the FNC
    const int numArgs = call->getNumArgs();

    // Call child cnt == 3
    // Can we get the index of our match?
    // If so, then we know which param we matched


    #if 1
    const auto *declRef    = result.Nodes.getNodeAs<DeclRefExpr>("REF");
    const auto *memExpr    = result.Nodes.getNodeAs<MemberExpr>("MEM");

    const auto *intLiteral = result.Nodes.getNodeAs<IntegerLiteral>("INT");
    const auto *strLiteral = result.Nodes.getNodeAs<StringLiteral>("STR");
    const auto *chrLiteral = result.Nodes.getNodeAs<CharacterLiteral>("CHR");

    if (declRef) {
      const auto name = declRef->getDecl()->getName();
      dumpMatch("REF", name, 1, srcMgr, declRef->getEndLoc());
    }
    if (memExpr) {
      const auto name = memExpr->getMemberNameInfo().getAsString();
      dumpMatch("MEM", name, 1, srcMgr, memExpr->getEndLoc());
    }
    if (intLiteral) {
      const auto value =  intLiteral->getValue();
      dumpMatch("INT", value, 1, srcMgr, intLiteral->getLocation());

      // Determine which parameter this argument has been given to
      auto parents = ctx->getParents(nodeMap.at("INT"));
      auto callPath = std::vector<DynTypedNode>();
      
      if (parents.size()>0) {
        // We assume .getParents() only returns one entry
        auto parent = parents[0];
        // Move up recursivly until we reach a Call expression
        // (we need to drop implicit casts etc.)
        // Since we have saved all of the nodes in the path we traversed
        // upwards, we can check which of the arguments our path corresponds to
        getCallPath(ctx, nodeMap, parent, "INT", callPath);

        // We use .push_back() so the last item will be the actual call
        // we are intrested in the direct child from the call that is on the
        // path towards our match, which will be at the penultimate position, 
        // if the callPath only contains one item, then our matched node is 
        // a direct child of the callexpr
        auto ourPath = callPath.size() > 1 ? 
          callPath[callPath.size()-2] : 
          nodeMap.at("INT"); 

        auto ourExpr = ourPath.get<Expr>();

        int argumentIndex = 0;
        for (auto call_arg : call->arguments()){
          // Break once we find an argument in the matched call expression
          // that matches the node we found traversing the AST upwards from
          // our match
          if (call_arg->getID(*ctx) == ourExpr->getID(*ctx) ) {
            break;
          }
          argumentIndex++;
        }

        PRINT_WARN( "call path " << callPath.size() << " " << argumentIndex);
      }
      

    }
    if (strLiteral) {
      const auto value =  strLiteral->getString();
      dumpMatch("STR", value, 1, srcMgr, strLiteral->getEndLoc());
    }
    if (chrLiteral) {
      const auto value =  chrLiteral->getValue();
      dumpMatch("CHR", value, 1, srcMgr, chrLiteral->getLocation());
    }
    if (func){
      const auto name = func->getName(); 
      dumpMatch("FNC", name, 1, srcMgr, func->getEndLoc() );
    }
    #endif
}

//-----------------------------------------------------------------------------
// SecondPassASTConsumer- implementation
// SecondPassMatcher-     implementation
//-----------------------------------------------------------------------------

SecondPassASTConsumer::SecondPassASTConsumer(std::vector<std::string> Names
    ) : MatchHandler(), Names(Names)  {

    // PRINT_WARN("Second pass!");

    const auto isArgumentOfCall = hasAncestor(
        callExpr(callee(
            functionDecl(hasName("XML_ExternalEntityParserCreate"))
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


    this->Finder.addMatcher(declRefMatcher, &(this->MatchHandler));
}

void SecondPassMatcher::run(const MatchFinder::MatchResult &result) {
    // Holds information on the actual sourc code
    const auto srcMgr = result.SourceManager;

    // Holds contxtual information about the AST, this allows
    // us to determine e.g. the parents of a matched node
    const auto ctx = result.Context;

    const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
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

    unsigned namesDiagID = diagnostics.getCustomDiagID(
      DiagnosticsEngine::Error, "missing -names-file"
    );

    for (size_t i = 0, size = args.size(); i != size; ++i) {
      if (args[i] == "-names-file") {
         if (parseArg(diagnostics, namesDiagID, size, args, i)){
             auto NamesFile = args[++i];
             this->readNamesFromFile(NamesFile);
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
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {

    return std::make_unique<ArgStatesASTConsumer>(this->Names);
    //RewriterForArgStates.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

    //auto astConsumer = std::make_unique<ArgStatesASTConsumer>(
    //    RewriterForArgStates, this->Names
    //);

    //return astConsumer;
  }

private:
  void readNamesFromFile(std::string filename) {
    std::ifstream file(filename);

    if (file.is_open()) {
      std::string line;

      while (std::getline(file,line)) {
        this->Names.push_back(line);
      }
      file.close();
    }
  }

  bool parseArg(DiagnosticsEngine &diagnostics, unsigned diagID, int size,
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
  std::vector<std::string> Names;
};

static FrontendPluginRegistry::Add<ArgStatesAddPluginAction>
    X(/*NamesFile=*/"ArgStates",
      /*Desc=*/"Enumerate the possible states for arguments to calls of the functions given in the -names-file argument.");
