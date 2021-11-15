#!/usr/bin/env python3

# SPDX-License-Identifier: MIT

import math
import os
import sys
import subprocess
import time
import glob
import grp
from shlex import split

ROOT = os.path.abspath(os.path.dirname(__file__))


class color:
    PURPLE = '\033[95m'
    CYAN = '\033[96m'
    DARKCYAN = '\033[36m'
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m'


class IostatLog(object):
    def __init__(self, outdir, log=False):
        self.__outdir = outdir
        self.__script = os.path.join(ROOT, "iobench_stat.sh")
        self.__log = log
        self.__sample = 0.1

    def __enter__(self):
        if (self.__log):
            print("Logging ...")
            shargs = (split("%s --outdir %s --sample %d" % (self.__script,
                                                            self.__outdir, self.__sample)))
            print(shargs)
            sh(shargs, out=None, shell=False)

    def __exit__(self, typ, value, traceback):
        if (self.__log):
            sh(split("%s --stop" % self.__script), out=None, shell=False)
            print("Logging ... done")


class CollectlLog(object):
    def __init__(self, outdir, log=False):
        self.__outdir = outdir
        self.__log = log

    def __enter__(self):
        if (self.__log):
            print("Logging ...")
            cmd = "collectl -i 0.1 > %s &" % (self.__outdir)
            print(cmd)
            os.system(cmd)

    def __exit__(self, typ, value, traceback):
        if (self.__log):
            sh(split("killall collectl"), out=None, shell=False)
            print("Logging ... done")


def next_power_of_2(x):
    return 1 if x == 0 else 2 ** math.ceil(math.log2(x))


def previous_power_of_2(x):
    if x == 0:
        return 1
    elif x == 1:
        return 1
    else:
        return 2 ** math.ceil(math.log2(x) - 1)


def sh(cmd, out=None, shell=True):
    print(";; %s" % cmd)
    p = subprocess.Popen(cmd, shell=shell, stdout=out, stderr=out)
    p.wait()
    return p


def run_sshpass(debug, password, user, server, *args):
    assert (not any('"' in str(a) or ' ' in str(a) for a in args))

    sargs = ['%s' % str(a) for a in args]
    cmd = ["sshpass", "-p", password, "ssh", "%s@%s" % (user, server), "\""] + sargs + ["\""]
    if debug:
        return
    cmd_string = " ".join(cmd)
    print(cmd_string)
    os.system(cmd_string)


def run_background(debug, *args):
    assert (not any('"' in str(a) or ' ' in str(a) for a in args))

    sargs = ['"%s"' % str(a) for a in args]
    if debug:
        return
    cmd = " ".join(sargs) + ' &'
    print(cmd)
    os.system(cmd)


def run_background_output(debug, out_file, err_file, *args):
    assert (not any('"' in str(a) or ' ' in str(a) for a in args))

    sargs = ['"%s"' % str(a) for a in args]
    if debug:
        return
    cmd = " ".join(sargs) + ' > ' + out_file + ' 2> ' + err_file + ' &'
    print(cmd)
    os.system(cmd)


def run(debug, *args):
    assert (not any('"' in str(a) or ' ' in str(a) for a in args))

    sargs = ['"%s"' % str(a) for a in args]
    if debug:
        return
    cmd = " ".join(sargs)
    print(cmd)
    os.system(cmd)


def run_output(debug, out_file, *args):
    assert (not any('"' in str(a) or ' ' in str(a) for a in args))

    sargs = ['"%s"' % str(a) for a in args]
    if debug:
        return
    cmd = " ".join(sargs) + ' 2>&1 | tee ' + out_file
    print(cmd)
    os.system(cmd)


def mkdirp(pn, group=None):
    if not os.path.exists(pn):
        os.makedirs(pn)
        os.chmod(pn, 0o777)
        # if not group is None:
        #    gid = grp.getgrnam(group).gr_gid
        #    #os.chown(pn, -1, gid)

def parsePerfNumber(input):
    output = int(input.replace(",", ""))
    return output
