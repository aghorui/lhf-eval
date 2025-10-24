#ifndef PROFILING_H
#define PROFILING_H

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>

struct PerformanceStatistics {
    using Count = uint64_t;
    using String = std::string;
    using TimePoint = std::chrono::steady_clock::time_point;
    template <typename K, typename V>
    using Map = std::map<K, V>;

    struct Duration {
        bool started = false;
        TimePoint t1;
        TimePoint t2;
        long double duration = 0;

        long double getCurrentDurationMilliseconds() {
            return (std::chrono::duration_cast<std::chrono::microseconds>(t2.time_since_epoch()).count() -
                    std::chrono::duration_cast<std::chrono::microseconds>(t1.time_since_epoch()).count()) /
                   1000.0;
        }

        long double getCumulativeDurationMilliseconds() {
            return duration;
        }
    };

    Map<String, Count> counters;
    Map<String, Duration> timers;

    // Timer Functions

    Duration &getTimer(const String &s) {
        if (timers.count(s) == 0) {
            timers[s] = Duration{};
        }

        return timers[s];
    }

    void timerStart(const String &s) {
        auto &d = getTimer(s);
        // assert(!d.started && "timer already started");
        d.started = true;
        d.t1 = std::chrono::steady_clock::now();
    }

    void timerEnd(const String &s) {
        auto &d = getTimer(s);
        assert(d.started && "timer already stopped");
        d.started = false;
        d.t2 = std::chrono::steady_clock::now();
        d.duration += d.getCurrentDurationMilliseconds();
    }

    // Counter Functions

    Count &getCounter(const String &s) {
        if (counters.count(s) == 0) {
            counters[s] = 0;
        }

        return counters[s];
    }

    void incrementCounter(const String &s) {
        getCounter(s)++;
    }

    // dump

    void dump() {
        using namespace std;
        cout << endl << "Statistics:" << endl;
        for (auto k : counters) {
            cout << "    " << "'" << k.first << "'" << ": " << k.second << endl;
        }
        for (auto k : timers) {
            cout << "    " << "'" << k.first << "'" << ": " << k.second.getCumulativeDurationMilliseconds() << " ms"
                 << endl;
        }
    }
};

extern PerformanceStatistics stat;

#endif
