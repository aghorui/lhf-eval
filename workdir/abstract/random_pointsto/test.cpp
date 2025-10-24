#include <iostream>
#include <fstream>
#include <iostream>
#include <lhf/profiling.hpp>
#include <sstream>
#include <cassert>
#include <utility>

#ifdef ENABLE_OUTPUT
#define __OUTPUT(__x) __x
#else
#define __OUTPUT(__x)
#endif

#ifdef ENABLE_NAIVE_IMPLEMENTATION
#include "implementation_naive.hpp"
#else
#include "implementation_lhf.hpp"
#endif

#include "lhf/profiling.hpp"

template<typename T>
std::string to_string(const T &container) {
	std::stringstream s;
	s << "{ ";
	for (auto &i : container) {
		s << i << " ";
	}
	s << "}";
	return s.str();
}

test::PointsToSet normalize(test::PointsToSet p) {
	test::PointsToSet ret;

	for (auto i : p) {
		if (!i.value().is_empty()) {
#ifdef ENABLE_NAIVE_IMPLEMENTATION
			ret = ret.set_insert_single({i.key(), i.value().data});
#else
			ret = ret.set_insert_single({i.key(), i.value().set_index});
#endif
		}
	}

	return ret;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s [test filename]\n", argv[0]);
		return 1;
	}

	enum {
		OPERTYPE,
		ARG1,
		ARG2,
		RESULT
	} state = OPERTYPE;

	std::set<std::pair<int, int>> arg1_array;
	std::set<std::pair<int, int>> arg2_array;
	std::set<std::pair<int, int>> expected_result_array;

	lhf::PerformanceStatistics perf;

	char opertype = '\0';

	std::ifstream file(argv[1]);

	if (file.bad()) {
		std::cout << "File not found: " << argv[1] << "\n";
		exit(1);
	}

	unsigned int num_tests;
	std::string line;

	if (!std::getline(file, line)) {
		printf("Malformed file: Expected first line to be a numerical value.\n");
		return 1;
	}

	{
		std::istringstream s(line);
		s >> num_tests;
	}

	unsigned int count = 0;

	while (std::getline(file, line)) {
		std::istringstream s(line);

		switch (state) {
		case OPERTYPE: {
			std::string opertype_str;
			s >> opertype_str;
			opertype = opertype_str[0];

			if(!(opertype == 'U' || opertype == 'I' || opertype == 'D')) {
				printf("Illegal operator: %c\n", opertype);
				exit(1);
			}
			state = ARG1;
			break;
		}

		case ARG1: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val1, val2;
				s >> val1 >> val2;
				arg1_array.insert({val1, val2});
			}
			state = ARG2;
			break;
		}

		case ARG2: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val1, val2;
				s >> val1 >> val2;
				arg2_array.insert({val1, val2});
			}
			state = RESULT;
			break;
		}

		case RESULT: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val1, val2;
				s >> val1 >> val2;
				expected_result_array.insert({val1, val2});
			}

			test::PointsToSet larg1;
			test::PointsToSet larg2;
			test::PointsToSet new_set;
			test::PointsToSet expected_result;

			perf.timer_start("Operation Time");
			for (auto i : arg1_array) {
				larg1 = larg1.insert_pointee(i.first, i.second);
			}

			for (auto i : arg2_array) {
				larg2 = larg2.insert_pointee(i.first, i.second);
			}

			switch (opertype) {
			case 'U': new_set = larg1.set_union(larg2); break;
			case 'I': new_set = larg1.set_intersection(larg2); break;
			case 'D': new_set = larg1.set_difference(larg2); break;
			}
			perf.timer_end("Operation Time");

#ifndef DISABLE_VERIFICATION
			for (auto i : expected_result_array) {
				expected_result = expected_result.insert_pointee(i.first, i.second);
			}

			new_set = normalize(new_set);

			// std::cout << "Propset Index: " << l.register_set(result) << "\n";
			// std::cout << "Propset Index: " << l.register_set(result) << "\n";
			if (!(new_set == expected_result)) {
				std::cout
					<< "Could not match the following operation: ";

				switch (opertype) {
				case 'U': std::cout << "UNION\n"; break;
				case 'I': std::cout << "INTERSECTION\n";  break;
				case 'D': std::cout << "DIFFERENCE\n";  break;
				}

				std::cout
					<< "      ARG1:" << to_string(larg1)   << "\n"
					<< "      ARG2:" << to_string(larg2)   << "\n"
					<< "  EXPECTED:" << to_string(expected_result) << "\n"
					<< "       GOT:" << to_string(new_set) << "\n";

				return 1;
			}
#endif

			count++;
			__OUTPUT(printf("Completed %d/%d\n", count, num_tests););
			if (count >= num_tests) {
				goto end;
			}
			arg1_array.clear();
			arg2_array.clear();
			expected_result_array.clear();
			state = OPERTYPE;
			break;
		}

		}
	}

end:
#if !defined(ENABLE_NAIVE_IMPLEMENTATION) && defined(LHF_ENABLE_PERFORMANCE_METRICS)
	std::cout << test::pointeeset.dump_perf();
	std::cout << test::pointstoset.dump_perf();
#endif
	std::cout << perf.dump() << std::endl;
	return 0;
}