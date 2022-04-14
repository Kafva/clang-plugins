#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>

#include "ArgStates.hpp"

#define INDENT "  "

static void addComma(std::ofstream &f, uint iter, uint size, bool newline=false){
    if (iter != size) {
      f << ", ";
    }
    newline && f << "\n";
}
static void writeStates(const struct ArgState& argState, std::ofstream &f) {
      uint stateSize = argState.States.size();
      uint k = 0;
      for (const auto &item : argState.States) {
        // Write the correct type
        switch(argState.Type){
          case INT:
            f << std::get<uint64_t>(item);
            break;
          case CHR:
            f << std::get<char>(item);
            break;
          case STR:
            f << std::get<std::string>(item);
            break;
          default:
            PRINT_ERR("ArgState with 'NONE' type encountered");
        }
        k++;
        addComma(f,k,stateSize);
      }
}

void DumpArgStates(std::unordered_map<std::string,std::vector<ArgState>> &functionStates,
 std::string filename){
  // We will dump the FunctionStates as JSON for the current TU only and join the
  // values externally in Python
  if (functionStates.size() == 0){
    return;
  }
  PRINT_WARN("Time to write states");

  std::ofstream f;
  f.open(filename, std::ofstream::out|std::ofstream::trunc);

  f << "{\n";

  // Iterate over key:values in the map
  uint functionCnt = functionStates.size();
  uint i = 0;
  for (const auto &funcMap : functionStates) {
    f << INDENT << "\"" << funcMap.first << "\": {\n";


    uint argCnt = funcMap.second.size();
    uint j = 0;
    for (const auto &argState : funcMap.second) {

      f << INDENT << INDENT << "\"" << argState.ParamName << "\": [";

      // Nondet arguments will have be given an empty list of states
      if (!argState.IsNonDet){
        f << "\n" << INDENT << INDENT << INDENT;

        // Only one of the state sets will contain values for an argument
        writeStates(argState, f);
        f << "\n" << INDENT << INDENT;
      }

      f << "]";
      
      addComma(f,++j,argCnt,true);
    }

    f << INDENT << "}";
    addComma(f,++i,functionCnt,true);
  }
  
  f << "}\n";
  f.close();
}

