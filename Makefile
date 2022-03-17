CLANG_DIR=/usr
BUILD_DIR=$(shell pwd)/build

NPROC=$(shell echo $$((`nproc` - 1)))

# The executables are configured in tools/*.cpp, we do not need 
# this for our use-case
OUT_EXEC=$(BUILD_DIR)/bin/ct-add-suffix
OUT_LIB=$(BUILD_DIR)/lib/libAddSuffix.so
OUTPUT_FILES= $(OUT_LIB) $(OUT_EXEC)

# Unlike `clang-rename`, this plugin only acts on one file at a time,
# allowing us to run several instances in parallel

TARGET_DIR=~/Repos/matrix
INPUT_FILE=$(TARGET_DIR)/src/matrix.c
INCLUDE_DIR=$(TARGET_DIR)/include

.PHONY: clean run

$(OUTPUT_FILES):
	mkdir -p $(BUILD_DIR)
	cmake -DCT_Clang_INSTALL_DIR=$(CLANG_DIR) -S. -B $(BUILD_DIR) && \
	make -C $(BUILD_DIR) -j$(NPROC) AddSuffix


run: $(OUTPUT_FILES)
	PLUGIN=$(OUT_LIB) \
	INCLUDE_DIR=$(INCLUDE_DIR) \
	TARGET_DIR=$(TARGET_DIR) \
	./run.sh $(INPUT_FILE) > /tmp/out.c

run_cat: run
	bat /tmp/out.c

clean:
	rm -rf $(BUILD_DIR)
