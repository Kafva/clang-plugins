CLANG_INSTALL_DIR?=/usr
LLVM_INCLUDE_DIR?=/usr/include/llvm
LLVM_CMAKE_DIR?=/usr/lib/cmake/clang

BUILD_DIR=$(shell pwd)/build
NPROC=$(shell echo $$((`nproc` - 1)))

OUT_LIB=$(BUILD_DIR)/lib/libArgStates.so
OUTPUT= $(OUT_LIB) $(OUT_EXEC)
SRCS=src/ArgStates.cpp src/SecondPass.cpp src/FirstPass.cpp src/WriteJson.cpp \
		 include/ArgStates.hpp include/Util.hpp include/Base.hpp
.PHONY: clean run all

STATES=.states

all: $(OUTPUT)

$(OUTPUT): $(BUILD_DIR)/Makefile $(SRCS)
	make -C $(BUILD_DIR) -j$(NPROC) ArgStates

#	-DCMAKE_CXX_FLAGS=-std=c++17
$(BUILD_DIR)/Makefile:
	mkdir -p $(BUILD_DIR)
	cmake -DCLANG_INSTALL_DIR=$(CLANG_INSTALL_DIR) \
		-DLLVM_INCLUDE_DIR=$(LLVM_INCLUDE_DIR) \
		-DLLVM_CMAKE_DIR=$(LLVM_CMAKE_DIR) \
		-S. -B $(BUILD_DIR)

run: $(OUTPUT)
	@mkdir -p $(STATES)
	./run.py
	bat $(STATES)/*.json

clean:
	rm -rf $(BUILD_DIR)
