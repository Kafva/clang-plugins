#ifndef ArgStates_Util_H
#define ArgStates_Util_H

#include "Base.hpp"

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

namespace util {
  
  const Stmt* getFirstLeaf(const Stmt* stmt, ASTContext* ctx);

  // Template functions need to be visible to every TU that uses them and
  // one must therefore have the implementation inside of a header
  template<typename T>
  void dumpMatch(std::string type, T msg, int pass, SourceManager* srcMgr, 
  SourceLocation srcLocation) {
    #if DEBUG_AST
      const auto location = srcMgr->getFileLoc(srcLocation);
      llvm::errs() << "\033[35m" << pass << "\033[0m: " << type << "> " 
        << location.printToString(*srcMgr)
        << " " << msg
        << "\n";
    #endif
    return;
  }
}

#endif
