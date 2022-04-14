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

#define OUTPUT_DIR_ENV "ARG_STATES_OUT_DIR"
#define DEBUG_AST true
#define INDENT "  "

#define PRINT_ERR(msg)  llvm::errs() << "\033[31m!>\033[0m " << msg << "\n"
#define PRINT_WARN(msg) llvm::errs() << "\033[33m!>\033[0m " << msg << "\n"
#define PRINT_INFO(msg) llvm::errs() << "\033[34m!>\033[0m " << msg << "\n"
typedef unsigned uint;

//-----------------------------------------------------------------------------
// Argument state structures
// We will need a seperate struct for passing values to the second pass
//-----------------------------------------------------------------------------
enum StateType {
  CHR, INT, STR, NONE
};

struct ArgState {
  // The ArgName will be empty for literals
  std::string ParamName;
  std::string ArgName;
  bool IsNonDet = false;
  StateType Type = NONE;


  // We only need one set of states for each Arg
  // This solution with variant requires C++17
  std::set<std::variant<char,uint64_t,std::string>> States;
};

using namespace clang;
using namespace ast_matchers;

#endif
