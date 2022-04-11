CLANG_DIR=/usr
BUILD_DIR=$(shell pwd)/build
NPROC=$(shell echo $$((`nproc` - 1)))

OUT_LIB=$(BUILD_DIR)/lib/libArgStates.so
SRCS=lib/ArgStates.cpp include/ArgStates.hpp

TARGET_DIR=~/.cache/euf/libexpat-90ed5777/expat
INCLUDE_DIR=$(TARGET_DIR)/lib
FUNC_LIST=~/Repos/euf/tests/data/expat_rename_2020_2022.txt
INPUT_FILE=$(TARGET_DIR)/lib/xmlparse.c

.PHONY: clean run

$(OUT_LIB): $(BUILD_DIR)/Makefile $(SRCS)
	make -C $(BUILD_DIR) -j$(NPROC) ArgStates

$(BUILD_DIR)/Makefile:
	mkdir -p $(BUILD_DIR)
	cmake -DCT_Clang_INSTALL_DIR=$(CLANG_DIR) -S. -B $(BUILD_DIR)

run: $(OUT_LIB)
	@PLUGIN=$(OUT_LIB) \
	TARGET_DIR=$(TARGET_DIR) \
	INCLUDE_DIR=$(INCLUDE_DIR) \
	FUNC_LIST=$(FUNC_LIST) \
	./run.py $(INPUT_FILE)


clean:
	rm -rf $(BUILD_DIR)
