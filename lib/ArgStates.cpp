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

void FirstPassMatcher::run(const MatchFinder::MatchResult &result) {
    // The idea:
    // Determine what types of arguments are passed to the function
    // For literal and NULL arguments, we add their value to the state space
    // For declrefs, we go up in the AST until we reach the enclosing function
    // and record all assignments to the declref
    // For other types, we set nondet for now
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

    const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
    const auto *func       = result.Nodes.getNodeAs<FunctionDecl>("FNC");
    
    const auto *declRef    = result.Nodes.getNodeAs<DeclRefExpr>("REF");
    const auto *memExpr    = result.Nodes.getNodeAs<MemberExpr>("MEM");

    const auto *intLiteral = result.Nodes.getNodeAs<IntegerLiteral>("INT");
    const auto *strLiteral = result.Nodes.getNodeAs<StringLiteral>("STR");
    const auto *chrLiteral = result.Nodes.getNodeAs<CharacterLiteral>("CHR");


    if (declRef) {
      const auto location = srcMgr->getFileLoc(declRef->getEndLoc());

      llvm::errs() << "(1) REF> " << location.printToString(*srcMgr)
        << " " << declRef->getDecl()->getName()
        << "\n";

      // If an argument refers to a declref we will walk up the AST and 
      // investigate what values are assigned to the identifier in question
     
      // We could techincally miss stuff if there are aliased ptrs
      // to the identifier
      

      if (call) {
        auto parents = ctx->getParents(*call);

        for (auto parent : parents){
          llvm::errs() << "Call parent:" << 
            parent.getNodeKind().asStringRef() << "\n";
        }


      } else {
        PRINT_ERR("No enclosing call");
      }

      //declRef->dumpColor();
    }
    if (memExpr) {
      const auto location = srcMgr->getFileLoc(memExpr->getEndLoc());

      llvm::errs() << "(1) MEM> " << location.printToString(*srcMgr)
        << " " << memExpr->getMemberNameInfo().getAsString()
        << "\n";
    }
    if (intLiteral) {
      const auto location = srcMgr->getFileLoc(intLiteral->getLocation());

      llvm::errs() << "(1) INT> " << location.printToString(*srcMgr)
        << " " << intLiteral->getValue()
        << "\n";
    }
    if (strLiteral) {
      llvm::errs() << "(1) STR> "
        << " " << strLiteral->getString()
        << "\n";
    }
    if (chrLiteral) {
      const auto location = srcMgr->getFileLoc(chrLiteral->getLocation());

      llvm::errs() << "(1) CHR> " << location.printToString(*srcMgr)
        << " " << chrLiteral->getValue()
        << "\n";
    }
    //if (func){
    //  const auto location = srcMgr->getFileLoc(func->getEndLoc());
    //  llvm::errs() << "FNC> " << location.printToString(*srcMgr)
    //    << " " << func->getName()
    //    << "\n";
    //}
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
    // PRINT_WARN("Second pass! (run)");

    // Holds information on the actual sourc code
    const auto srcMgr = result.SourceManager;

    // Holds contxtual information about the AST, this allows
    // us to determine e.g. the parents of a matched node
    const auto ctx = result.Context;

    const auto *call       = result.Nodes.getNodeAs<CallExpr>("CALL");
    const auto *declRef    = result.Nodes.getNodeAs<DeclRefExpr>("REF");

    if (declRef) {
      const auto location = srcMgr->getFileLoc(declRef->getEndLoc());

      llvm::errs() << "(2) REF> " << location.printToString(*srcMgr)
        << " " << declRef->getDecl()->getName()
        << "\n";

      // If an argument refers to a declref we will walk up the AST and 
      // investigate what values are assigned to the identifier in question
     
      // We could techincally miss stuff if there are aliased ptrs
      // to the identifier
      

      if (call) {
        auto parents = ctx->getParents(*call);

        for (auto parent : parents){
          llvm::errs() << "Call parent:" << 
            parent.getNodeKind().asStringRef() << "\n";
        }


      } else {
        PRINT_ERR("No enclosing call");
      }

      //declRef->dumpColor();
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
