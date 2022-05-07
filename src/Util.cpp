#include "Util.hpp"

namespace util {
  /// Recursively go down the children() iterator of a stmt
  /// and return the leaf stmt given from always picking the first child
  const Stmt* getFirstLeaf(const Stmt* stmt, ASTContext* ctx) {
        bool hasChild = false;
        const Stmt* firstChild;

        for (const auto child : stmt->children()) {
          hasChild = true;
          firstChild = child;
          break;
        }

        if (!hasChild){
          return stmt;
        } else {
          return getFirstLeaf(firstChild, ctx);
        }
  }

}

