PROJ_ROOT := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/..
BUILD_DIR := ${PROJ_ROOT}/build
SRC_DIR := ${PROJ_ROOT}/src
SCRIPTS_DIR := ${SRC_DIR}/tools/scripts
CONF_DIR := ${SRC_DIR}/tools/config

CC=clang++

SHELL := /bin/bash

MAKE := make --no-print-directory

# target settings
BUILD_MODES := Debug Release RelWithDebInfo
TARGET_ARCHS := x86_64

# misc.
NLCPU := ${shell nproc}

# misc
CPU_TOPOLOGY := ${SCRIPTS_DIR}/cpu-topology.py
CPU_TOPOLOGY_MAPS := \
	${SRC_DIR}/include/util/cpu_topology_host.h \
	${SCRIPTS_DIR}/topology/cpu_topology.py

# set compilers
.EXPORT_ALL_VARIABLES:
CC = /usr/bin/clang
CXX = /usr/bin/clang++

# make targets
.PHONY: all
all: make

.PHONY: cmake
cmake:
	@for ARCH in $(TARGET_ARCHS); do ( : ; \
		for MODE in $(BUILD_MODES); do ( : ; \
			mkdir -p ${BUILD_DIR}/$$MODE-$$ARCH; \
			cd ${BUILD_DIR}/$$MODE-$$ARCH; \
			TARGET_ARCH=$$ARCH cmake ${SRC_DIR} \
				-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
				-DCMAKE_BUILD_TYPE=$$MODE; \
		); done; \
	); done
	exit

.PHONY: make
make: ${CPU_TOPOLOGY_MAPS}
	@if [[ ! -e ${BUILD_DIR}/CMakeCache.txt ]]; then make cmake; fi
	@for ARCH in $(TARGET_ARCHS); do ( : ; \
		for MODE in $(BUILD_MODES); do ( : ; \
			echo "-----------------"; \
			echo $$ARCH "-" $$MODE; \
			cd ${BUILD_DIR}/$$MODE-$$ARCH; \
			$(MAKE)  -j${NLCPU}; \
		); done; \
	); done

	make ${BUILD_DIR}/Release-x86_64/lib/main.bc

# make main.bc
${BUILD_DIR}/Release-x86_64/lib/main.bc:
	$(CC) -c -emit-llvm -I${SRC_DIR}/include -I${SRC_DIR}/util/pci-ring-buffer/include -I${BUILD_DIR}/Release-x86_64/flatbuffer/include -flimit-debug-info -fno-rtti -O2 -pthread -std=gnu++14 ${SRC_DIR}/include/algorithm/main.cc -o ${BUILD_DIR}/Release-x86_64/lib/main.bc

${SRC_DIR}/include/util/cpu_topology_host.h:
	${CPU_TOPOLOGY} /proc/cpuinfo c > $@

${SCRIPTS_DIR}/topology/cpu_topology.py:
	mkdir -p ${SCRIPTS_DIR}/topology
	${CPU_TOPOLOGY} /proc/cpuinfo python > $@

.PHONY: clean
clean:
	@for ARCH in $(TARGET_ARCHS); do ( : ; \
		for MODE in $(BUILD_MODES); do ( : ; \
			if [ -e ${BUILD_DIR}/$$MODE-$$ARCH/Makefile ]; \
			then \
				cd ${BUILD_DIR}/$$MODE-$$ARCH; \
				make clean; \
			fi; \
		); done; \
	); done

TAGS:
	etags `find . -type f -name '*.cc' -or -name '*.cpp'  -or -name '*.c' -or -name '*.h'`

.PHONY: cscope
cscope:
	find ${SRC_DIR} -name '*.cc'  \
	            -or -name '*.cpp' \
	            -or -name '*.c'   \
	            -or -name '*.h' > cscope.files
	cscope -b -q -R

.PHONY: distclean
distclean:
	rm -rf ${BUILD_DIR}
	rm -f  ${CPU_TOPOLOGY_MAPS}
	rm -f cscope.out tags

.PHONY: help
help:
	@cat README.md

.PHONY: format
format:
	./format.sh

