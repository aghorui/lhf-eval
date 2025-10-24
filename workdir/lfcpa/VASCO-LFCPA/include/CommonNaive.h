#ifndef LFCPA_COMMON_H
#define LFCPA_COMMON_H
// #include "Analysis.h"
#include "slim/IR.h"
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

#define LHF_ENABLE_DEBUG
#define LHF_ENABLE_PERFORMANCE_METRICS
#include "lhf/lhf.hpp"

#include <cstddef>
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <sstream>

#define ENABLE_INTEGRITY_CHECKING

#define GET_KEY(__x) ((__x))
#define GET_VALUE(__x) ((__x).second)

#define GET_PTSET_KEY(__x) ((__x).first)
#define GET_PTSET_VALUE(__x) ((__x).second)

static long long int DBG_total_insts = 0;

inline bool compareIndices(std::vector<SLIMOperand *> ipVec1, std::vector<SLIMOperand *> ipVec2) {
    if (ipVec1.size() != ipVec2.size())
        return false;

    for (int i = 0; i < ipVec1.size(); i++) {
        if (ipVec1[i]->getValue() != ipVec2[i]->getValue())
            return false;
    }
    return true;
}

inline bool compareOperands(SLIMOperand *a1, SLIMOperand *a2) {
    if (a1 == a2) {
        return true;
    }

    if (a1->isDynamicAllocationType() and a2->isDynamicAllocationType()) {
        // ignore indices
        if (a1->getValue() == a2->getValue() and compareIndices(a1->getIndexVector(), a2->getIndexVector())) {
            return true;
        }
    } else if (a1->isArrayElement() and a2->isArrayElement()) {
        llvm::Value *v1 = a1->getValueOfArray();
        llvm::Value *v2 = a2->getValueOfArray();
        if (v1 == v2) {
            return true;
        }
    } else {
        if (a1->getValue() == a2->getValue() and compareIndices(a1->getIndexVector(), a2->getIndexVector())) {
            return true;
        }
    }
    return false;
}

struct SLIMOperandLess {
    bool operator()(SLIMOperand *a, SLIMOperand *b) const {
        if (!compareOperands(a, b)) {
            return a < b;
        } else {
            return false;
        }
    }
};

struct SLIMOperandEqual {
    bool operator()(SLIMOperand *a, SLIMOperand *b) const {
        return compareOperands(a, b);
    }
};

struct SLIMOperandPrinter {
    std::string operator()(SLIMOperand *a) const {
        std::stringstream s;
        s << a;
        return s.str();
        // return a->hasName() ? a->getName().str() : "<UNNAMED>";
    }
};

template<typename SetT>
static inline bool setLessThan(const SetT &a, const SetT &b) {
    if (a.size() > b.size()) {
        return true;
    }

    if (a.size() == 0) {
        return false;
    }

    auto cursor_1 = a.begin();
    const auto &cursor_end_1 = a.end();
    auto cursor_2 = b.begin();
    const auto &cursor_end_2 = b.end();

    while (cursor_1 != cursor_end_1) {
        if (SLIMOperandLess()(*cursor_1, *cursor_2)) {
            cursor_1++;
            cursor_2++;
        } else {
            return true;
        }
    }

    return false;
}

template<typename SetT>
static inline bool setEqual(const SetT &a, const SetT &b) {
    if (a.size() != b.size()) {
        return false;
    }

    if (a.size() == 0) {
        return true;
    }

    auto cursor_1 =a.begin();
    const auto &cursor_end_1 = a.end();
    auto cursor_2 = b.begin();
    const auto &cursor_end_2 = b.end();

    while (cursor_1 != cursor_end_1) {
        if (SLIMOperandEqual()(*cursor_1, *cursor_2)) {
            cursor_1++;
            cursor_2++;
        } else {
            return false;
        }
    }

    return true;
}

static size_t STAT_operation_count = 0;

struct LFLivenessSet {
    using container_type = std::set<SLIMOperand *>;
    using key_type = SLIMOperand *;
    using value_type = SLIMOperand *;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using size_type = typename container_type::size_type;

    std::set<SLIMOperand *> elems;

    LFLivenessSet() {}

    ~LFLivenessSet() {}

    LFLivenessSet(container_type &elems):
        elems(elems) {}

    LFLivenessSet(container_type &&elems):
        elems(elems) {}

    static LFLivenessSet create_from_single(key_type p) {
        STAT_operation_count++;
        return LFLivenessSet({p});
    }

    bool operator<(const LFLivenessSet &b) const {
        STAT_operation_count++;
        return setLessThan(elems, b.elems);
    }

    bool operator==(const LFLivenessSet &b) const {
        STAT_operation_count++;
        return setEqual(elems, b.elems);
    }

    LFLivenessSet &operator=(const container_type &b) {
        elems = b;
        return *this;
    }

    LFLivenessSet &operator=(const container_type &&b) {
        elems = b;
        return *this;
    }

    LFLivenessSet set_union(const LFLivenessSet &b) const {
        STAT_operation_count++;
        container_type out;
        std::set_union(
            elems.begin(), elems.end(),
            b.elems.begin(), b.elems.end(), std::inserter(out, out.begin()), SLIMOperandLess());
        return LFLivenessSet(std::move(out));
    }

    LFLivenessSet set_intersection(const LFLivenessSet &b) const {
        STAT_operation_count++;
        container_type out;
        std::set_intersection(
            elems.begin(), elems.end(),
            b.elems.begin(), b.elems.end(), std::inserter(out, out.begin()), SLIMOperandLess());
        return LFLivenessSet(std::move(out));
    }

    LFLivenessSet get_purely_global() const {
        STAT_operation_count++;
        if (empty()) {
            return LFLivenessSet();
        }

        container_type new_set;

        for (SLIMOperand *i : elems) {
            if (i->isVariableGlobal()) {
                new_set.insert(i);
            }
        }

        return std::move(new_set);
    }

    LFLivenessSet get_purely_local() const {
        STAT_operation_count++;
        if (empty()) {
            return LFLivenessSet();
        }

        container_type new_set;

        for (SLIMOperand *i : elems) {
            if (!i->isVariableGlobal()) {
                new_set.insert(i);
            }
        }

        return std::move(new_set);
    }

    LFLivenessSet insert_single(key_type p) {
        STAT_operation_count++;
        elems.insert(p);
        return std::move(elems);
    }

    LFLivenessSet remove_single(key_type p) {
        STAT_operation_count++;
        elems.erase(p);
        return std::move(elems);
    }

    bool contains(SLIMOperand *p) const {
        STAT_operation_count++;
        return elems.count(p) > 0;
    }

    const_iterator begin() const {
        return elems.begin();
    }

    const_iterator end() const {
        return elems.end();
    }

    bool empty() const {
        return elems.size() == 0;
    }

    std::size_t size() const {
        return elems.size();
    }
};

struct LFPointsToSet {
    using container_type = std::map<SLIMOperand *, std::set<SLIMOperand *>>;
    using key_type = SLIMOperand *;
    using value_type = std::set<SLIMOperand *>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using size_type = typename container_type::size_type;

    container_type elems;

    LFPointsToSet() {}

    ~LFPointsToSet() {}

    LFPointsToSet(container_type &elems):
        elems(elems) {}

    LFPointsToSet(container_type &&elems):
        elems(elems) {}

    bool operator<(const LFPointsToSet &b) const {
        STAT_operation_count++;
        return elems < b.elems;
    }

    bool operator==(const LFPointsToSet &b) const {
        STAT_operation_count++;
        if (elems.size() != b.elems.size()) {
            return false;
        }

        if (elems.size() == 0) {
            return true;
        }

        auto cursor_1 = elems.begin();
        const auto &cursor_end_1 = elems.end();
        auto cursor_2 = b.elems.begin();
        const auto &cursor_end_2 = b.elems.end();

        while (cursor_1 != cursor_end_1) {
            if (compareOperands(cursor_1->first, cursor_2->first) &&
                setEqual(cursor_1->second, cursor_2->second)) {
                cursor_1++;
                cursor_2++;
            } else {
                return false;
            }
        }

        return true;
    }

    bool empty() const {
        return elems.size() == 0;
    }

    LFPointsToSet set_union(const LFPointsToSet &b) const {
        STAT_operation_count++;
        container_type new_set;

        auto cursor_1 = elems.begin();
        const auto &cursor_end_1 = elems.end();
        auto cursor_2 = b.elems.begin();
        const auto &cursor_end_2 = b.elems.end();

        while (cursor_1 != cursor_end_1) {
            if (cursor_2 == cursor_end_2) {
                new_set.insert(cursor_1, cursor_end_1);
                break;
            }

            if (SLIMOperandLess()(cursor_2->first, cursor_1->first)) {
                new_set.insert(*cursor_2);
                cursor_2++;
            } else {
                if (!(SLIMOperandLess()(cursor_1->first, cursor_2->first))) {
                    LFLivenessSet::container_type union_data;
                    std::set_union(
                        cursor_1->second.begin(), cursor_1->second.end(),
                        cursor_2->second.begin(), cursor_2->second.end(),
                        std::inserter(union_data, union_data.begin()), SLIMOperandLess());
                    new_set.insert({cursor_1->first, union_data});
                    cursor_2++;
                } else {
                    new_set.insert(*cursor_1);
                }
                cursor_1++;
            }
        }

        new_set.insert(cursor_2, cursor_end_2);

        return std::move(new_set);
    }

    LFPointsToSet insert_pointee(SLIMOperand *pointer, SLIMOperand *pointee) {
        STAT_operation_count++;
        elems[pointer].insert(pointee);
        return std::move(elems);
    }

    LFPointsToSet update_pointees(SLIMOperand *pointer, LFLivenessSet pointees) {
        STAT_operation_count++;
        elems.insert_or_assign(pointer, pointees.elems);
        return std::move(elems);
    }

    LFLivenessSet get_pointees(SLIMOperand *pointer) {
        STAT_operation_count++;
        return elems.at(pointer);
    }

    std::size_t size() const {
        return elems.size();
    }

    const_iterator begin() const {
        return elems.begin();
    }

    const_iterator end() const {
        return elems.end();
    }
};

#endif
