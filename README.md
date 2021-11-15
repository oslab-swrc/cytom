# Cytom: Processing Billion-Scale Evolving Graphs on a Single Machine

This repository is the main codebase for the Cytom project and builds a single
binary that includes both the graph reader (or generator) as well as our engine
for processing evolving graphs, Cytom.

# Prerequisites
* Cmake 3.12 or newer
* A recent clang (6.0+)
* [Googletest](https://github.com/google/googletest) needs to be installed, this
can be done following [these
instructions](https://github.com/google/googletest/blob/master/googletest/README.md).
* [Flatbuffers](https://google.github.io/flatbuffers/) need to be installed,
  [instructions](https://google.github.io/flatbuffers/md__building.html)

# Build
To build Cytom, follow these steps:
```
cd src
make
```
This will build the binaries of Cytom, in a _release_ and _debug_
configuration into the _build_-folder.

# Datasets and Preparation
In the paper, we use six datasets (3 real-world, 3 synthetic):
* uk-2007: http://law.di.unimi.it/webdata/uk-2007-05/
* Twitter: http://an.kaist.ac.kr/traces/WWW2010.html
* Livejournal: https://snap.stanford.edu/data/soc-LiveJournal1.html
* Orkut: https://snap.stanford.edu/data/com-Orkut.html
* rmat-22, generated using the internal RMAT generator with the same parameters
  as the graph500 benchmark.

These graphs (and a number of pre-configured RMAT graphs) are already included
in the configuration of Cytom.
If you wish to add another graph dataset, it has to be added in the
configuration files.
This is done adding the appropriate settings in `config/config.py` (i.e., the
variables `GRAPH_SETTINGS_RMAT`, `INPUT_FILE`, and `GRAPH_SETTINGS_DELIM`),
following a similar pattern as the other graphs.

## Binary Input
For faster ingestion, Cytom can read graphs from a binary format (the same as
GridGraph, in case one happens to use both for benchmarking purposes).
We provide a script to convert txt-based graphs to the binary format:
```
cd src/scripts
./convert_graph.py --input /data/graph/input/livejournal/soc-LiveJournal1.txt --output /data/graph/input/livejournal/livejournal.bin
```

## Add/Delete Algorithms in ALgorithm Store
We provide a script to submit new algorithms, compile it and store its bitcode:
```
./run.py --algorithm-store --add --name cc bfs sssp --path ../examples/cc.cc ../examples/bfs.cc ../examples/sssp.cc
```
To list all existing algorithms:
```
./run.py --algorithm-store --list
```

## To profile algorithms
```
./run.py --profile
```

## Running jobs sequentially in Cytom
To analyze three algorithms (e.g., connected components, breath-first search, single-source shortest path) on three specific
graphs (e.g., livejournal, orkut, twitter) sequentially, use the following format:
```
./run.py --sequential --algorithm cc,bfs,sssp --graph livejournal,orkut,twitter-small
```
To run the above jobs concurrently:
```
./run.py --concurrent --algorithm cc,bfs,sssp --graph livejournal,orkut,twitter-small
```

All runs of Cytom generate a logfile for further analysis.

# Executing Cytom
Cytom uses a number of configuration files to allow for automatically running a
range of different scenarios for evaluation purposes.
These files are located in `src/scripts/plots/plotconfig` and allow an
automated parsing of the Cytom's log files to generate the evaluation graphs as
seen in our paper.

## Analyzing Results
To run a benchmark suite of algorithms and datasets as well as a quick analysis
of these, the `run` script includes an `analyze` mode that automates parsing
and, if enabled, the computation of standard deviation and averages.
This is based on the aforementioned configuration files:
```
cd src/scripts
./run.py --plot-config algorithm-performance
./run.py --analyze --plot-config algorithm-performance
```
This generates a file format that can be used as the direct input to gnuplot,
the file is stored at `src/scripts/plots/plotdata`.

# Authors
* Steffen Maass [steffen.maass@gatech.edu](mailto:steffen.maass@gatech.edu)
* Mingyu Guan [mingyu.guan@gatech.edu](mailto:mingyu.guan@gatech.edu)
* Mohan Kumar [mohankumar@gatech.edu](mailto:mohankumar@gatech.edu)
* Taesoo Kim [taesoo@gatech.edu](taesoo@gatech.edu)

# Citation
```
@phdthesis{maass:thesis,
  title        = {{S}ystems {A}bstractions for {B}ig {D}ata {P}rocessing on a {S}ingle {M}achine},
  author       = {Steffen Maass},
  school       = {G}eorgia {I}nstitute of {T}echnology},
  year         = 2019,
  month        = aug,
}
```
