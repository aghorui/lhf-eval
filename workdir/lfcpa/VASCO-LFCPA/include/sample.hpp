#ifndef GPGPTANALYSIS_SAMPLE_DUMP_DATA_HPP
#define GPGPTANALYSIS_SAMPLE_DUMP_DATA_HPP

#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class DumpPointsToInfo {
    std::vector<std::string> emptyPointsTo;
    std::map<std::pair<std::string, long long>, std::vector<std::string>> pointsToInfo;

public:
    const inline std::vector<std::string> &getPointsToInfo(const std::pair<std::string, long long> &fileLine) {
        auto it = pointsToInfo.find(fileLine);
        if (it != pointsToInfo.end())
            return it->second;
        else
            return emptyPointsTo;
    }

    void insertPointsToPair(const std::pair<std::string, long long> &fileLine, const std::set<std::string> &targets) {
        auto it = pointsToInfo.find(fileLine);
        if (it == pointsToInfo.end()) {
            pointsToInfo.emplace(fileLine, std::vector<std::string>());
        }
        // Insert each target into the vector for the given key
        for (const auto &target : targets) {
            pointsToInfo.at(fileLine).push_back(target);
        }
    }

    DumpPointsToInfo() {
        insertPointsToPair({"samples/FP1.c", 10}, {"Q"});
        insertPointsToPair({"samples/FP2.c", 11}, {"Q"});
        insertPointsToPair({"samples/FP3.c", 12}, {"Q"});
        insertPointsToPair({"samples/FP4.c", 12}, {"Q"});
    }
};

#endif
