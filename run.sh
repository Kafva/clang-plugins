#!/usr/bin/env bash
die(){ echo -e "$1" >&2 ; exit 1; }
usage="usage: $(basename $0) <file.c>"
[ -z "$1" ] && die "$usage"

[[  -z "$PLUGIN" || -z "$INCLUDE_DIR" || -z "$TARGET_DIR"  ]] && 
  die "Missing environment variable(s)"


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


# TODO read compile_commands.json for includes
cd $TARGET_DIR
clang -cc1 -load "$PLUGIN" \
	-plugin AddSuffix \
	-plugin-arg-AddSuffix -name -plugin-arg-AddSuffix matrix_init  \
	-plugin-arg-AddSuffix -suffix -plugin-arg-AddSuffix _old \
	$(cat $isystem_flags) \
	$TARGET_FILE -I $INCLUDE_DIR -I/usr/include

