#!/usr/bin/env python3
'''
This script assumes that clang-plugins is being ran as a submodule in euf
'''
import sys
sys.path.append("..")
from cparser.arg_states import get_subdir_tus, call_arg_states_plugin

TARGET_DIR="/home/jonas/.cache/euf/libexpat-90ed5777/expat"
#SOURCE_SUB_DIR=f"{TARGET_DIR}/xmlwf"
SOURCE_SUB_DIR=f"{TARGET_DIR}/lib"
QUIET = True
SYMBOL_LIST="/home/jonas/Repos/euf/tests/expected/libexpat_90ed_ef31_change_set.txt"

if __name__ == '__main__':
    subdir_tus = get_subdir_tus(TARGET_DIR)
    subdir_tu  = subdir_tus[SOURCE_SUB_DIR]

    with open(SYMBOL_LIST, mode = 'r', encoding='utf8') as f:
        for sym in f.readlines():
            sym = sym.rstrip('\n')
            print(f"===> {sym} <===")
            call_arg_states_plugin(TARGET_DIR, SOURCE_SUB_DIR, subdir_tu, sym, quiet=QUIET)
