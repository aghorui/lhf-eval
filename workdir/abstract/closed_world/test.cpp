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

	unsigned int arg1_index;
	unsigned int arg2_index;
	std::set<int> result;

	char opertype = '\0';

	std::ifstream file(argv[1]);

	if (file.bad()) {
		std::cout << "File not found: " << argv[1] << "\n";
		exit(1);
	}

	unsigned int num_tests = 0;
	unsigned int corpus_count = 0;
	std::string line;

	if (!std::getline(file, line)) {
		printf("Malformed file: Expected first line to be a numerical value.\n");
		return 1;
	}

	{
		std::istringstream s(line);
		s >> corpus_count;
	}

	std::vector<test::PointeeSet> corpus;
	unsigned int count = 0;

	count = corpus_count;
	while (count > 0) {
		std::getline(file, line);
		count -= 1;
		std::istringstream s(line);
		std::set<int> arg1;
		int len;
		s >> len;
		for (int i = 0; i < len; i++) {
			int val;
			s >> val;
			arg1.insert(val);
		}
		corpus.push_back(test::PointeeSet(arg1.begin(), arg1.end()));
	}

	{
		std::getline(file, line);
		std::istringstream s(line);
		s >> num_tests;
	}

	while (std::getline(file, line)) {
		std::istringstream s(line);

		switch (state) {
		case OPERTYPE: {
			std::string opertype_str;
			s >> opertype_str;
			opertype = opertype_str[0];

			if(!(opertype == 'U' || opertype == 'I' || opertype == 'D' || opertype == 'S')) {
				printf("Illegal operator: %c\n", opertype);
				exit(1);
			}

			if (opertype == 'S') {
				unsigned int start, stop;
				s >> start >> stop;
				for (unsigned int i = start; i <= stop; i++) {
					corpus.push_back(corpus[i]);
				}
				__OUTPUT(std::cout << "Increased corpus by " << stop - start + 1 << std::endl;);
				continue;
			}

			state = ARG1;
			break;
		}

		case ARG1: {
			s >> arg1_index;
			state = ARG2;
			break;
		}

		case ARG2: {
			s >> arg2_index;
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

			test::PointeeSet new_set;

			perf.timer_start("Operation Time");
			switch (opertype) {
			case 'U': new_set = corpus[arg1_index].set_union(corpus[arg2_index]); break;
			case 'I': new_set = corpus[arg1_index].set_intersection(corpus[arg2_index]); break;
			case 'D': new_set = corpus[arg1_index].set_difference(corpus[arg2_index]); break;
			}
			perf.timer_end("Operation Time");

#ifndef DISABLE_VERIFICATION
			test::PointeeSet expected_result(result.begin(), result.end());
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
					<< "      ARG1:" << to_string(corpus[arg1_index]) << "\n"
					<< "      ARG2:" << to_string(corpus[arg2_index]) << "\n"
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
			arg1_index = 0;
			arg2_index = 0;
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