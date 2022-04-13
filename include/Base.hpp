#ifndef ArgStates_Base_H
#define ArgStates_Base_H
#include <unordered_map>
#include <string>
#include <vector>
#include <set>

#define DEBUG_AST true

#define PRINT_ERR(msg)  llvm::errs() << "\033[31m!>\033[0m " << msg << "\n"
#define PRINT_WARN(msg) llvm::errs() << "\033[33m!>\033[0m " << msg << "\n"
#define PRINT_INFO(msg) llvm::errs() << "\033[34m!>\033[0m " << msg << "\n"
typedef unsigned uint;

#define OUTPUT_FILE "/home/jonas/Repos/euf/clang-suffix/arg_states.json"

struct ArgState {
  // The ArgName will be empty for literals
  std::string ParamName;
  std::string ArgName;

  // We only need one set of states for each Arg
  // A union{} cannot be used on complex types
  // and a template type would cause issues since
  // different versions would need to be in the same array
  std::set<char> ChrStates;
  std::set<uint64_t> IntStates;
  std::set<std::string> StrStates;
};

#endif
