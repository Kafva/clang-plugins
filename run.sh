#!/usr/bin/env bash
die(){ echo -e "$1" >&2 ; exit 1; }
usage="usage: $(basename $0) <file.c>"
[ -z "$1" ] && die "$usage"

[[  -z "$PLUGIN" || -z "$INCLUDE_DIR" || -z "$TARGET_DIR" || 
  -z "$REPLACE_FILE" ]] && die "Missing environment variable(s)"

# https://clang.llvm.org/docs/FAQ.html#id2
# The -cc1 flag is used to invoke the clang 'frontend', using only the frontend
# infers that default options are lost, errors like 
# 	'stddef.h' file not found
# are caused from the fact that the builtin-include path of clang is missing
# We can see the default frontend options used by clang with
# 	clang -### test/file.cpp
TARGET_FILE=$1
frontend_flags=$(clang -### "$1" 2>&1 | sed '1,4d; s/" "/", "/g')
isystem_flags=$(mktemp)

python3 << EOF > $isystem_flags
print_next = False
out = ""

for flag in [ $frontend_flags ]:
  if flag == '-internal-isystem':
    out += " " + flag
    print_next = True
  elif print_next:
    out += " " + flag
    print_next = False

print(out)
EOF

# To allow us to replace references to global symbols inside macros
# we first preprocess the file, only expanding #define statements
# In oniguruma this results in certain functions gaining a 'onig_' prefix...
# due to a #define in regint.h
#
# Using the --passthru-includes option avoids expansion of the #include lines
# in the output but it still processes defines in these headers. The .h files
# could contain #macros so this is preferable.
#
# The question is if compilation will still go through with the normal
# build process with the header included...
#
# I have a feeling it will since any macros in the .c file will simply already
# have been expanded...
#   https://stackoverflow.com/q/65045678/9033629

expand_macros(){
  # TODO: PASS CORRECT DEFINES HERE
  # --passthru-defines --passthru-unknown-exprs --passthru-magic-macros
  pcpp --passthru-comments --passthru-includes ".*" \
    --line-directive --passthru-unfound-includes  \
     "$1"
}

expanded_file=$(mktemp --suffix .c)

expand_macros $TARGET_FILE > $expanded_file

# Verify that the expanded file does not have any weird
# re-#define behaviour
diff <(expand_macros $expanded_file) $expanded_file ||
  die "Preprocessing is not idempotent for $TARGET_FILE"


# TODO read compile_commands.json for includes
cd $TARGET_DIR
clang -cc1 -load "$PLUGIN" \
	-plugin AddSuffix \
	-plugin-arg-AddSuffix -names-file -plugin-arg-AddSuffix $REPLACE_FILE  \
	-plugin-arg-AddSuffix -suffix -plugin-arg-AddSuffix _old_aaaaaaa \
	$(cat $isystem_flags) \
	$expanded_file -I $INCLUDE_DIR -I/usr/include

