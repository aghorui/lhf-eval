#include <iostream>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>

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

	lhf::PerformanceStatistics perf;

	std::set<int> arg1;
	std::set<int> arg2;
	std::set<int> result;

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
				int val;
				s >> val;
				arg1.insert(val);
			}
			state = ARG2;
			break;
		}

		case ARG2: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val;
				s >> val;
				arg2.insert(val);
			}
			state = RESULT;
			break;
		}

		case RESULT: {
			int len;
			s >> len;
			for (int i = 0; i < len; i++) {
				int val;
				s >> val;
				result.insert(val);
			}

			perf.timer_start("Operation Time");
			test::PointeeSet larg1(arg1.begin(), arg1.end());
			test::PointeeSet larg2(arg2.begin(), arg2.end());

			test::PointeeSet new_set;

			switch (opertype) {
			case 'U': new_set = larg1.set_union(larg2); break;
			case 'I': new_set = larg1.set_intersection(larg2); break;
			case 'D': new_set = larg1.set_difference(larg2); break;
			}
			perf.timer_end("Operation Time");

#ifndef DISABLE_VERIFICATION
			// std::cout << "Propset Index: " << l.register_set(result) << "\n";
			// std::cout << "Propset Index: " << l.register_set(result) << "\n";
			if (!(new_set == test::PointeeSet(result.begin(), result.end()))) {
				std::cout
					<< "Could not match the following operation: ";

				switch (opertype) {
				case 'U': std::cout << "UNION\n"; break;
				case 'I': std::cout << "INTERSECTION\n";  break;
				case 'D': std::cout << "DIFFERENCE\n";  break;
				}

				std::cout
					<< "      ARG1:" << to_string(arg1)   << "\n"
					<< "      ARG2:" << to_string(arg2)   << "\n"
					<< "  EXPECTED:" << to_string(result) << "\n"
					<< "       GOT:" << to_string(new_set) << "\n";

				return 1;
			}
#endif

			count++;
			__OUTPUT(printf("Completed %d/%d\n", count, num_tests););
			if (count >= num_tests) {
				goto end;
			}
			arg1.clear();
			arg2.clear();
			result.clear();
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