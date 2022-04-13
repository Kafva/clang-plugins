CLANG_DIR=/usr
BUILD_DIR=$(shell pwd)/build
NPROC=$(shell echo $$((`nproc` - 1)))

OUT_LIB=$(BUILD_DIR)/lib/libArgStates.so
OUTPUT= $(OUT_LIB) $(OUT_EXEC)
SRCS=lib/ArgStates.cpp include/ArgStates.hpp
.PHONY: clean run

$(OUTPUT): $(BUILD_DIR)/Makefile $(SRCS)
	make -C $(BUILD_DIR) -j$(NPROC) ArgStates

$(BUILD_DIR)/Makefile:
	mkdir -p $(BUILD_DIR)
	cmake -DCT_Clang_INSTALL_DIR=$(CLANG_DIR) -S. -B $(BUILD_DIR)

run: $(OUTPUT)
	./run.py
	cat arg_states.json

clean:
	rm -rf $(BUILD_DIR)
