#include "ArgStates.hpp"

static void addComma(std::ofstream &f, uint iter, uint size, bool newline=false){
    if (iter != size) {
      f << ", ";
    }
    newline && f << "\n";
}
static void writeStates(const struct ArgState& argState, std::ofstream &f) {
      uint stateSize = argState.states.size();
      uint k = 0;
      for (const auto &item : argState.states) {
        // Write the correct type
        switch(argState.type){
          case INT:
            f << std::get<uint64_t>(item);
            break;
          case CHR:
            f << std::get<unsigned int>(item);
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

void ArgStatesASTConsumer::dumpArgStates(){
  // We dump the argumentStates as JSON for the current TU only and join the
  // values externally in Python
  if (this->argumentStates.size() == 0){
    return;
  }
  auto filename = this->getOutputPath();

  if(filename.size()==0) { 
    PRINT_ERR("No output filename configured");
    return; 
  } else {
    PRINT_INFO("Writing output to: " << filename);
  }

  std::ofstream f;
  f.open(filename, std::ofstream::out|std::ofstream::trunc);

  f << "{\n"
    << INDENT << "\"" << symbolName << "\": {\n";


  uint argCnt = this->argumentStates.size();
  uint i = 0;
  for (const auto &argStatePair : this->argumentStates) {

    f << INDENT << INDENT << "\"" << argStatePair.first << "\": [";

    // nondet() arguments will have been given an empty list of states
    // det() arguments need to have an empty ids[] set, otherwise an invocation
    // matched by ANY still exists that is nondet() for the argument
    if (!argStatePair.second.isNonDet && argStatePair.second.ids.size() == 0){
      f << "\n" << INDENT << INDENT << INDENT;

      // Only one of the state sets will contain values for an argument
      writeStates(argStatePair.second, f);
      f << "\n" << INDENT << INDENT;
    }

    f << "]";
    
    addComma(f,++i,argCnt,true);
  }

  f << INDENT << "}\n"
    << "}\n";
  f.close();
}

std::string ArgStatesASTConsumer::getOutputPath(){
    const auto outputDir = std::string(getenv(OUTPUT_DIR_ENV));
    if (this->filename.size() >= 2 && outputDir.size() > 0) {
      // <sym_name>_<tu>.json
      auto outputPath = outputDir + "/" + this->symbolName + "_" + 
                        this->filename.substr(0,this->filename.size()-2) +
                        ".json";
      return outputPath;
    } else {
      return std::string();
    }
}

