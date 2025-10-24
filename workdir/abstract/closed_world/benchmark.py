#!/usr/bin/python3

import os
import sys
import random
import argparse
import time
from enum import Enum
from typing import IO, Generator, List, Set, Tuple

random.seed(time.time())

class RNGType(Enum):
	UNIFORM     = "uniform"
	NORMAL      = "normal"
	OPTIMISTIC  = "optimistic"

class Operation(Enum):
	UNION = "U"
	INTERSECTION = "I"
	DIFFERENCE = "D"
	CORPUS_INCREMENT = "S"

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

def generate_normal_int_list(
			size: int,
			mean: int,
			stddev: int,
			min_value: int | None = None,
			max_value: int | None = None) -> Set[int]:
	count: int = 0
	ret: Set[int] = set()

	while count < size:
		n = generate_normal_int(mean, stddev, min_value, max_value)
		if n not in ret:
			ret.add(n)
			count += 1

	return ret

def generate_int_set_random_uniform(universe_max: int, max_set_size: int) -> Set[int]:
	size: int = random.randint(0, max_set_size)
	return set(random.sample(range(0, universe_max + 1), size))

def generate_normal_set_internal(size = 20, universe_max = 200):
	data = generate_normal_int_list(
		size, universe_max // 2, universe_max // 4, 0, universe_max)
	return data

def generate_int_set_random_normal(universe_max: int, max_set_size: int) -> Set[int]:
	return set(
		generate_normal_set_internal(
			generate_normal_int(0, max_set_size // 2, 0, max_set_size),
			universe_max))

def container_to_str(a):
	return  f"{len(a)} " + (" ".join(map(str, sorted(a))))

def generate_operation_str(optype: Operation, arg1: int, arg2: int, result: Set[int]) -> str:
	return \
		optype.value     + "\n" + \
		str(arg1) + "\n" + \
		str(arg2) + "\n" + \
		container_to_str(result) + "\n"

def generate_corpus_str(corpus: List[Set[int]]) -> str:
	ret = ""

	ret += f"{len(corpus)}\n"
	for i in corpus:
		ret += f"{container_to_str(i)}\n"

	return ret

def generate_corpus_inc_str(arg1: int, arg2: int) -> str:
	return \
		Operation.CORPUS_INCREMENT.value + \
		" " + str(arg1) + " " + str(arg2) + "\n"

duration: float = 0

def generate_operations(file: IO, dist: RNGType, num_operations: int, universe_max: int, max_set_size: int, corpus_size: int):
	global duration
	operation_corpus: List[Tuple[int, int, Operation]] = []
	corpus: List[Set] = []

	if dist == RNGType.OPTIMISTIC:
		corpus = [ set() for _ in range(corpus_size) ]

		corpus_filldata = [
			generate_int_set_random_uniform(universe_max, max_set_size)
			for _ in range(corpus_size // 4)
		]
		for _ in range(corpus_size * 3 // 4):
			corpus[random.randint(0, len(corpus) - 1)] = \
			corpus_filldata[random.randint(0, len(corpus_filldata) - 1)]

		operation_corpus_size = random.randrange(1, len(corpus), 1)
		for _ in range(operation_corpus_size):
			optype = random.choice([ Operation.UNION, Operation.INTERSECTION, Operation.DIFFERENCE ])
			operation_corpus.append((
				random.randint(0, len(corpus) - 1),
				random.randint(0, len(corpus) - 1),
				optype))
	else:
		for _ in range(corpus_size):
			if dist == RNGType.UNIFORM:
				s = generate_int_set_random_uniform(universe_max, max_set_size)
			elif dist == RNGType.NORMAL:
				s = generate_int_set_random_normal(universe_max, max_set_size)
			else:
				raise RuntimeError("Unknown case")
			corpus.append(s)

	file.write(generate_corpus_str(corpus))

	corpus_next_increment = len(corpus) * 10

	file.write(f"{num_operations}\n");

	for _ in range(num_operations):
		if corpus_next_increment <= 0:
			start = random.randint(0, len(corpus) - 1)
			stop = random.randint(start, len(corpus) - 1)
			corpus.extend([ set(corpus[i]) for i in range(start, stop + 1) ])
			corpus_next_increment = len(corpus)
			file.write(generate_corpus_inc_str(start, stop))

			if dist == RNGType.OPTIMISTIC:
				operation_corpus_increment_size = random.randint(0, stop - start)
				for _ in range(operation_corpus_increment_size):
					optype = random.choice([ Operation.UNION, Operation.INTERSECTION, Operation.DIFFERENCE ])
					operation_corpus.append((
						random.randint(0, len(corpus) - 1),
						random.randint(0, len(corpus) - 1),
						optype))
		else:
			corpus_next_increment -= 1

		if dist == RNGType.OPTIMISTIC:
			time_start = time.time()
			arg1, arg2, optype = operation_corpus[random.randint(0, (len(operation_corpus) - 1))]
			if optype == Operation.UNION:
				result = corpus[arg1].union(corpus[arg2])
			elif optype == Operation.INTERSECTION:
				result = corpus[arg1].intersection(corpus[arg2])
			elif optype == Operation.DIFFERENCE:
				result = corpus[arg1].difference(corpus[arg2])
			else:
				raise ValueError(f"Invalid Enum Value: {optype}")
			time_end = time.time()
			duration += time_end - time_start

		else:
			time_start = time.time()
			if dist == RNGType.UNIFORM:
				arg1 = random.randint(0, (len(corpus) - 1))
				arg2 = random.randint(0, (len(corpus) - 1))
			elif dist == RNGType.NORMAL:
				arg1 = generate_normal_int(0, (len(corpus) - 1) // 2, 0, (len(corpus) - 1))
				arg2 = generate_normal_int(0, (len(corpus) - 1) // 2, 0, (len(corpus) - 1))
			else:
				raise RuntimeError("Unknown case")

			optype: Operation = random.choice([ Operation.UNION, Operation.INTERSECTION, Operation.DIFFERENCE ])
			result = {}

			if optype == Operation.UNION:
				result = corpus[arg1].union(corpus[arg2])
			elif optype == Operation.INTERSECTION:
				result = corpus[arg1].intersection(corpus[arg2])
			elif optype == Operation.DIFFERENCE:
				result = corpus[arg1].difference(corpus[arg2])
			else:
				raise ValueError(f"Invalid Enum Value: {optype}")
			time_end = time.time()
			duration += time_end - time_start

		# print(optype, "|||", corpus[arg1], "|||", corpus[arg2], "|||", result)
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
		"--corpussize",
		type=int,
		required=True,
		help="Size of corpus")

	parser.add_argument(
		"--dist",
		choices=[ "uniform", "normal", "optimistic" ],
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
	corpus_size     = args.corpussize

	if args.dist == "uniform":
		dist_type = RNGType.UNIFORM
	elif args.dist == "normal":
		dist_type = RNGType.NORMAL
	elif args.dist == "optimistic":
		dist_type = RNGType.OPTIMISTIC
	else:
		dist_type = RNGType.UNIFORM

	generate_operations(file, dist_type, num_operations, universe_max, set_length_max, corpus_size)

	print(f"@@@@ Python Operation Time: {duration * 1000} ms")

if __name__ == '__main__':
	main()