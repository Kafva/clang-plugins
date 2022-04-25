#include "ArgStates.hpp"
#include "Util.hpp"

//-----------------------------------------------------------------------------
// SecondPassASTConsumer- implementation
// SecondPassMatcher-     implementation
// (Unfinished)
//-----------------------------------------------------------------------------
void SecondPassASTConsumer::HandleTranslationUnit(ASTContext &ctx) {
  this->finder.matchAST(ctx);
}

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
    // In the second pass we will match agianst every plain declRef
    // that was encountered as an argument during the first pass
    //
    // We can only derive state information if we find ALL references
    // to the the variable, and all are simple assignments
    // or simple statements where we know its value is unchanged.
    //
    // Any reference to the variable which we cannot conclusivly
    // say gives it a new value or does NOT give it a new value
    // need to be treated as potential state changes...


    // Holds information on the actual source code
    this->srcMgr = result.SourceManager;

    // Holds contxtual information about the AST, this allows
    // us to determine e.g. the parents of a matched node
    this->ctx = result.Context;
    this->nodeMap = result.Nodes.getMap();

    //const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
    const auto *declRef    = result.Nodes.getNodeAs<DeclRefExpr>("REF");

    if (declRef) {
      const auto name = declRef->getDecl()->getName();
      util::dumpMatch("REF", name, 2, srcMgr, declRef->getEndLoc());
    }
}

