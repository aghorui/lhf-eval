#!/usr/bin/python3

import os
import sys
import random
import argparse
import time
from enum import Enum
from typing import IO, Generator, Set, Tuple

random.seed(time.time())

class RNGType(Enum):
	UNIFORM     = "uniform"
	NORMAL      = "normal"

class Operation(Enum):
	UNION = "U"
	INTERSECTION = "I"
	DIFFERENCE = "D"

persistent_set = {}

def generate_normal_int(
			mean: int,
			stddev: int,
			min_value: int | None = None,
			max_value: int | None = None) -> int:
		value = random.normalvariate(mean, stddev)
		value = abs(round(value))

		if min_value is not None:
			value = max(min_value, value)
		if max_value is not None:
			value = min(max_value, value)

		return value

def generate_normal_intptr_list(
			size: int,
			mean: int,
			stddev: int,
			min_value: int | None = None,
			max_value: int | None = None) -> Set[Tuple[int, int]]:
	count: int = 0
	ret: Set[Tuple[int, int]] = set()

	while count < size:
		n = (generate_normal_int(mean, stddev // 2, min_value, max_value),
			generate_normal_int(mean, stddev, min_value, max_value))
		if n not in ret:
			ret.add(n)
			count += 1

	return ret

def generate_int_set_random_uniform(universe_max: int, max_set_size: int) -> Set[Tuple[int, int]]:
	size: int = random.randint(0, max_set_size)
	nums = random.sample(range(0, universe_max + 1), size * 2)
	num_iter = iter(nums)
	return set(zip(num_iter, num_iter))

def generate_normal_set_internal(size = 20, universe_max = 200):
	data = generate_normal_intptr_list(
		size, universe_max // 2, universe_max // 4, 0, universe_max)
	return data

def generate_int_set_random_normal(universe_max: int, max_set_size: int) -> Set[Tuple[int, int]]:
	return set(
		generate_normal_set_internal(
			generate_normal_int(0, max_set_size // 2, 0, max_set_size),
			universe_max))

def container_to_str(a):
	return  f"{len(a)} " + (" ".join(map(str, sorted(a))))

def pointsto_container_to_str(a):
	return  f"{len(a)} " + (" ".join(map(str, [ j for t in sorted(a) for j in t ])))

def generate_operation_str(
		optype: Operation,
		arg1: Set[Tuple[int, int]],
		arg2: Set[Tuple[int, int]],
		result: Set[Tuple[int, int]]) -> str:
	return \
		optype.value           + "\n" + \
		pointsto_container_to_str(arg1) + "\n" + \
		pointsto_container_to_str(arg2) + "\n" + \
		pointsto_container_to_str(result) + "\n"

duration: float = 0

def generate_operations(file: IO, dist: RNGType, num_operations: int, universe_max: int, max_set_size: int):
	file.write(f"{num_operations}\n");
	for _ in range(num_operations):
		time_start = time.time()
		if dist == RNGType.UNIFORM:
			arg1 = generate_int_set_random_uniform(universe_max, max_set_size)
			arg2 = generate_int_set_random_uniform(universe_max, max_set_size)
		elif dist == RNGType.NORMAL:
			arg1 = generate_int_set_random_normal(universe_max, max_set_size)
			arg2 = generate_int_set_random_normal(universe_max, max_set_size)
		else:
			raise RuntimeError("Unknown case")
		optype: Operation = random.choice([ x for x in Operation ])
		result = {}

		if optype == Operation.UNION:
			result = arg1.union(arg2)
		elif optype == Operation.INTERSECTION:
			result = arg1.intersection(arg2)
		elif optype == Operation.DIFFERENCE:
			result = arg1.difference(arg2)
		time_end = time.time()
		global duration
		duration += time_end - time_start

		file.write(generate_operation_str(optype, arg1, arg2, result))

def main():
	parser = argparse.ArgumentParser(
		description="Generate testing input for LatticeHashForest")

	parser.add_argument(
		"--count",
		type=int,
		required=True,
		help="Number of operations to perform")

	parser.add_argument(
		"--maxsize",
		type=int,
		required=True,
		help="Maximum size of problem sets")

	parser.add_argument(
		"--maxval",
		type=int,
		required=True,
		help="Maximum unique values for elements")

	parser.add_argument(
		"--dist",
		choices=[ "uniform", "normal" ],
		type=str,
		required=False,
		help="Type of random distribution to use",
		default="uniform")

	parser.add_argument(
		"filepath",
		type=str,
		nargs='?',
		help="File path (Optional)",
		default=None)

	args = parser.parse_args()

	file = sys.stdout

	if args.filepath != None:
		file = open(args.filepath, "w")

	num_operations  = args.count
	universe_max    = args.maxval
	set_length_max  = args.maxsize
	dist_type       = RNGType.NORMAL if args.dist == "normal" else RNGType.UNIFORM

	generate_operations(file, dist_type, num_operations, universe_max, set_length_max)
	print(f"@@@@ Python Operation Time: {duration * 1000} ms")

if __name__ == '__main__':
	main()