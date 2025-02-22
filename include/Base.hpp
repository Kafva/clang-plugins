#ifndef ArgStates_Base_H
#define ArgStates_Base_H

#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <variant>
#include <tuple>


#define OUTPUT_DIR_ENV "ARG_STATES_OUT_DIR"
#define DEBUG_ENV "DEBUG_AST"
#define INDENT "  "

#define PRINT_ERR(msg)                              llvm::errs() << \
                            "\033[31m!>\033[0m " << msg << "\n"
#define PRINT_WARN(msg) if (getenv(DEBUG_ENV)!=NULL) llvm::errs() << \
                            "\033[33m!>\033[0m " << msg << "\n"
#define PRINT_INFO(msg) if (getenv(DEBUG_ENV)!=NULL) llvm::errs() << \
                            "\033[34m!>\033[0m " << msg << "\n"
typedef unsigned uint;
typedef std::variant<unsigned int,uint64_t,std::string> variants;

//-----------------------------------------------------------------------------
// Argument state structures
// We will need a separate struct for passing values to the second pass
//-----------------------------------------------------------------------------
enum StateType {
  CHR, INT, STR, UNARY, NONE
};

struct ArgState {
  bool isNonDet = false;
  StateType type = NONE;

  // Populated with the (leaf) node ID of every expr that is passed
  // to this function parameter in the current TU
  std::set<uint64_t> ids;

  // We only need one set of states for each Arg
  // This solution with variant requires C++17
  // Characters a represented as unsigned int
  std::set<variants> states;

  // Will be empty for parameters without names in their declaration, e.g.
  //  foo(int, char*)
  std::string paramName;
};

using namespace clang;
using namespace ast_matchers;

#endif
