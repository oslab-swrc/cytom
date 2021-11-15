#!/usr/bin/env python3

# SPDX-License-Identifier: MIT

import os
import sys
import argparse
import json
import subprocess

import config

parser = argparse.ArgumentParser(description='add/delete algorithms to Cytom.')
#actions
parser.add_argument('--add', action='store_true')
parser.add_argument('--delete', action='store_true')
parser.add_argument('--clear', action='store_true') # delete all added algorithms
parser.add_argument('--list', action='store_true') # list all added algorithm after other actions (add/delete)

# options
parser.add_argument('--name', nargs="*",
                   help='name(s) of the new algorithm')
parser.add_argument('--path', nargs="*",
                   help='absolute path(s) to the new algorithm')

args = parser.parse_args()

if args.add:
    if not args.name:
        print("Please specify name(s) of the algorithm(s)")
        raise SystemExit(0)
    
    if not args.path:
        print("Please specify path(s) of the algorithm(s)")
        raise SystemExit(0)

    if len(args.name) != len(args.path):
        print("Oops! The number of names does not equal to the number of paths.")
        raise SystemExit(0)

    current_algorithms = config.ALGORITHMS
    for i in range(len(args.name)):
        # compile algorithm to bitcode and store
        arg1 = "src_to_bc"
        arg2 = 'input="' + args.path[i] + '"'
        arg3 = 'output="' + args.name[i] + '.bc"'
        subprocess.run(["make", arg1, arg2, arg3], stdout=subprocess.PIPE, text=True, cwd="../../algorithms")
        # TODO: what if make fails?

        # add tag to config.py
        if args.name[i] not in current_algorithms:
            current_algorithms.append(args.name[i])
            print("Successfully add " + args.name[i])
        else:
            print("Successfully overwrite " + args.name[i])

    algorithm_line = json.dumps(current_algorithms)
    algorithm_line = "ALGORITHMS = " + algorithm_line + "\n"

    with open("config.py", "r") as f:
        lines = f.readlines()
    with open("config.py", "w") as f:
        for line in lines:
            if "ALGORITHMS" in line:
                f.write(algorithm_line)
            else:
                f.write(line)

elif args.delete:
    # TODO: check if file exist
    if not args.name:
        print("Please specify name(s) of the algorithm(s)")
        raise SystemExit(0)
    
    # delete specific algorithm(s)
    algo_bc_arr = ['{0}.bc'.format(args.name[i]) for i in range(len(args.name))]
    algo_bc = " ".join(algo_bc_arr)

    subprocess.run(["rm", algo_bc], stdout=subprocess.PIPE, text=True, cwd="../../algorithms")
    
    current_algorithms = config.ALGORITHMS

    # delete tag(s) in config.py
    delete_algos = []
    for name in args.name:
        if name not in current_algorithms:
            print("Oops!" + name + "is not a valid algorithm name")
        else:
            delete_algos.append(name)

    res_algos = [algo for algo in current_algorithms if algo not in delete_algos]

    algorithm_line = json.dumps(res_algos)
    algorithm_line = "ALGORITHMS = " + algorithm_line + "\n"

    with open("config.py", "r") as f:
        lines = f.readlines()
    with open("config.py", "w") as f:
        for line in lines:
            if "ALGORITHMS" in line:
                f.write(algorithm_line)
            else:
                f.write(line)

    delete_algos_str = " ".join(delete_algos)
    print("Successfully delete " + delete_algos_str)
elif args.clear:
    # delete all algorithms
    arg = "clean"
    subprocess.run(["make", arg], stdout=subprocess.PIPE, text=True, cwd="../../algorithms")

    empty_algos = []
    algorithm_line = json.dumps(empty_algos)
    algorithm_line = "ALGORITHMS = " + algorithm_line + "\n"

    with open("config.py", "r") as f:
        lines = f.readlines()
    with open("config.py", "w") as f:
        for line in lines:
            if "ALGORITHMS" in line:
                f.write(algorithm_line)
            else:
                f.write(line)
    print("Successfully delete all algorithms")
elif args.list:
    print("\n".join(config.ALGORITHMS))
else:
    print("Please specify action (add/delete/list)")

