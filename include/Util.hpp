#ifndef ArgStates_Util_H
#define ArgStates_Util_H
#include "Base.hpp"

void DumpArgStates(std::unordered_map<std::string,std::vector<ArgState>> 
  &FunctionStates, std::string filename);

#endif
