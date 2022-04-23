#!/usr/bin/env python3
'''
This script assumes that clang-plugins is being ran as a submodule in euf
'''
from pathlib import Path
import sys
sys.path.append("..")

BASE_DIR = f"{str(Path(__file__).parent.parent.absolute())}/clang-plugins"

from src.arg_states import get_subdir_tus, call_arg_states_plugin
from src.util import mkdir_p, remove_files_in

QUIET = False
SYMBOL_LIST="/home/jonas/Repos/euf/tests/expected/libexpat_90ed_ef31_change_set.txt"

#TARGET_DIR="/home/jonas/Repos/jabberd-2.7.0"
#SOURCE_SUB_DIR=f"{TARGET_DIR}/sx"

TARGET_DIR="/home/jonas/.cache/euf/libexpat-90ed5777/expat"
SOURCE_SUB_DIR=f"{TARGET_DIR}/xmlwf"
#SOURCE_SUB_DIR=f"{TARGET_DIR}/lib"

if __name__ == '__main__':
    subdir_tus = get_subdir_tus(TARGET_DIR, TARGET_DIR)
    subdir_tu  = subdir_tus[SOURCE_SUB_DIR]
    outdir = f"{BASE_DIR}/.states"
    mkdir_p(outdir)
    remove_files_in(outdir)

    with open(SYMBOL_LIST, mode = 'r', encoding='utf8') as f:
        for sym in f.readlines():
            sym = sym.rstrip('\n')
            print(f"===> {sym} <===")
            call_arg_states_plugin(sym, outdir, TARGET_DIR, SOURCE_SUB_DIR, subdir_tu, quiet=QUIET)
            break
