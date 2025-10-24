#ifndef LFCPA_COMMON_H
#define LFCPA_COMMON_H
// #include "Analysis.h"
#include "slim/IR.h"
#include <stdexcept>

#define LHF_ENABLE_DEBUG
#define LHF_ENABLE_PERFORMANCE_METRICS
#include "lhf/lhf.hpp"

#include <cstddef>
#include <llvm-14/llvm/Support/raw_ostream.h>
#include <sstream>

#define ENABLE_INTEGRITY_CHECKING

#define GET_KEY(__x) ((__x).get_key())
#define GET_VALUE(__x) (std::get<0>((__x).get_value()))

#define GET_PTSET_KEY(__x) ((__x).get_key())
#define GET_PTSET_VALUE(__x) (std::get<0>((__x).get_value()))

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

static size_t STAT_operation_count = 0;

struct LivenessLHF
    : public lhf::LatticeHashForest<
          SLIMOperand *, SLIMOperandLess, std::hash<SLIMOperand *>, SLIMOperandEqual, SLIMOperandPrinter> {

    Index get_purely_global(Index a) {
        STAT_operation_count++;
        if (is_empty(a)) {
            return a;
        }

        PropertySet new_set;

        for (const PropertyElement &i : get_value(a)) {
            if (i.get_key()->isVariableGlobal()) {
                LHF_PUSH_ONE(new_set, i);
            }
        }

        return register_set(new_set);
    }

    Index get_purely_local(Index a) {
        STAT_operation_count++;
        if (is_empty(a)) {
            return a;
        }

        PropertySet new_set;

        for (const PropertyElement &i : get_value(a)) {
            if (!i.get_key()->isVariableGlobal()) {
                LHF_PUSH_ONE(new_set, i);
            }
        }

        return register_set(new_set);
    }
};

struct PointsToLHF : public lhf::LatticeHashForest<
                         SLIMOperand *, SLIMOperandLess, lhf::DefaultHash<SLIMOperand *>, SLIMOperandEqual,
                         SLIMOperandPrinter, lhf::NestingBase<SLIMOperand *, LivenessLHF>> {

    PointsToLHF(LivenessLHF &l) : LatticeHashForest(RefList{l}) {}

    // TODO REIMPLEMENT THIS
    Index update_pointees(Index set_value, PropertyElement k) {
        const PropertySet &first = get_value(set_value);
        PropertySet new_set;
        bool inserted = false;
        auto cursor_1 = first.begin();
        const auto &cursor_end_1 = first.end();

        while (cursor_1 != cursor_end_1) {
            if (less_key(k, *cursor_1)) {
                LHF_PUSH_ONE(new_set, k);
                LHF_PUSH_RANGE(new_set, cursor_1, cursor_end_1);
                inserted = true;
                break;
            } else {
                if (!(less_key(*cursor_1, k))) {
                    LHF_PUSH_ONE(new_set, k);
                    cursor_1++;
                    LHF_PUSH_RANGE(new_set, cursor_1, cursor_end_1);
                    inserted = true;
                    break;
                } else {
                    LHF_PUSH_ONE(new_set, *cursor_1);
                }
                cursor_1++;
            }
        }

        if (!inserted) {
            LHF_PUSH_ONE(new_set, k);
        }

        return register_set(std::move(new_set));
    }

    Index insert_pointee(Index set_value, PropertyElement k) {
        Index insertee = register_set_single(k);
        return set_union(set_value, insertee);
    }

    // This also needs to handle the empty set condition, apparently.
    LivenessLHF::Index get_pointees(Index set_value, SLIMOperand *pointer) {
        const PropertySet &first = get_value(set_value);
        for (auto &elem : first) {
            // POTENTIAL PROBLEM?
            if (GET_KEY(elem) == pointer) {
                return GET_VALUE(elem);
            }
        }
        return lhf::EMPTY_SET_VALUE;
    }
};

static LivenessLHF livenessLHF;
static PointsToLHF pointsToLHF(livenessLHF);

struct LFLivenessSet {
    using key_type = SLIMOperand *;
    using value_type = SLIMOperand *;
    using iterator = typename LivenessLHF::PropertySet::iterator;
    using const_iterator = typename LivenessLHF::PropertySet::const_iterator;
    using size_type = typename LivenessLHF::PropertySet::size_type;

    using Index = LivenessLHF::Index;
    using PropertyElement = LivenessLHF::PropertyElement;

    Index set_value = lhf::EMPTY_SET_VALUE;

    LFLivenessSet() {}

    ~LFLivenessSet() {}

    LFLivenessSet(const Index &b) {
#ifdef ENABLE_INTEGRITY_CHECKING
        // std::cout << "liveness set_value: " << set_value << "\n";
        assert(set_value.value >= 0 && set_value < livenessLHF.property_set_count());
        // CHECK SET INTEGRITY
        std::set<SLIMOperand *> k;
        for (const PropertyElement &i : livenessLHF.get_value(b)) {
            assert(k.count(i.get_key()) == 0);
            k.insert(i.get_key());
        }
#endif
        set_value = b;
    }

    LFLivenessSet(const Index &&b) {
        // std::cout << "liveness set_value: " << set_value << "\n";
#ifdef ENABLE_INTEGRITY_CHECKING
        assert(set_value.value >= 0 && set_value.value < livenessLHF.property_set_count());
#endif
        set_value = b;
    }

    static LFLivenessSet create_from_single(SLIMOperand *p) {
        return livenessLHF.register_set_single(p);
    }

    bool operator<(const LFLivenessSet &b) const {
        STAT_operation_count++;
        return set_value < b.set_value;
    }

    bool operator==(const LFLivenessSet &b) const {
        STAT_operation_count++;
        return set_value == b.set_value;
    }

    LFLivenessSet &operator=(const Index &b) {
        set_value = b;
        return *this;
    }

    LFLivenessSet &operator=(const Index &&b) {
        set_value = b;
        return *this;
    }

    LFLivenessSet set_union(const LFLivenessSet &b) const {
        STAT_operation_count++;
        return livenessLHF.set_union(set_value, b.set_value);
    }

    LFLivenessSet set_intersection(const LFLivenessSet &b) const {
        STAT_operation_count++;
        return livenessLHF.set_intersection(set_value, b.set_value);
    }

    LFLivenessSet get_purely_global() const {
        STAT_operation_count++;
        return LFLivenessSet(livenessLHF.get_purely_global(set_value));
    }
    LFLivenessSet get_purely_local() const {
        STAT_operation_count++;
        return LFLivenessSet(livenessLHF.get_purely_local(set_value));
    }

    LFLivenessSet insert_single(SLIMOperand *p) const {
        STAT_operation_count++;
        return livenessLHF.set_insert_single(set_value, p);
    }

    LFLivenessSet remove_single(SLIMOperand *p) const {
        STAT_operation_count++;
        return livenessLHF.set_remove_single(set_value, p);
    }

    const LivenessLHF::PropertySet &get_value() const {
        return livenessLHF.get_value(set_value);
    }

    bool contains(SLIMOperand *p) const {
        STAT_operation_count++;
        return livenessLHF.contains(set_value, p);
    }

    const_iterator begin() const {
        return livenessLHF.get_value(set_value).begin();
    }

    const_iterator end() const {
        return livenessLHF.get_value(set_value).end();
    }

    bool empty() const {
        return set_value == lhf::EMPTY_SET_VALUE;
    }

    std::size_t size() const {
        return livenessLHF.size_of(set_value);
    }
};

struct LFPointsToSet {
    using key_type = SLIMOperand *;
    using value_type = SLIMOperand *;
    using iterator = typename PointsToLHF::PropertySet::iterator;
    using const_iterator = typename PointsToLHF::PropertySet::const_iterator;
    using size_type = typename PointsToLHF::PropertySet::size_type;

    using Index = PointsToLHF::Index;
    using PropertyElement = PointsToLHF::PropertyElement;

    Index set_value = lhf::EMPTY_SET_VALUE;

    LFPointsToSet() {
        // nop
    }

    LFPointsToSet(Index set_value) : set_value(set_value) {
        // std::cout << "ptset set_value: " << set_value << "\n";
        // std::cout << "operand1: { ";
        // for (auto i : pointsToLHF.get_value(set_value)) {
        //     std::cout << "(" << i.key << ", " << i.value << "),";
        // }
        // std::cout << " }" << std::endl;
#ifdef ENABLE_INTEGRITY_CHECKING
        assert(set_value.value >= 0 && set_value < pointsToLHF.property_set_count());
        // CHECK SET INTEGRITY
        std::set<SLIMOperand *> k;
        for (const PropertyElement &i : pointsToLHF.get_value(set_value)) {
            // std::cout << "checking: (" << i.key << ", " << i.value << ")" <<
            // std::endl;
            assert(k.count(GET_KEY(i)) == 0);
            k.insert(GET_KEY(i));
        }
#endif
    }

    ~LFPointsToSet() {}

    bool operator<(const LFPointsToSet &b) const {
        STAT_operation_count++;
        return set_value < b.set_value;
    }

    bool operator==(const LFPointsToSet &b) const {
        STAT_operation_count++;
        return set_value == b.set_value;
    }

    bool empty() const {
        return set_value == lhf::EMPTY_SET_VALUE;
    }

    LFPointsToSet set_union(const LFPointsToSet &b) const {
        STAT_operation_count++;
        return pointsToLHF.set_union(set_value, b.set_value);
    }

    LFPointsToSet insert_pointee(SLIMOperand *pointer, SLIMOperand *pointee) {
        STAT_operation_count++;
        LivenessLHF::Index pointeeSet = livenessLHF.register_set_single(pointee);
        return pointsToLHF.insert_pointee(set_value, {pointer, pointeeSet});
    }

    LFPointsToSet update_pointees(SLIMOperand *pointer, LFLivenessSet pointees) {
        STAT_operation_count++;
        return pointsToLHF.update_pointees(set_value, {pointer, pointees.set_value});
    }

    LFLivenessSet get_pointees(SLIMOperand *pointer) {
        STAT_operation_count++;
        return pointsToLHF.get_pointees(set_value, pointer);
    }

    const PointsToLHF::PropertySet &get_value() const {
        return pointsToLHF.get_value(set_value);
    }

    std::size_t size() const {
        return pointsToLHF.size_of(set_value);
    }

    const_iterator begin() const {
        return pointsToLHF.get_value(set_value).begin();
    }

    const_iterator end() const {
        return pointsToLHF.get_value(set_value).end();
    }
};

#endif
