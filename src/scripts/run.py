#!/usr/bin/env python3

# SPDX-License-Identifier: MIT

##################
# Use this script to start the Cytom server
##################

import socket
import sys
import argparse
# import pickle
import os
import json
import subprocess
import psutil
import shlex
import time
import math
import datetime
import matplotlib.pyplot as plt
from multiprocessing import Process, Queue
from sklearn.linear_model import LinearRegression

import config
import build
import utils

class SingleGraphConfig:
	def __init__(self, parent, run, algorithm, graph, insertion_batch, tile_batch,
			traversal, dynamic_compaction, tile_distribution_strategy, in_memory_ingestion,
			enable_interactive_algo, enable_writing_edges, enable_edge_apis,
			enable_algorithm_reexecution, enable_reading_edges, enable_writing_results,
			deletions, enable_selective_scheduling, delta):
		self.run = run
		self.parent = parent
		self.algorithm = algorithm
		self.graph = graph
		self.insertion_batch = insertion_batch
		self.tile_batch = tile_batch
		self.traversal = traversal
		self.dynamic_compaction = dynamic_compaction
		self.tile_distribution_strategy = tile_distribution_strategy
		self.in_memory_ingestion = in_memory_ingestion
		self.enable_interactive_algo = enable_interactive_algo
		self.enable_writing_edges = enable_writing_edges
		self.enable_reading_edges = enable_reading_edges
		self.enable_edge_apis = enable_edge_apis
		self.enable_algorithm_reexecution = enable_algorithm_reexecution
		self.enable_writing_results = enable_writing_results
		self.deletions = deletions
		self.enable_selective_scheduling = enable_selective_scheduling
		self.delta = delta

		self.generateArguments()

	def generateArguments(self):
		input_file = "~"
		generator = ""
		max_vertices = 0
		count_edges = 0

		if self.graph in config.GRAPH_SETTINGS_DELIM:
			max_vertices = config.GRAPH_SETTINGS_DELIM[self.graph]["vertices"]
			generator = "binary"
			input_file = config.INPUT_FILE[self.graph]["binary"]
			count_edges = config.GRAPH_SETTINGS_DELIM[self.graph]["edges"]
		elif self.graph in config.GRAPH_SETTINGS_RMAT:
			max_vertices = config.GRAPH_SETTINGS_RMAT[self.graph]["vertices"]
			count_edges = config.GRAPH_SETTINGS_RMAT[self.graph]["edges"]
			generator = "rmat"

		if self.parent.args.max_edges > 0:
			count_edges = min(count_edges, self.parent.args.max_edges)

		meta_tiles_per_dimension = 0
		tile_distribution_strategy = self.tile_distribution_strategy
		if self.tile_distribution_strategy == "hierarchical-tile-distributor":
			tile_distribution_strategy = "tile-distributor"
			count_tiles = max_vertices / config.VERTICES_PER_TILE
			meta_tiles_per_dimension = min(utils.previous_power_of_2(count_tiles / 4),
			                             config.HIERACHICAL_TILE_DISTRIBUTION_MAX_META_TILES_PER_DIMENSION)
			meta_tiles_per_dimension = max(meta_tiles_per_dimension, 1)

			# enable_adaptive_scheduling = "1" if config.ALGORITHM_ENABLE_SELECTIVE_SCHEDULING[
			#   self.algorithm] else "0"

		enable_adaptive_scheduling = self.enable_selective_scheduling

		count_meta_tile_managers = 6
		count_edge_inserters = 6
		if self.parent.args.disable_algorithm:
			count_meta_tile_managers = 24
			count_edge_inserters = 10

		algo_start_vertex = config.DEFAULT_ALGORITHM_START_VERTEX 
		if self.parent.args.algorithm_start_vertex != algo_start_vertex:
			algo_start_vertex = self.parent.args.algorithm_start_vertex

		algo_start = self.parent.args.algorithm_threshold * count_edges

		if self.parent.args.algorithm_threshold == -1.0:
			algo_start = config.MIN_COUNT_EDGES

		algo_start = int(round(algo_start))

		self.arguments = {
			"max-vertices": max_vertices,
			"vertices-per-tile": config.VERTICES_PER_TILE,
			"batch-insertion-count": self.insertion_batch,
			"meta-tile-manager-count": count_meta_tile_managers,
			"count-edges": count_edges,
			"edge-inserters-count": count_edge_inserters,
			"use-dynamic-compaction": self.dynamic_compaction,
			"count-parallel-compaction": 48,
			"enable-edge-apis": self.enable_edge_apis,
			"count-algorithm-executors": 24,
			"algorithm-tile-batching": self.tile_batch,
			"algorithm-iterations": self.parent.args.max_iterations,
			"path-perf-events": config.PERF_EVENTS_ROOT,
			"enable-perf-event-collection": "1" if self.parent.args.collect_perf_events else "0",
			"enable-static-algorithm-before-compaction": "0",
			"enable-static-algorithm-after-compaction": "0",
			"enable-rewrite-ids": config.DEFAULT_REWRITE_IDS,
			"traversal": self.traversal,
			"generator": generator,
			"binary-input-file": input_file,
			"algorithm": self.algorithm, 
			"deletion-percentage": self.deletions,
			"enable-interactive-algorithm": self.enable_interactive_algo,
			"enable-lazy-algorithm": "0",
			"count-algorithm-appliers": 24,
			"enable-adaptive-scheduling": enable_adaptive_scheduling,
			"tile-distribution-strategy": tile_distribution_strategy,
			"path-to-edges-output": config.PATH_EDGES_OUTPUT,
			"enable-writing-edges": self.enable_writing_edges,
			"enable-in-memory-ingestion": self.in_memory_ingestion,
			"td-count-meta-tiles-per-dimension": meta_tiles_per_dimension,
			"algo-start-id": algo_start_vertex,
			"enable-reading-edges": self.enable_reading_edges,
			"path-to-edges-input": config.PATH_EDGES_INPUT,
			"enable-algorithm-reexecution": self.enable_algorithm_reexecution,
			"path-to-results": config.PATH_RESULTS,
			"enable-write-results": self.enable_writing_results,
			"delta": self.delta,
			"start-algorithm-count": algo_start,
			"output-detailed-tile-stats": "1" if self.parent.args.detailed_tile_stats else "0",
		}

	def generateLogFileName(self):
		filename = "log"
		if self.parent.args.prefix is not None:
			filename += "_" + self.parent.args.prefix

		tile_distribution_strategy = self.arguments["tile-distribution-strategy"]
		if tile_distribution_strategy == "tile-distributor":
			if self.arguments["td-count-meta-tiles-per-dimension"] == 0:
				tile_distribution_strategy = "flat-tile-distributor"

		arg_list = [
			self.graph,
			self.arguments["algorithm"],
			self.arguments["batch-insertion-count"],
			self.arguments["use-dynamic-compaction"],
			self.arguments["algorithm-tile-batching"],
			self.arguments["traversal"],
			tile_distribution_strategy,
			self.arguments["deletion-percentage"],
			self.arguments["enable-interactive-algorithm"],
			self.arguments["enable-in-memory-ingestion"],
			self.arguments["enable-writing-edges"],
			self.arguments["enable-reading-edges"],
			self.arguments["enable-edge-apis"],
			self.arguments["enable-algorithm-reexecution"],
			self.arguments["enable-write-results"],
			self.arguments["enable-adaptive-scheduling"],
			self.arguments["delta"],
			self.run
		]

		if self.parent.args.perf_stat:
			arg_list = ["perf_stat"] + arg_list

		for argument in arg_list:
			filename += "_" + str(argument)
		filename += ".txt"
		return os.path.join(config.LOG_ROOT, filename)

	def genConfigStr(self):
		log_filename = self.generateLogFileName()

		config_str = ""
		args_count = 0
		for key, value in self.arguments.items():
			config_str += "--%s %s " % (key, value)

		return config_str

class GraphConfigGenerator:
	def __init__(self, args):
		self.args = args

		self.configs = ConfigLists(self.args)
		self.configs.generateConfigs()

	def genConfigs(self):
		configs = []
		# Loop over all options, generate set of runs.
		for run in self.configs.runs:
			for algorithm in self.configs.algorithms:
				for graph in self.configs.graphs:
					for insertion_batch in self.configs.insertion_batches:
						for tile_batch in self.configs.tile_batches:
							for traversal in self.configs.traversals:
								for dynamic_compaction in self.configs.dynamic_compactions:
									for tile_distribution_strategy in self.configs.tile_distribution_strategies:
										for in_memory_ingestion in self.configs.in_memory_ingestion:
											for interactive_algo in self.configs.interactive_algo:
												for enable_write_edges in self.configs.enable_write_edges:
													for enable_edge_api in self.configs.enable_edge_apis:
														for enable_algorithm_reexecution in self.configs.enable_algorithm_reexecution:
															for enable_reading_edges in self.configs.enable_reading_edges:
																for enable_writing_results in self.configs.enable_writing_results:
																	for deletions in self.configs.deletions:
																		for selective_scheduling in self.configs.selective_scheduling:
																			for delta in self.configs.delta:
																				single_config = SingleGraphConfig(self, run, algorithm,
																					graph, insertion_batch,
																					tile_batch, traversal,
																					dynamic_compaction,
																					tile_distribution_strategy,
																					in_memory_ingestion,
																					interactive_algo,
																					enable_write_edges,
																					enable_edge_api,
																					enable_algorithm_reexecution,
																					enable_reading_edges,
																					enable_writing_results,
																					deletions,
																					selective_scheduling,
																					delta)
																				configs.append(single_config)
		return configs

class ConfigLists:
	def __init__(self, args):
		self.args = args

	def generateConfigs(self):
		# submit new jobs - generate all arguments
		if self.args.algorithm is not None:
			self.algorithms = self.args.algorithm.split(",")
			self.algorithms = [x.strip() for x in self.algorithms]
		else:
			self.algorithms = config.ALGORITHMS

		# assign a default algorithm when the algorithms are disabled.
		if self.args.disable_algorithm:
			self.algorithms = ["bfs"]

		if self.args.graph is not None:
			self.graphs = self.args.graph.split(",")
			self.graphs = [x.strip() for x in self.graphs]
		else:
			self.graphs = config.GRAPHS

		if self.args.insertion_batch_size is not None:
			self.insertion_batches = []
			if isinstance(self.args.insertion_batch_size, str) and ".." in self.args.insertion_batch_size:
				split = self.args.insertion_batch_size.split("..")
				min_batch_size = int(split[0])
				max_batch_size = int(split[1])
			elif isinstance(self.args.insertion_batch_size,
							str) and "," in self.args.insertion_batch_size:
				split = self.args.insertion_batch_size.split(",")
				self.insertion_batches = [int(x.strip()) for x in split]
			else:
				min_batch_size = int(self.args.insertion_batch_size)
				max_batch_size = int(self.args.insertion_batch_size)

			if len(self.insertion_batches) == 0:
				i = min_batch_size
				while i <= max_batch_size:
					self.insertion_batches.append(i)
					i = i * 2
		else:
			self.insertion_batches = [config.DEFAULT_INSERTION_BATCH_SIZE]

		if self.args.tile_batch_size is not None:
			if isinstance(self.args.tile_batch_size, str) and ".." in self.args.tile_batch_size:
				split = self.args.tile_batch_size.split("..")
				min_batch_size = int(split[0])
				max_batch_size = int(split[1])
			else:
				min_batch_size = int(self.args.tile_batch_size)
				max_batch_size = int(self.args.tile_batch_size)

			i = min_batch_size
			self.tile_batches = []
			while i <= max_batch_size:
				self.tile_batches.append(i)
				i *= 2
		else:
			self.tile_batches = [config.DEFAULT_TILE_BATCH_SIZE]

		if self.args.enable_edge_apis:
			self.enable_edge_apis = ["0", "1"]
		else:
			self.enable_edge_apis = [config.DEFAULT_ENABLE_EDGE_APIS]

		if self.args.enable_algorithm_reexecution:
			self.enable_algorithm_reexecution = ["1"]
		else:
			self.enable_algorithm_reexecution = [config.DEFAULT_ALGORITHM_REEXECUTION]

		if self.args.enable_traversals:
			self.traversals = config.TRAVERSALS
		else:
			self.traversals = [config.DEFAULT_TRAVERSAL]

		if self.args.dynamic_compaction is not None:
			self.dynamic_compactions = ["0", "1"]
		else:
			self.dynamic_compactions = [config.DEFAULT_DYNAMIC_COMPACTION]

		if self.args.tile_distribution_strategy is not None:
			self.tile_distribution_strategies = config.TILE_DISTRIBUTION_STRATEGIES
		else:
			self.tile_distribution_strategies = [config.DEFAULT_TILE_DISTRIBUTION_STRATEGY]

		if self.args.in_memory_ingestion:
			self.in_memory_ingestion = ["1"]
		else:
			self.in_memory_ingestion = [config.DEFAULT_IN_MEMORY_INGESTION]

		if self.args.interactive_algo is not None:
			self.interactive_algo = ["0", "1"]
		else:
			self.interactive_algo = [config.DEFAULT_INTERACTIVE_ALGO]

		if self.args.disable_algorithm:
			self.interactive_algo = ["0"]

		if self.args.enable_write_edges:
			self.enable_write_edges = ["1"]
		else:
			self.enable_write_edges = [config.DEFAULT_WRITE_EDGES]

		if self.args.read_edges is not None:
			self.enable_reading_edges = [self.args.read_edges]
		else:
			self.enable_reading_edges = [config.DEFAULT_READ_EDGES]

		if self.args.enable_writing_results:
			self.enable_writing_results = ["1"]
		else:
			self.enable_writing_results = [config.DEFAULT_WRITE_RESULTS]

		if self.args.enable_deletions:
			self.deletions = ["0.01", "0.05", "0.10"]
		else:
			self.deletions = ["0.00"]

		if self.args.override_selective_scheduling:
			self.selective_scheduling = ["0", "1"]
		elif self.args.disable_selective_scheduling:
			self.selective_scheduling = ["0"]
		else:
			self.selective_scheduling = ["1"]

		if self.args.enable_delta:
			self.delta = config.DELTAS
		else:
			self.delta = [config.DEFAULT_DELTA]

		self.runs = [i for i in range(int(self.args.runs))]

	def countConfigs(self):
		return len(self.runs) * len(self.algorithms) * len(self.graphs) * len(
			self.insertion_batches) * len(self.tile_batches) * len(self.traversals) * len(
			self.dynamic_compactions) * len(self.tile_distribution_strategies) * len(
			self.in_memory_ingestion) * len(self.interactive_algo) * len(self.enable_write_edges) * len(
			self.enable_reading_edges) * len(self.enable_edge_apis) * len(
			self.enable_algorithm_reexecution) * len(self.enable_writing_results) * len(
			self.deletions) * len(self.selective_scheduling) * len(self.delta)

def sched_func(algo_q, stat_q):
	scheduler = Scheduler(stat_q)

	exit = False
	while not exit:
		# check algo_q
		while True:
			try:
				new_algos = algo_q.get(timeout=1)
				if new_algos == 0:
					# prepare to exit, no more algos
					exit = True
				else:
					scheduler.add_algos(new_algos)
			except:
				# print("algo queue is empty, now launch scheduler..")
				break

		if scheduler.num_of_unexec_algos > 0:
			scheduler.schedule()
		else:
			time.sleep(1)

	# exit
	# print("--------Scheduler is existing..")
	scheduler.exit()


class Scheduler():
	def __init__(self, stat_q):
		self.unfit = True
		self.model_update = False

		self.model_X = config.model_X
		self.model_y = config.model_y

		# read profiler data from config
		self.profile_data = config.PROFILE_DATA

		self.task_id = 0
		self.dataset_algo_map = dict() # unexecute algos, <dataset, [algos]>
		self.num_of_unexec_algos = 0

		self.stat_q = stat_q
		self.predict_cur_cpu_usage = 0
		self.alive_tasks = dict()  # <task_id, (process, predict_cpu_usage)>

		self.model = LinearRegression() # Features: sum_profile_cpu_usage, max_profile_cpu_usage, avg_profile_cpu_usage, num_of_algos

		if self.model_X and self.model_y:
			self.model.fit(self.model_X, self.model_y)
			self.unfit = False
	

	def add_algos(self, algos):
		for algo in algos:
			if algo.graph in self.dataset_algo_map:
				self.dataset_algo_map[algo.graph].append(algo)
			else:
				self.dataset_algo_map[algo.graph] = [algo]

			self.num_of_unexec_algos += 1

	def check_stat_q(self):
		# check stat_q: update scheduler if new data available in queue
		X = []
		y = []
		while True:
			try:
				new_data = self.stat_q.get_nowait()
				profile_single = new_data[0]
				if profile_single:
					algorithm = new_data[1]
					graph = new_data[2]
					result_cpu_usage = new_data[3]
					running_time = new_data[4]
					self.saveProfileData(algorithm, graph, result_cpu_usage, running_time)
				else:
					sum_profile_cpu_usage = new_data[1]
					# max_profile_cpu_usage = new_data[2] 
					# avg_profile_cpu_usage = new_data[3]
					# num_of_algos = new_data[4]
					result_cpu_usage = new_data[2]

					# X.append([sum_profile_cpu_usage, max_profile_cpu_usage, avg_profile_cpu_usage, num_of_algos])
					X.append([sum_profile_cpu_usage])
					y.append(result_cpu_usage)
			except:
				# print("stat queue is empty now")
				break

		if X and y:
			self.fitTrueData(X, y)

	def schedule(self):
		'''
		Args:
			'unsched_algos': a list of SingleGraphConfig
		
		Method: remove algos in dataset_algo_map which are executed successfully
		'''

		self.check_stat_q()

		# print("-----------[Now schedule]:")

		# sort algos by profile_cpu_usage in each dataset
		for dataset in self.dataset_algo_map:
			algos = self.dataset_algo_map[dataset]
			algo_names = [algo.algorithm for algo in algos]
			profile_cpu_usage_list = self.get_profile_cpu_usage(algo_names, dataset)
			self.dataset_algo_map[dataset] = [algo for _,algo in sorted(zip(profile_cpu_usage_list,algos), key=lambda pair: pair[0], reverse=True)]
			
			# print(dataset+":", ','.join([algo.algorithm for algo in self.dataset_algo_map[dataset]]))

		# schedule as level increases
		continue_sched = True
		level = 0 # schedule 1/(2^level) algos in a dataset group
		while continue_sched:
			untouch_dataset = 0
			for dataset in self.dataset_algo_map:
				self.update_predict_cur_cpu_usage()
				runtime_cpu_usage = psutil.cpu_percent(interval=0.5)
				current_cpu_usage = max(self.predict_cur_cpu_usage, runtime_cpu_usage)
				# print("predict current cpu usage: ", self.predict_cur_cpu_usage, "runtime cpu usage:", runtime_cpu_usage)
				if self.num_of_unexec_algos == 0 or current_cpu_usage >= 90.0:
					continue_sched = False
					break
				if not self.dataset_algo_map[dataset]:
					# no algos of dataset 
					untouch_dataset += 1
					continue

				dataset_algos = self.dataset_algo_map[dataset]
				num_of_dataset_algos = len(dataset_algos)
				stride = num_of_dataset_algos - level
				if stride <= 0:
					untouch_dataset += 1
					continue

				exec_idxes = []
				start_idx = 0
				while start_idx < num_of_dataset_algos:
					end_idx = min(start_idx+stride, num_of_dataset_algos)
					sched_algos = dataset_algos[start_idx:end_idx]
					# avail, sum_profile_cpu_usage, max_profile_cpu_usage, avg_profile_cpu_usage, predict_cpu_usage = self.check_cpu_usage(sched_algos, dataset)
					avail, sum_profile_cpu_usage, predict_cpu_usage = self.check_cpu_usage(sched_algos, dataset)
					if avail:
						algo_names_list = [algo.algorithm for algo in sched_algos]
						task_config_strs = [algo.genConfigStr() for algo in sched_algos]
						
						# spawn a new process to execute task
						# p = Process(target=execute_task, args=(self.stat_q, self.task_id, algo_names_list, task_config_strs, sum_profile_cpu_usage, max_profile_cpu_usage, avg_profile_cpu_usage, predict_cpu_usage, "log_tmp.txt", dataset)) # TODO: assign different logfile to each task
						p = Process(target=execute_task, args=(self.stat_q, self.task_id, algo_names_list, task_config_strs, sum_profile_cpu_usage, predict_cpu_usage, "log_tmp.txt", dataset))
						print("-------Now launch task", self.task_id, ":", algo_names_list, "on", dataset)
						# print("Predict CPU usage: ", predict_cpu_usage)
						p.start()

						# update
						self.alive_tasks[self.task_id] = (p, predict_cpu_usage)
						self.predict_cur_cpu_usage += predict_cpu_usage
						exec_idxes += list(range(start_idx,end_idx)) 
						self.num_of_unexec_algos -= len(sched_algos)
						self.task_id += 1

						start_idx += stride
					else:
						start_idx += 1

				self.dataset_algo_map[dataset] = [self.dataset_algo_map[dataset][idx] for idx in range(num_of_dataset_algos) if idx not in exec_idxes]

			if untouch_dataset == len(self.dataset_algo_map):
				continue_sched = False
			level += 1

	def check_cpu_usage(self, algos, graph):
		algo_names = [algo.algorithm for algo in algos]

		num_of_algos = len(algos)
		profile_cpu_usage_list = self.get_profile_cpu_usage(algo_names, graph)
		sum_profile_cpu_usage = sum(profile_cpu_usage_list)
		# max_profile_cpu_usage = max(profile_cpu_usage_list)
		# avg_profile_cpu_usage = sum_profile_cpu_usage / num_of_algos
		if self.unfit:
			predict_cpu_usage = sum_profile_cpu_usage
		else:
			# predict_cpu_usage = self.model.predict([[sum_profile_cpu_usage, max_profile_cpu_usage, avg_profile_cpu_usage, num_of_algos]])[0]
			predict_cpu_usage = self.model.predict([[sum_profile_cpu_usage]])[0]

		self.update_predict_cur_cpu_usage()
		current_cpu_usage = max(self.predict_cur_cpu_usage, psutil.cpu_percent(interval=0.5))
		if current_cpu_usage + predict_cpu_usage <= 100.0:
			# return True, sum_profile_cpu_usage, max_profile_cpu_usage, avg_profile_cpu_usage, predict_cpu_usage
			return True, sum_profile_cpu_usage, predict_cpu_usage
		else:
			return False, sum_profile_cpu_usage, predict_cpu_usage

	def update_predict_cur_cpu_usage(self):
		for task, process in list(self.alive_tasks.items()):
			if process[0].is_alive():
				# join and check if still alive
				process[0].join(timeout=0)

				if not process[0].is_alive():
					# join successfully
					self.predict_cur_cpu_usage -= process[1]
					del self.alive_tasks[task]
			else:
				# p is joined, update
				self.predict_cur_cpu_usage -= process[1]
				del self.alive_tasks[task]

	def get_profile_cpu_usage(self, algo_names, graph):
		profile_graph_data = self.profile_data[graph]

		avg_graph_cpu_usage = self.get_avg_graph_cpu_usage(graph)

		# sum_cpu_usage = 0
		profile_cpu_usage_list = []
		for algo in algo_names:
			if algo in profile_graph_data:
				profile_cpu_usage_list.append(profile_graph_data[algo]["cpu_usage"])
			else:
				profile_cpu_usage_list.append(avg_graph_cpu_usage)

		return profile_cpu_usage_list

	def get_avg_graph_cpu_usage(self, graph):
		profile_graph_data = self.profile_data[graph]

		sum_graph_cpu_usage = 0
		for algorithm in profile_graph_data:
			sum_graph_cpu_usage += profile_graph_data[algorithm]["cpu_usage"]

		return sum_graph_cpu_usage / len(profile_graph_data)

	def fitTrueData(self, X, y):
		self.model_X = self.model_X + X
		self.model_y = self.model_y + y

		# TODO: batch fit
		self.model.fit(self.model_X, self.model_y)
		self.model_update = True
		if self.unfit:
			self.unfit = False

	def saveProfileData(self, algorithm, graph, result_cpu_usage, running_time):
		# add data
		if graph not in self.profile_data:
			self.profile_data[graph] = {}

		if algorithm not in self.profile_data[graph]:
			self.profile_data[graph][algorithm] = {}	

		self.profile_data[graph][algorithm]["cpu_usage"] = result_cpu_usage
		self.profile_data[graph][algorithm]["running_time"] = running_time

	def exit(self):
		start = time.time()
		while self.num_of_unexec_algos > 0:
			self.schedule()
			time.sleep(1)

		# join all alive process
		for task in self.alive_tasks:
			self.alive_tasks[task][0].join()

		end = time.time()
		# print("Total execution time(s):", end-start)

		# check stat_q last time
		self.check_stat_q()

		# save config
		profile_line = json.dumps(self.profile_data)
		profile_line = "PROFILE_DATA = " + profile_line + "\n"

		model_X_line = json.dumps(self.model_X)
		model_X_line = "model_X = " + model_X_line + "\n"

		model_y_line = json.dumps(self.model_y)
		model_y_line = "model_y = " + model_y_line + "\n"

		with open("config.py", "r") as f:
			lines = f.readlines()
		with open("config.py", "w") as f:
			for line in lines:
				if "PROFILE_DATA" in line:
					f.write(profile_line)
				elif "model_X" in line:
					f.write(model_X_line)
				elif "model_y" in line:
					f.write(model_y_line)
				else:
					f.write(line)

class AlgorithmStore():
	def __init__(self):
		self.algorithms = config.ALGORITHMS

	def connect(self, args):
		if args.add:
			self.add(args)
		elif args.delete:
			self.delete(args)
		elif args.clear:
			self.clear(args)
		elif args.list:
			print("\n".join(self.algorithms))
		else:
			print("-----------[AlgorithmStore]: Please specify action (add/delete/list)")

	def add(self, args):
		if not args.name:
			print("-----------[AlgorithmStore]: Please specify name(s) of the algorithm(s)")
			return

		if not args.path:
			print("-----------[AlgorithmStore]: Please specify path(s) of the algorithm(s)")
			return

		if len(args.name) != len(args.path):
			print("-----------[AlgorithmStore]: Oops! The number of names does not equal to the number of paths.")
			return

		for i in range(len(args.name)):
			# compile algorithm to bitcode and store
			arg1 = "src_to_bc"
			arg2 = 'input="' + args.path[i] + '"'
			arg3 = 'output="' + args.name[i] + '.bc"'
			subprocess.run(["make", arg1, arg2, arg3], stdout=subprocess.PIPE, text=True, cwd="../../algorithms")
			# TODO: what if make fails?

			# add tag to config.py
			if args.name[i] not in self.algorithms:
				self.algorithms.append(args.name[i])
				print("-----------[AlgorithmStore]: Successfully add " + args.name[i])
			else:
				print("-----------[AlgorithmStore]: Successfully overwrite " + args.name[i])

	def delete(self, args):
		# TODO: check if file exist
		if not args.name:
			print("-----------[AlgorithmStore]: Please specify name(s) of the algorithm(s)")
			return

		# delete specific algorithm(s)
		algo_bc_arr = ['{0}.bc'.format(args.name[i]) for i in range(len(args.name))]
		algo_bc = " ".join(algo_bc_arr)

		subprocess.run(["rm", algo_bc], stdout=subprocess.PIPE, text=True, cwd="../../algorithms")

		# delete tag(s) in config.py
		delete_algos = []
		for name in args.name:
			if name not in self.algorithms:
				print("-----------[AlgorithmStore]: Oops!" + name + "is not a valid algorithm name")
			else:
				delete_algos.append(name)

		res_algos = [algo for algo in self.algorithms if algo not in delete_algos]

		self.algorithms = res_algos
		delete_algos_str = " ".join(delete_algos)
		print("-----------[AlgorithmStore]: Successfully delete " + delete_algos_str)

	def clear(self, args):
		# delete all algorithms
		arg = "clean"
		subprocess.run(["make", arg], stdout=subprocess.PIPE, text=True, cwd="../../algorithms")

		self.algorithms = []
		print("-----------[AlgorithmStore]: Successfully delete all algorithms")

	def exit(self):
		algorithm_line = json.dumps(self.algorithms)
		algorithm_line = "ALGORITHMS = " + algorithm_line + "\n"

		with open("config.py", "r") as f:
			lines = f.readlines()
		with open("config.py", "w") as f:
			for line in lines:
				if "ALGORITHMS" in line:
					f.write(algorithm_line)
				else:
					f.write(line)

def clearCaches():
	cmd = "sudo ./startup-config"
	print(cmd)
	os.system(cmd)

def execute_task(stat_q, task_id, algo_names, configs, sum_profile_cpu_usage, predict_cpu_usage, logfile, graph, profile_single=False):
	'''
	args: 
	algo_names: list of algorithms to be executed
	configs: list of configs (as str) of algorithms to be executed
	'''
	algo_names_set = set(algo_names)

	# link all used algorithms with main.bc
	# llvm-link $(filter-out $@,$(MAKECMDGOALS)) -o link.bc
	args = ["llvm-link", config.RELEASE_MAIN]
	algorithm_set_bc_arr = ['{0}.bc'.format(algo) for algo in algo_names_set]
	args += algorithm_set_bc_arr
	link_output = "link_" + str(task_id) + ".bc"
	args += ["-o", link_output]
	p = subprocess.run(args, stdout=subprocess.PIPE, text=True, cwd="../../algorithms")

	# compose jobs in main func
	opt_load = "-load"
	pass_path = "../build/Release-x86_64/lib/libParallizeAlgorithm.so"
	pass_target = "-composeJobs"
	input_arg = link_output
	output_flag = "-o"
	pass_output = "task_" + str(task_id) +".bc"
	jobs_list = ["-jobs"] * (len(algo_names) * 2 - 1)
	jobs_list[0::2] = algo_names
	jobs_list.insert(0, "-jobs")
	p = subprocess.run(["opt", opt_load, pass_path, pass_target, input_arg, output_flag, pass_output] + jobs_list, stdout=subprocess.PIPE, text=True, cwd="../../algorithms")

	# TODO: delete link_idx.bc

	# bc to executable
	target = "bc_to_exe"
	link_bc = "link_bc=" + link_output
	task_bc = "task_bc=" + pass_output
	task_s = "task_s=task_" +  str(task_id) +".s"
	task_exe = "task_exe=exe_" +  str(task_id)
	p = subprocess.run(["make", target, link_bc, task_bc, task_s, task_exe], stdout=subprocess.PIPE, text=True, cwd="../../algorithms")

	# compose cmd with jobs
	executable = config.EXE + "_" +  str(task_id)
	cmd = executable + " " + " ".join(configs)
	# clearCaches()

	logfile = os.path.join(config.LOG_ROOT, logfile)
	# TODO: fix logfile by process.communicate()
	# cmd += "2>&1 | tee " + logfile # TODO: how to format an unique logfile for certain set of jobs? (overwrite if re-run)
	# execute_cmd(cmd)

	args = shlex.split(cmd)
	# print("execute cmd: ", " ".join(args))

	cpu_count = psutil.cpu_count()
	cpu_usage_sum = 0.0
	time_axis = []
	cpu_axis = []

	# pre_cpu_usage = psutil.cpu_percent()
	# profile_cpu_usage = self.get_profile_cpu_usage(algo_names, graph)
	
	start = time.time()
	p = psutil.Popen(args, stdout=subprocess.PIPE)
	while p.is_running() and p.status() != psutil.STATUS_ZOMBIE:
		# print("CPU Usage: ", str(p.cpu_percent(interval=0.5)/cpu_count)+"%")
		cur_cpu_usage = p.cpu_percent(interval=0.5)/cpu_count
		cpu_usage_sum += cur_cpu_usage
		time_axis.append(datetime.datetime.now())
		cpu_axis.append(cur_cpu_usage)

	end = time.time()
	running_time = end - start
	cpu_usage_avg = cpu_usage_sum / len(cpu_axis)
	print("------------------------------------------")
	if profile_single:
		print("Task", task_id, "has finished: ", algo_names, "on", graph)
	else:
		print("Task", task_id, "has finished: ", algo_names, "on", graph)
	
	# rm executable
	p = subprocess.run(["rm", executable], stdout=subprocess.PIPE, text=True)

	# put data in queue
	if profile_single:
		stat_q.put([profile_single, algo_names[0], graph, cpu_usage_avg, running_time])
	else:
		stat_q.put([profile_single, sum_profile_cpu_usage, cpu_usage_avg])

class CytomManager():
	def __init__(self):
		self.algo_q = Queue() # for algos to schedule
		self.stat_q = Queue() # for profile and SGD data

		self.algorithm_store = AlgorithmStore()

		# launch scheduler process
		self.scheduler_process = Process(target=sched_func, args=(self.algo_q, self.stat_q))
		self.scheduler_process.start()
		self.launched_processes = []
		
		self.job_submitted_time = []

		# check build & LOGROOT
		self.setup()

	def setup(self):
		# check build
		# make_dir = config.SRC_ROOT
		# make_cmd = "make;"
		# cmd = "cd %s;%s" % (make_dir, make_cmd)
		# os.system(cmd)

		# mkdir for LOGs
		utils.mkdirp(config.LOG_ROOT)

	def submit_algos(self, algos):
		'''
		Args:
			'algos': a list of SingleGraphConfig
		'''
		
		self.job_submitted_time.append(datetime.datetime.now())
		self.algo_q.put(algos)

	def connect_algorithm_store(self, args):
		self.algorithm_store.connect(args)

	def profile_all(self, algos):
		'''
		Args:
			'jobs': a list of SingleGraphConfig
		'''

		task_id = 0
		for algo in algos:
			algo_names = [algo.algorithm]
			configs = [algo.genConfigStr()]
			execute_task(self.stat_q, task_id, algo_names, configs, 0, 0, "log_tmp.txt", algo.graph, profile_single=True)
			task_id += 1

	def exit(self):
		self.algorithm_store.exit()

		self.algo_q.put(0)

		self.scheduler_process.join()

def run_cytom(args):
	cytom_manager = CytomManager()
	if args.sequential or args.profile:
		generator = GraphConfigGenerator(args)
		algos = generator.genConfigs()
		cytom_manager.profile_all(algos)
	elif args.concurrent:
		generator = GraphConfigGenerator(args)
		algos = generator.genConfigs()
		start = time.time()
		cytom_manager.submit_algos(algos)
	elif args.algorithm_store:
		 cytom_manager.connect_algorithm_store(args)
	
	cytom_manager.exit()

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument("--clean", action="store_true", dest="clean", default=False)
	parser.add_argument("--distclean", action="store_true", dest="distclean", default=False)
	parser.add_argument("--print-only", action="store_true", dest="print_only", default=False)
	parser.add_argument("--prefix", default=None)
	parser.add_argument("--analyze", action="store_true", dest="analyze", default=False)
	parser.add_argument("--algorithm", default=None)
	parser.add_argument("--graph", default=None)
	parser.add_argument("--insertion-batch-size", default=None)
	parser.add_argument("--tile-batch-size", default=None)
	parser.add_argument("--enable-edge-apis", action="store_true", default=False)
	parser.add_argument("--enable-algorithm-reexecution", action="store_true", default=False)
	parser.add_argument("--enable-writing-results", action="store_true", default=False)
	parser.add_argument("--tile-distribution-strategy", default=None)
	parser.add_argument("--interactive-algo", default=None)
	parser.add_argument("--disable-algorithm", action="store_true", default=False)
	parser.add_argument("--enable-write-edges", action="store_true", default=False)
	parser.add_argument("--enable-deletions", action="store_true", default=False)
	parser.add_argument("--enable-delta", action="store_true", default=False)
	parser.add_argument("--override-selective-scheduling", action="store_true", default=False)
	parser.add_argument("--wait-after-execution", action="store_true", default=False)
	parser.add_argument("--read-edges", default=None)
	parser.add_argument("--in-memory-ingestion", action="store_true", default=False)
	parser.add_argument("--enable-traversals", action="store_true", default=False)
	parser.add_argument("--dynamic-compaction", default=None)
	parser.add_argument("--max-edges", default=0, type=int)
	parser.add_argument("--algorithm-threshold", default=0.0, type=float)
	parser.add_argument("--runs", default=1)
	parser.add_argument("--max-iterations", default=100, type=int)
	parser.add_argument("--debug", action="store_true", dest="debug", default=False)
	parser.add_argument("--iostat", action="store_true", dest="iostat", default=False)
	parser.add_argument("--collect-perf-events", action="store_true", default=False)
	parser.add_argument("--perf-stat", action="store_true", default=False)
	parser.add_argument("--plot-config", default="")
	parser.add_argument("--override-algorithm", default=None)
	parser.add_argument("--override-graph", default=None)
	parser.add_argument("--detailed-tile-stats", action="store_true", default=False)
	parser.add_argument("--disable-selective-scheduling", action="store_true", dest="disable_selective_scheduling", default=False)
	parser.add_argument("--algorithm-start-vertex", dest="algorithm_start_vertex", default=1, type=int)

	# algorithm-store subparser
	parser.add_argument("--algorithm-store", action="store_true", dest="algorithm_store", default=False) # algorithm_store commands
	parser.add_argument('--add', action='store_true')
	parser.add_argument('--delete', action='store_true')
	parser.add_argument('--clear', action='store_true') # delete all added algorithms
	parser.add_argument('--list', action='store_true') # list all added algorithm after other actions (add/delete)
	parser.add_argument('--name', nargs="*",
	                   help='name(s) of the new algorithm')
	parser.add_argument('--path', nargs="*",
	                   help='absolute path(s) to the new algorithm')

	# parser.add_argument("--submit", action="store_true", dest="submit", default=False) # submit new job(s)
	parser.add_argument('--sequential', action='store_true')
	parser.add_argument('--concurrent', action='store_true')
	parser.add_argument("--profile", action="store_true", dest="profile", default=False) # profile all jobs across all graphs datasets

	args = parser.parse_args()

	cpu_usage_sum = 0.0
	time_axis = []
	cpu_axis = []
	start = time.time()
	p = Process(target=run_cytom, args=[args])
	p.start()

	while p.is_alive():
		cur_cpu_usage = psutil.cpu_percent(interval=0.5)
		cpu_usage_sum += cur_cpu_usage
		time_axis.append(datetime.datetime.now())
		cpu_axis.append(cur_cpu_usage)
		p.join(timeout=0)

	end = time.time()
	running_time = end - start
	cpu_usage_avg = cpu_usage_sum / len(cpu_axis)

	if args.sequential or args.concurrent:
		print("----------Overall status--------------")
		print("Average CPU usage:", str(cpu_usage_avg)+"%")
		print("Running time:", str(running_time)+"s")
