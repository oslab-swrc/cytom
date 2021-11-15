#!/usr/bin/env python3

# SPDX-License-Identifier: MIT

import os
import socket


def join(*args):
    return os.path.normpath(os.path.join(*args))


HERE = os.path.abspath(os.path.dirname(__file__))
HOST = socket.gethostname()

# dir config
SRC_ROOT = join(os.path.dirname(__file__), "../")
BUILD_ROOT = join(SRC_ROOT, "../build")
SCRIPTS_ROOT = join(SRC_ROOT, "scripts")
DATA_ROOT = "/data/graph/"
LOG_ROOT = join(SRC_ROOT, "../log")
PERF_EVENTS_ROOT = join(SRC_ROOT, "../perf_events")
PLOT_ROOT = join(SCRIPTS_ROOT, "plots")
PLOT_CONFIGS = join(PLOT_ROOT, "plotconfig")
PLOT_DATA = join(PLOT_ROOT, "plotdata")

# add
ALG_ROOT = join(SRC_ROOT, "../algorithms")
EXE = join(ALG_ROOT, "exe")

# bin config
DEBUG_MAIN = join(BUILD_ROOT, "Debug-x86_64/lib/main/main")
# RELEASE_MAIN = join(BUILD_ROOT, "Release-x86_64/lib/main/main")
RELEASE_MAIN = join(BUILD_ROOT, "Release-x86_64/lib/main.bc")

# Activates the debug mode to run the debug binary and print fine-grained
# logging.
DEBUG_MODE = False

# Perf event collection.
# Whether to enable the perf event collection to trace JSON checkpoints.
ENABLE_PERF_EVENT_COLLECTION = False

# vertex-engine config
MAX_ITERATIONS = 100  # Number of iterations.

# input
ORIG_DATA_PATH = DATA_ROOT
ORIG_DATA_PATH_INPUT = join(ORIG_DATA_PATH, "input")
ORIG_DATA_PATH_OUTPUT = join(ORIG_DATA_PATH, "output")

PATH_EDGES_OUTPUT = join(SRC_ROOT, "..", "edges-output")
PATH_EDGES_INPUT = join(SRC_ROOT, "..", "edges-input")

DEFAULT_IN_MEMORY_INGESTION = 0 

DEFAULT_WRITE_EDGES = 0

DEFAULT_READ_EDGES = 0

TRAVERSALS = ["hilbert", "row-first", "column-first"]
DEFAULT_TRAVERSAL = "row-first"

ALGORITHMS = []

model_X = []
model_y = []
PROFILE_DATA = {}
# GRAPHS = ["rmat-16", "rmat-22", "rmat-24", "livejournal", "orkut", "twitter-small",
#           "twitter-full", "uk2007"]

GRAPHS = ["livejournal", "orkut", "twitter-small"]

TILE_DISTRIBUTION_STRATEGIES = ["static", "atomics", "tile-distributor",
                                "hierarchical-tile-distributor"]
DEFAULT_TILE_DISTRIBUTION_STRATEGY = "hierarchical-tile-distributor"
# DEFAULT_TILE_DISTRIBUTION_STRATEGY = "tile-distributor"

HIERACHICAL_TILE_DISTRIBUTION_MAX_META_TILES_PER_DIMENSION = 64

MIN_INSERTION_BATCH_SIZE = 1
MAX_INSERTION_BATCH_SIZE = 2 ** 24
DEFAULT_INSERTION_BATCH_SIZE = 2 ** 20

MIN_COUNT_EDGES = 0

MIN_TILE_BATCH_SIZE = 1
MAX_TILE_BATCH_SIZE = 2 ** 10
DEFAULT_TILE_BATCH_SIZE = 256

VERTICES_PER_TILE = 65536

DEFAULT_INTERACTIVE_ALGO = 1

DEFAULT_DYNAMIC_COMPACTION = 0

DEFAULT_ALGORITHM_REEXECUTION = 0

DEFAULT_ENABLE_EDGE_APIS = 1

DEFAULT_REWRITE_IDS = 0

PATH_RESULTS = join(SRC_ROOT, "..", "results-storage")
DEFAULT_WRITE_RESULTS = 0

DEFAULT_DELTA = 0.01
DELTAS = [0.00, 0.0025, 0.005, 0.01, 0.02, 0.04, 0.08, 0.16, 0.32, 0.64]

DEFAULT_ALGORITHM_START_VERTEX = 1

# define which algorithms need a weighted dataset
#ALGORITHM_WEIGHTED = {
#    "pagerank": False,
#    "bfs": False,
#    "cc": False,
#    "spmv": False,
#    "sssp": True,
#    "sswp": True,
#    "bp": True,
#    "tc": False
#}

#BFS_START_VERTEX = {
#    "twitter-full": 0,
#    "uk2007": 2587,
#    "livejournal": 1,
#    "orkut": 1,
#    "rmat-22": 0
#}

#ALGORITHM_ENABLE_SELECTIVE_SCHEDULING = {
#    "pagerank": False,
#    "pagerank-delta": True,
#    "bfs": True,
#    "bfs-async": True,
#    "connected-components": True,
#    "connected-components-async": True,
#    "spmv": False,
#    "sssp": True,
#    "sssp-async": True,
#    "sswp": True,
#    "bp": False,
#    "tc": False
#}

DELIM_TAB = "tab"
DELIM_COMMA = "comma"
DELIM_SPACE = "space"
DELIM_SEMICOLON = "semicolon"

GRAPH_SETTINGS_DELIM = {
    # "twitter-small": {"vertices": 2391579, "use_original_ids": False},
    # "twitter-full": {"vertices": 41652230, "use_original_ids": False},
    "twitter-small": {"vertices": 1334392, "edges": 46180342, "use_original_ids": False},
    "twitter-full": {"vertices": 41652230, "edges": 1468365182, "use_original_ids": False},
    "friendster": {"vertices": 65608366, "edges": 1806067135, "use_original_ids": False},
    "buzznet": {"vertices": 101169, "use_original_ids": False},
    "livejournal": {"vertices": 4847571, "edges": 68993773, "use_original_ids": True},
    "orkut": {"vertices": 3072441, "edges": 117185083, "use_original_ids": True},
    "uk2007": {"vertices": 105896555, "edges": 3738733599, "use_original_ids": False},
    "yahoo": {"vertices": 1413511394, "use_original_ids": True},
    "wdc2012": {"vertices": 3563602789, "use_original_ids": True},
    "wdc2014": {"vertices": 1724573718, "use_original_ids": True}
}

GRAPH_SETTINGS_RMAT = {
    "rmat-16": {"vertices": 2 ** 16, "edges": 2 ** 20, "use_original_ids": True},
    "rmat-20": {"vertices": 2 ** 20, "edges": 2 ** 24, "use_original_ids": True},
    "rmat-21": {"vertices": 2 ** 21, "edges": 2 ** 25, "use_original_ids": True},
    "rmat-22": {"vertices": 2 ** 22, "edges": 2 ** 26, "use_original_ids": True},
    "rmat-24": {"vertices": 2 ** 24, "edges": 2 ** 28, "use_original_ids": True},
    "rmat-27": {"vertices": 2 ** 27, "edges": 2 ** 31, "use_original_ids": True},
    "rmat-32-orig": {"vertices": 2 ** 32, "edges": 2 ** 36, "use_original_ids": True},
    "rmat-32": {"vertices": 2 ** 32, "edges": 10 ** 12, "use_original_ids": True},
    "rmat-33": {"vertices": 2 ** 33, "edges": 2 ** 40, "use_original_ids": True},
}

INPUT_FILE = {
    "test": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "test/test.csv"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "test/test.bin"),
    },
    "buzznet": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "buzznet/edges.csv"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "buzznet/edges.bin"),
    },
    "livejournal": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "livejournal/soc-LiveJournal1.txt"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "livejournal/livejournal.bin"),
    },
    "orkut": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "orkut/com-orkut.ungraph.txt"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "orkut/orkut.bin"),
    },
    "twitter-small": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "twitter-small/twitter_rv_small.net"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "twitter-small/twitter_rv_small-rewritten-new.bin"),
    },
    "twitter-full": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "twitter-full/twitter_rv.net"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "twitter-full/twitter_rv-rewritten.bin"),
    },
    "friendster": {
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "friendster/friendster-rewritten.bin"),
    },
    "uk2007": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "uk2007-05/uk2007.net"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "uk2007-05/uk2007.bin"),
    },
    "wdc2014": {
        "delim": os.path.join(ORIG_DATA_PATH_INPUT, "wdc14/hyperlink14.net"),
        "binary": os.path.join(ORIG_DATA_PATH_INPUT, "wdc14/hyperlink14.bin"),
    },
}
