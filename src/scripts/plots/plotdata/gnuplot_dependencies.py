#!/usr/bin/env python3

import os
import argparse
import re
import shutil


def parse_gnuplot_file(plotfile):
    with open(plotfile, "r") as f:
        for match in re.finditer("'(.*)'", f.read()):
            path = os.path.join(os.curdir, match.group(1))
            if os.path.isfile(path):
                yield os.path.realpath(path)


if __name__ == "__main__":
    # parse options
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    args = parser.parse_args()

    if args.input is None:
        exit(0)

    targets = parse_gnuplot_file(args.input)
    targetStr = "%s:" % args.input.replace(".gp", ".pdf")
    for target in targets:
        targetStr += " %s" % target
    print(targetStr)
