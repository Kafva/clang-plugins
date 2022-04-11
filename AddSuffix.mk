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
#
# NOTE: the program considers #ifdefs and does not replace things inside false #defs
# since these elements will not be part of the parsed AST
# 
# NOTE: #macros are expanded before the AST processing, this means that we can't
# replace references inside macros in the original source unless we expand
# all macros before performing any replacements

# clang -Xclang -ast-dump ~/Repos/oniguruma/src/st.c -Isrc -I/usr/include -E
#TARGET_DIR=~/Repos/oniguruma
#INPUT_FILE=$(TARGET_DIR)/src/regcomp.c
#INCLUDE_DIR=$(TARGET_DIR)/src

# 'rehash' is used inside a macro
#REPLACE_FILE=/home/jonas/Repos/euf/clang-suffix/test/onig_tests.txt

#TARGET_DIR=~/Repos/oniguruma
#INCLUDE_DIR=$(TARGET_DIR)/src
#INPUT_FILE=~/Repos/euf/clang-suffix/test/macro.c
#REPLACE_FILE=~/Repos/euf/tests/data/oni_rename.txt
#EXPAND=false

#TARGET_DIR=~/Repos/openssl
#INCLUDE_DIR=$(TARGET_DIR)/include
#INPUT_FILE=~/Repos/openssl/include/openssl/err.h
#REPLACE_FILE=~/Repos/euf/tests/data/ssl_rename.txt
#EXPAND=true

# Look at 'redirection_ok()'
#TARGET_DIR=~/Repos/openssl
#INCLUDE_DIR=$(TARGET_DIR)/include
#INPUT_FILE=~/Repos/euf/tests/data/E_http_client.c
#REPLACE_FILE=~/Repos/euf/tests/data/ssl_rename.txt
#GREP_TARGET=redirection_ok
#EXPAND=false

#		#define ADD_DIRECT
TARGET_DIR=/home/jonas/.cache/euf/oniguruma-65a9b1aa
INCLUDE_DIR=$(TARGET_DIR)/
REPLACE_FILE=/home/jonas/Repos/euf/tests/data/oni_rename.txt
INPUT_FILE=$(TARGET_DIR)/st.c
GREP_TARGET=rehash
EXPAND=true

.PHONY: clean run

INPUT_BASE=$(shell basename $(INPUT_FILE))

$(OUTPUT_FILES): $(BUILD_DIR)/Makefile
	make -C $(BUILD_DIR) -j$(NPROC) AddSuffix

$(BUILD_DIR)/Makefile:
	mkdir -p $(BUILD_DIR)
	cmake -DCT_Clang_INSTALL_DIR=$(CLANG_DIR) -S. -B $(BUILD_DIR)

run: $(OUTPUT_FILES)
	PLUGIN=$(OUT_LIB) \
	INCLUDE_DIR=$(INCLUDE_DIR) \
	TARGET_DIR=$(TARGET_DIR) \
	REPLACE_FILE=$(REPLACE_FILE) \
	EXPAND=$(EXPAND) \
	time ./run.sh $(INPUT_FILE) > /tmp/out.c

example: run
	@cp /tmp/out.c $(INPUT_BASE)
	@wc -l $(INPUT_BASE)
	@grep -C 5 "$(GREP_TARGET)" $(INPUT_BASE) | sed '/^$$/d'  | bat --style full --language c

run_cat: run
	bat /tmp/out.c

clean:
	rm -rf $(BUILD_DIR)
