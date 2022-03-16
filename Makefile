CLANG_DIR=/usr
BUILD_DIR=./build

INPUT=test/file.cpp

.PHONY: clean run

$(BUILD_DIR)/lib/libCodeRefactor.so:
	mkdir -p $(BUILD_DIR)
	cmake -DCT_Clang_INSTALL_DIR=$(CLANG_DIR) -S. -B $(BUILD_DIR) &&
		make -C $(BUILD_DIR) -j $(shell `nproc` - 1) CodeRefactor &&
run:
	$(CLANG_DIR)/bin/clang -cc1 -load $(BUILD_DIR)/lib/libCodeRefactor.so \
		-plugin CodeRefactor \
		-plugin-arg-CodeRefactor -class-name -plugin-arg-CodeRefactor Base  \
		-plugin-arg-CodeRefactor -old-name -plugin-arg-CodeRefactor foo  \
		-plugin-arg-CodeRefactor -new-name -plugin-arg-CodeRefactor bar $(INPUT)
clean:
	rm -rf $(BUILD_DIR)
