#include "ArgStates.hpp"

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

void ArgStatesASTConsumer::dumpArgStates(){
  // We dump the functionStates as JSON for the current TU only and join the
  // values externally in Python
  if (this->functionStates.size() == 0){
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


  uint argCnt = this->functionStates.size();
  uint i = 0;
  for (const auto &argState : this->functionStates) {

    f << INDENT << INDENT << "\"" << argState.ParamName << "\": [";

    // Nondet arguments will have been given an empty list of states
    if (!argState.IsNonDet){
      f << "\n" << INDENT << INDENT << INDENT;

      // Only one of the state sets will contain values for an argument
      writeStates(argState, f);
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

