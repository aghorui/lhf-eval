import re
import csv
import sys
from typing import List

input_str = open(sys.argv[1]).read()
input_str = re.sub(r'(?m)^\+.*\n?', '', input_str)

float_regex_str = r"""^(?:[+-]?(?:\d+|\d*\.(?=\d)\d*)(?:[eE][+-]?\d+)?)$"""

float_regex: re.Pattern = re.compile(float_regex_str)
input_str = input_str.split()

input_str = [ s.strip(',') for s in input_str ]

w = [ float_regex.match(i)[0] for i in input_str if float_regex.match(i) != None ]

print(w)

assert(len(w) % 11 == 0)

def slice_per(source, step):
	i = 0
	ret = []
	while i < len(source):
		current = []
		for j in range(step):
			current.append(source[i])
			i += 1
		ret.append(current)

	return ret


data = slice_per(w, 11)

header = [
	'Operation Count',
	'Operation Time Python',
	'Overall Time Python',
	'PMU Python',
	'Operation Time LHF',
	'Overall Time LHF',
	'PMU LHF',
	'Operation Count 2',
	'Operation Time Naive',
	'Overall Time Naive',
	'PMU Naive']

d_closedworld = data[0:9]
d_closedworld_pointsto = data[9:18]

def write_csv(filename, data: List):
	data.insert(0, header)
	with open(filename, "w", newline="") as f:
		writer = csv.writer(f)
		writer.writerows(data)

write_csv("d_closedworld.csv", d_closedworld)
write_csv("d_closedworld_pointsto.csv", d_closedworld_pointsto)