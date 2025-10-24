#ifndef LHF_INTERFACE_IMPLEMENTATION
#define LHF_INTERFACE_IMPLEMENTATION

#include "lhf/lhf.hpp"

namespace test {

// LHF_DEFINITIONS

struct PointeeSetStore
	: lhf::LatticeHashForest<
		  long, lhf::DefaultLess<long>, lhf::DefaultHash<long>,
		  lhf::DefaultEqual<long>, lhf::DefaultPrinter<long>,
		  lhf::NestingNone<long>> {
	explicit PointeeSetStore(RefList reflist) : LatticeHashForest(reflist) {}

	// LHF-LEVEL COMPONENTS GO HERE
};

struct PointsToSetStore
	: lhf::LatticeHashForest<
		  long, lhf::DefaultLess<long>, lhf::DefaultHash<long>,
		  lhf::DefaultEqual<long>, lhf::DefaultPrinter<long>,
		  lhf::NestingBase<long, PointeeSetStore>> {
	explicit PointsToSetStore(RefList reflist) : LatticeHashForest(reflist) {}

	// LHF-LEVEL COMPONENTS GO HERE

	Index insert_pointee(Index set_value, PropertyElement k) {
		Index insertee = register_set_single(k);
		return set_union(set_value, insertee);
	}
};

// LHF ALIASES

// GLOBAL STATE

static PointeeSetStore pointeeset({});
static PointsToSetStore pointstoset(PointsToSetStore::RefList{pointeeset});

// INTERFACE BASE

template <typename LHFT>
struct InterfaceBase {

	template <typename ElementT>
	class ConstIteratorImpl {
		using BaseConstIterator = typename LHFT::PropertySet::const_iterator;
		BaseConstIterator iter;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = ElementT;
		using difference_type = std::ptrdiff_t;
		using pointer = void;
		using reference = ElementT;

		ConstIteratorImpl(BaseConstIterator iter) : iter(iter) {}

		const ElementT operator*() const {
			return ElementT(*iter);
		}

		ConstIteratorImpl &operator++() {
			++iter;
			return *this;
		}

		bool operator!=(const ConstIteratorImpl &other) const {
			return iter != other.iter;
		}
	};

	using key_type = typename LHFT::PropertyElement::InterfaceKeyType;
	using value_type = typename LHFT::PropertyElement::InterfaceValueType;
	using size_type = typename LHFT::PropertySet::size_type;

	using Index = typename LHFT::Index;

	Index set_index;

	InterfaceBase(Index set_index = Index()) : set_index(set_index) {}

	bool operator<(const InterfaceBase &b) const {
		return set_index < b.set_index;
	}

	bool operator==(const InterfaceBase &b) const {
		return set_index == b.set_index;
	}

	bool is_empty() const {
		return set_index.is_empty();
	}
};

// INTERFACES

struct PointeeSet : InterfaceBase<PointeeSetStore> {
	struct Element {
		const PointeeSetStore::PropertyElement &p;
		Element(const PointeeSetStore::PropertyElement &p) : p(p) {}

		int value() const {
			return p.get_value();
		}

		friend std::ostream& operator<<(std::ostream& os, const Element& obj) {
			os << obj.value();
			return os;
		}
	};

	using ConstIterator = ConstIteratorImpl<Element>;

	PointeeSet(Index set_index = Index()) : InterfaceBase(set_index) {}
	PointeeSet(PointeeSetStore::PropertySet &s): InterfaceBase(pointeeset.register_set(s)) {}
	PointeeSet(PointeeSetStore::PropertySet &&s): InterfaceBase(pointeeset.register_set(std::move(s))) {}
	template<typename Iterator>
	PointeeSet(Iterator begin, Iterator end): InterfaceBase(pointeeset.register_set(begin, end)) {}

	// CONTAINER INTERFACE GOES HERE

	PointeeSet set_union(const PointeeSet &b) {
		return PointeeSet(pointeeset.set_union(set_index, b.set_index));
	}

	PointeeSet set_difference(const PointeeSet &b) {
		return PointeeSet(pointeeset.set_difference(set_index, b.set_index));
	}

	PointeeSet set_intersection(const PointeeSet &b) {
		return PointeeSet(pointeeset.set_intersection(set_index, b.set_index));
	}

	PointeeSet set_insert_single(const PointeeSetStore::PropertyElement &p) {
		return PointeeSet(pointeeset.set_insert_single(set_index, p));
	}

	PointeeSet set_remove_single(const PointeeSetStore::PropertyElement &p) {
		return PointeeSet(pointeeset.set_remove_single(set_index, p));
	}

	bool contains(const PointeeSetStore::PropertyElement &p) {
		return pointeeset.contains(set_index, p);
	}

	const typename PointeeSetStore::PropertySet &get_value() const {
		return pointeeset.get_value(set_index);
	}

	std::size_t size() const {
		return pointeeset.size_of(set_index);
	}

	ConstIterator begin() const {
		return ConstIterator(get_value().begin());
	}

	ConstIterator end() const {
		return ConstIterator(get_value().end());
	}

	friend std::ostream& operator<<(std::ostream& os, const PointeeSet& obj) {
		os << "{ ";
		for (auto &i : obj) {
			os << i << " ";
		}
		os << "}";
		return os;
	}
};

struct PointsToSet : InterfaceBase<PointsToSetStore> {
	struct Element {
		const PointsToSetStore::PropertyElement &p;
		Element(const PointsToSetStore::PropertyElement &p) : p(p) {}

		// ELEMENT INTERFACE GOES HERE

		int key() const {
			return p.get_key();
		}

		PointeeSet value() const {
			return PointeeSet(std::get<0>(p.get_value()));
		}

		friend std::ostream& operator<<(std::ostream& os, const Element& obj) {
			os << "(" << obj.key() << " -> " << obj.value() << ")";
			return os;
		}
	};

	using ConstIterator = ConstIteratorImpl<Element>;

	PointsToSet(Index set_index = Index()) : InterfaceBase(set_index) {}
	PointsToSet(PointsToSetStore::PropertySet &s): InterfaceBase(pointstoset.register_set(s)) {}
	PointsToSet(PointsToSetStore::PropertySet &&s): InterfaceBase(pointstoset.register_set(std::move(s))) {}
	template<typename Iterator>
	PointsToSet(Iterator begin, Iterator end): InterfaceBase(pointstoset.register_set(begin, end)) {}

	// CONTAINER INTERFACE GOES HERE

	PointsToSet set_union(const PointsToSet &b) {
		return PointsToSet(pointstoset.set_union(set_index, b.set_index));
	}

	PointsToSet set_difference(const PointsToSet &b) {
		return PointsToSet(pointstoset.set_difference(set_index, b.set_index));
	}

	PointsToSet set_intersection(const PointsToSet &b) {
		return PointsToSet(
			pointstoset.set_intersection(set_index, b.set_index));
	}

	PointsToSet set_insert_single(const PointsToSetStore::PropertyElement &p) {
		return PointsToSet(pointstoset.set_insert_single(set_index, p));
	}

	PointsToSet set_remove_single(const PointsToSetStore::PropertyElement &p) {
		return PointsToSet(pointstoset.set_remove_single(set_index, p));
	}

	PointsToSet insert_pointee(const key_type k, const value_type v) {
		PointeeSet pointee_set(pointeeset.register_set_single(v));
		return pointstoset.insert_pointee(set_index, {k, {pointee_set.set_index}});
	}

	bool contains(const PointsToSetStore::PropertyElement &p) {
		return pointstoset.contains(set_index, p);
	}

	const typename PointsToSetStore::PropertySet &get_value() const {
		return pointstoset.get_value(set_index);
	}

	std::size_t size() const {
		return pointstoset.size_of(set_index);
	}

	ConstIterator begin() const {
		return ConstIterator(get_value().begin());
	}

	ConstIterator end() const {
		return ConstIterator(get_value().end());
	}


	friend std::ostream& operator<<(std::ostream& os, const PointsToSet& obj) {
		os << "{ ";
		for (auto &i : obj) {
			os << i << " ";
		}
		os << "}";
		return os;
	}
};

// INTERFACE ALIASES

}; // namespace test
#endif