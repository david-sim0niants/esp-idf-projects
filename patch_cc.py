#!/usr/bin/python3

import json, sys
from pathlib import Path

REPLACE_FLAGS = {
    '-mlongcalls': '',
    '-mlong-calls': '',
    '-fno-shrink-wrap': '',
    '-fno-shrink-wrap': '',
    '-fstrict-volatile-bitfields': '',
    '-fno-tree-switch-conversion': '',
    '-fzero-init-padding-bits=all': '',
    '-fno-malloc-dce': ''
}

def fix_comm_args(comm: str):
    for old_f, new_f in REPLACE_FLAGS.items():
        comm = comm.replace(old_f, new_f)
    return comm

def fix_compiler(comm: str):
    comm_splits = comm.split()
    curr_compiler = comm_splits[0]
    for compiler in ['gcc', 'g++']:
        idx = curr_compiler.find(compiler)
        if idx == -1:
            continue;
        curr_compiler = curr_compiler[idx:]

    comm_splits[0] = curr_compiler
    return ' '.join(comm_splits)

cc_file = Path("compile_commands.json")
if not cc_file.exists():
    print("compile_commands.json not found.", file=sys.stderr)
    exit(1)

with cc_file.open("r") as f:
    db = json.load(f)

for entry in db:
    if "command" in entry:
        entry["command"] = fix_comm_args(fix_compiler(entry["command"]))

with cc_file.open("w") as f:
    json.dump(db, f, indent=2)
