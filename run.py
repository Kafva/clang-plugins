#!/usr/bin/env python3
import json, re, subprocess, sys, os

TARGET_DIR="/home/jonas/.cache/euf/libexpat-90ed5777/expat"
PLUGIN="/home/jonas/Repos/euf/clang-suffix/build/lib/libArgStates.so"
SYMBOL_NAME="XML_ExternalEntityParserCreate"
#SOURCE_SUB_DIR=f"{TARGET_DIR}/xmlwf"
SOURCE_SUB_DIR=f"{TARGET_DIR}/lib"

SYMBOL_LIST="/home/jonas/Repos/euf/tests/expected/libexpat_90ed_ef31_change_set.txt"
OUTDIR="/home/jonas/Repos/euf/clang-suffix/states"


# 1. Split up the dep dir into subdirs (including top level)
# 2. Iterate over CHANGED_FUNCTIONS and call for each name ONCE per directory

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

def call_arg_states(ccdb_args: list[str], cwd: str, symbol_name: str) -> None:
    '''
    Some of the ccdb arguments are not comptabile with the -cc1 frontend and need to
    be filtered out
    '''
    blacklist = r"|".join(["-g", "-c", r"-f.*", r"-W.*"])

    ccdb_filtered  = filter(lambda a: not re.match(blacklist, a), ccdb_args)
    script_env = os.environ.copy()
    script_env.update({ "ARG_STATES_OUT_DIR": OUTDIR })


    # We assume that the isystem-flags are the same for all source files in a directory
    cmd = [ "clang", "-cc1", "-load", PLUGIN,
        "-plugin", "ArgStates",
        "-plugin-arg-ArgStates", "-symbol-name", "-plugin-arg-ArgStates", symbol_name ] + \
        get_isystem_flags(list(INPUT_FILES)[0], TARGET_DIR) + \
        list(INPUT_FILES) + [ "-I", "/usr/include" ] + list(ccdb_filtered)

    #print(f"({cwd})> \n", ' '.join(cmd))
    out = sys.stderr
    subprocess.run(cmd, cwd = cwd, stdout = out, stderr = out, env = script_env)

# We will run the plugin once PER changed name PER source directory
# If we try to run if once per changed named the include paths become inconsisitent between TUs
# Running the plugin for all names (and once per file) is a bad idea as seen with the uber hack macros
# in clang-suffix.
#
# For this to work we need to create a union of all the ccmd flags for each directory
INPUT_FILES = set()

with open(f"{TARGET_DIR}/compile_commands.json", mode = 'r', encoding='utf8') as f:
    ccdb = json.load(f)
    ccdb_args = set()
    for tu in ccdb:
        if tu['directory'] == SOURCE_SUB_DIR:
            # Note the use of [1:-3] to skip over the cc and output files
            ccdb_args |= set( tu['arguments'][1:-3] )
            INPUT_FILES.add(tu['file'])

    with open(SYMBOL_LIST, mode = 'r', encoding='utf8') as f:
        for sym in f.readlines():
            call_arg_states(list(ccdb_args), SOURCE_SUB_DIR, sym.rstrip('\n') )
