#!/usr/bin/env python

# SPDX-License-Identifier: MIT

import os
import optparse
import config


def build(opts):
    make_dir = config.SRC_ROOT

    make_cmd = ""
    if opts.clean:
        make_cmd = "make clean;"
    elif opts.distclean:
        make_cmd = "make distclean;make cmake;"
    make_cmd += "make;"

    cmd = "cd %s;%s" % (make_dir, make_cmd)
    os.system(cmd)


if __name__ == "__main__":
    # parse options
    parser = optparse.OptionParser()
    parser.add_option("--clean", action="store_true", dest="clean",
                      default=False)
    parser.add_option("--distclean", action="store_true", dest="distclean",
                      default=False)
    (opts, args) = parser.parse_args()

    build(opts)
