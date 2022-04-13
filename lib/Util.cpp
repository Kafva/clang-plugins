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

template<typename T>
static void writeStates(std::ofstream &f, std::set<T> Set) {
      // Integer state
      uint stateSize = Set.size();
      uint k = 0;
      for (const auto &item : Set) {
        f << item;
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

      f << INDENT << INDENT << "\"" << argState.ParamName << "\": [\n"
        << INDENT << INDENT << INDENT;

      // Only one of the state sets will contain values for an argument
      writeStates(f, argState.IntStates);
      writeStates(f, argState.ChrStates);
      writeStates(f, argState.StrStates);

      
      f << "\n" << INDENT << INDENT << "]";
      addComma(f,++j,argCnt,true);
    }


    f << INDENT << "}";
    addComma(f,++i,functionCnt,true);
  }
  
  f << "}\n";
  f.close();
}

