#!/usr/bin/env python3
import json
import os
import re
import subprocess
import sys

def get_isystem_flags(source_file: str, dep_path: str) -> list:
    '''
    https://clang.llvm.org/docs/FAQ.html#id2
    The -cc1 flag is used to invoke the clang 'frontend', using only the 
    frontend infers that default options are lost, errors like 
    	'stddef.h' file not found
    are caused from the fact that the builtin-include path of clang is missing
    We can see the default frontend options used by clang with
    	clang -### test/file.cpp
    '''
    isystem_flags = subprocess.check_output(
        f"clang -### {source_file} 2>&1 | sed -E '1,4d; s/\" \"/\", \"/g; s/(.*)(\\(in-process\\))(.*)/\\1\\3/'",
        shell=True, cwd = dep_path
    ).decode('ascii').split(",")

    out = []
    add_next = False

    for flag in isystem_flags:
        flag = flag.strip().strip('"')

        if flag == '-internal-isystem':
            out.append(flag)
            add_next = True
        elif add_next:
            out.append(flag)
            add_next = False

    return out

def call_arg_states(ccdb_args: list[str]) -> None:
    '''
    Some of the ccdb arguments are not comptabile with the -cc1 frontend and need to
    be filtered out
    '''
    blacklist = r"|".join([ "-g", "-c", r"-f.*", r"-W.*" ])

    ccdb_filtered  = filter(lambda a: not re.match(blacklist, a), ccdb_args)

    cmd = [ "clang", "-cc1", "-load", PLUGIN,
        "-plugin", "ArgStates",
         "-plugin-arg-ArgStates", "-names-file", "-plugin-arg-ArgStates", FUNC_LIST,
        "-plugin-arg-ArgStates", "-suffix", "-plugin-arg-ArgStates", "_old_aaaaaaa" ] + \
        get_isystem_flags(INPUT_FILE, TARGET_DIR) + \
        [ INPUT_FILE, "-I", "/usr/include" ] + list(ccdb_filtered)

    print(' '.join(cmd))
    out = sys.stderr
    subprocess.run(cmd, cwd = TARGET_DIR, stdout = out, stderr = out)


if len(sys.argv) <= 1:
    sys.exit(1)

INPUT_FILE=sys.argv[1]
PLUGIN=os.getenv("PLUGIN")
INCLUDE_DIR=os.getenv("INCLUDE_DIR")
TARGET_DIR=str(os.getenv("TARGET_DIR"))
FUNC_LIST=os.getenv("FUNC_LIST")

with open(f"{TARGET_DIR}/compile_commands.json", mode = 'r', encoding='utf8') as f:
    ccdb = json.load(f)
    for tu in ccdb:
        if tu['file'] == INPUT_FILE:
            # Note the use of [1:-3] to skip over the cc and output files
            ccdb_args = tu['arguments'][1:-3]
            call_arg_states(ccdb_args)
            break
