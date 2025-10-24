#ifndef LHF_INTERFACE_IMPLEMENTATION
#define LHF_INTERFACE_IMPLEMENTATION

#include <functional>
#include <map>
#include <ostream>
#include <set>

namespace test {

template <
	typename OrderedSetT, typename ElementT,
	typename PropertyLess = std::less<ElementT>>
struct SetLess {
	bool operator()(const OrderedSetT *a, const OrderedSetT *b) const {
		PropertyLess less;

		auto cursor_1 = a->begin();
		const auto &cursor_end_1 = a->end();
		auto cursor_2 = b->begin();
		const auto &cursor_end_2 = b->end();

		while (cursor_1 != cursor_end_1 && cursor_2 != cursor_end_2) {
			if (less(*cursor_1, *cursor_2)) {
				return true;
			}

			if (less(*cursor_2, *cursor_1)) {
				return false;
			}

			cursor_1++;
			cursor_2++;
		}

		return a->size() < b->size();
	}
};

template <
	typename OrderedSetT, typename ElementT,
	typename PropertyEqual = std::equal_to<ElementT>>
struct SetEqual {
	inline bool operator()(const OrderedSetT *a, const OrderedSetT *b) const {
		PropertyEqual eq;
		if (a->size() != b->size()) {
			return false;
		}

		if (a->size() == 0) {
			return true;
		}

		auto cursor_1 = a->begin();
		const auto &cursor_end_1 = a->end();
		auto cursor_2 = b->begin();

		while (cursor_1 != cursor_end_1) {
			if (!eq(*cursor_1, *cursor_2)) {
				return false;
			}

			cursor_1++;
			cursor_2++;
		}

		return true;
	}
};
// INTERFACE BASE

template <typename ContainerT>
struct InterfaceBase {

	using ContainerType = ContainerT;

	template <typename ElementT>
	class ConstIteratorImpl {
		using BaseConstIterator = typename ContainerType::const_iterator;
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

	using key_type = typename ContainerType::key_type;
	using value_type = typename ContainerType::value_type;
	using size_type = typename ContainerType::size_type;

	ContainerType data;

	InterfaceBase(const ContainerType &data = {}) : data(data) {}
	InterfaceBase(ContainerType &&data) : data(data) {}

	bool is_empty() const {
		return data.size() == 0;
	}
};

// INTERFACES

struct PointeeSet : InterfaceBase<std::set<int>> {
	struct Element {
		const value_type p;
		Element(const value_type p) : p(p) {}

		int value() const {
			return p;
		}

		friend std::ostream& operator<<(std::ostream& os, const Element& obj) {
			os << obj.value();
			return os;
		}
	};

	using ConstIterator = ConstIteratorImpl<Element>;

	PointeeSet(): InterfaceBase() {}
	PointeeSet(const ContainerType &data) : InterfaceBase(data) {}
	PointeeSet(ContainerType &&data) : InterfaceBase(data) {}
	template <typename Iterator>
	PointeeSet(Iterator begin, Iterator end) : InterfaceBase(ContainerType(begin, end)) {}

	// CONTAINER INTERFACE GOES HERE

	PointeeSet set_union(const PointeeSet &b) const {
		ContainerType out;
		std::set_union(
			data.begin(), data.end(), b.data.begin(), b.data.end(),
			std::inserter(out, out.begin()));
		return PointeeSet(std::move(out));
	}

	PointeeSet set_intersection(const PointeeSet &b) const {
		ContainerType out;
		std::set_intersection(
			data.begin(), data.end(), b.data.begin(), b.data.end(),
			std::inserter(out, out.begin()));
		return PointeeSet(std::move(out));
	}

	PointeeSet set_difference(const PointeeSet &b) const {
		ContainerType out;
		std::set_difference(
			data.begin(), data.end(), b.data.begin(), b.data.end(),
			std::inserter(out, out.begin()));
		return PointeeSet(std::move(out));
	}

	PointeeSet set_insert_single(const value_type &p) {
		data.insert(p);
		return PointeeSet(std::move(data));
	}

	PointeeSet set_remove_single(const value_type &p) {
		data.erase(p);
		return PointeeSet(std::move(data));
	}

	bool contains(const value_type &p) {
		return data.count(p) > 0;
	}

	const ContainerType &get_value() const {
		return data;
	}

	bool operator<(const InterfaceBase &b) const {
		return SetLess<ContainerType, value_type>()(&data, &b.data);
	}

	bool operator==(const InterfaceBase &b) const {
		return SetEqual<ContainerType, value_type>()(&data, &b.data);
	}

	std::size_t size() const {
		return data.size();
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

struct PointsToSet : InterfaceBase<std::map<int, std::set<int>>> {
	struct Element {
		const value_type &p;
		Element(const value_type &p) : p(p) {}

		// ELEMENT INTERFACE GOES HERE

		int key() const {
			return p.first;
		}

		PointeeSet value() const {
			return PointeeSet(p.second);
		}

		friend std::ostream& operator<<(std::ostream& os, const Element& obj) {
			os << "(" << obj.key() << " -> " << obj.value() << " }";
			return os;
		}
	};

	using ConstIterator = ConstIteratorImpl<Element>;

	PointsToSet(ContainerType &data) : InterfaceBase(data) {}
	PointsToSet(ContainerType &&data = {}) : InterfaceBase(data) {}
	template <typename Iterator>
	PointsToSet(Iterator begin, Iterator end) : InterfaceBase(ContainerType(begin, end)) {}

	// CONTAINER INTERFACE GOES HERE

	PointsToSet set_union(const PointsToSet &b) {
		ContainerType new_set;

		auto cursor_1 = data.begin();
		const auto &cursor_end_1 = data.end();
		auto cursor_2 = b.data.begin();
		const auto &cursor_end_2 = b.data.end();

		while (cursor_1 != cursor_end_1) {
			if (cursor_2 == cursor_end_2) {
				new_set.insert(cursor_1, cursor_end_1);
				break;
			}

			if (cursor_2->first < cursor_1->first) {
				new_set.insert(*cursor_2);
				cursor_2++;
			} else {
				if (!(cursor_1->first < cursor_2->first)) {
					PointeeSet::ContainerType union_data;
					std::set_union(
						cursor_1->second.begin(), cursor_1->second.end(),
						cursor_2->second.begin(), cursor_2->second.end(),
						std::inserter(union_data, union_data.begin()));
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

	PointsToSet set_intersection(const PointsToSet &b) {
		ContainerType new_set;

		auto cursor_1 = data.begin();
		const auto &cursor_end_1 = data.end();
		auto cursor_2 = b.data.begin();
		const auto &cursor_end_2 = b.data.end();

		while (cursor_1 != cursor_end_1 && cursor_2 != cursor_end_2) {
			if (cursor_1->first < cursor_2->first) {
				cursor_1++;
			} else {
				if (!(cursor_2->first < cursor_1->first)) {
					PointeeSet::ContainerType intersection_data;
					std::set_intersection(
						cursor_1->second.begin(), cursor_1->second.end(),
						cursor_2->second.begin(), cursor_2->second.end(),
						std::inserter(intersection_data, intersection_data.begin()));
					new_set.insert({cursor_1->first, intersection_data});
					cursor_1++;
				}
				cursor_2++;
			}
		}

		return std::move(new_set);
	}

	PointsToSet set_difference(const PointsToSet &b) {
		ContainerType new_set;

		auto cursor_1 = data.begin();
		const auto &cursor_end_1 = data.end();
		auto cursor_2 = b.data.begin();
		const auto &cursor_end_2 = b.data.end();

		while (cursor_1 != cursor_end_1) {
			if (cursor_2 == cursor_end_2) {
				new_set.insert(cursor_1, cursor_end_1);
				break;
			}

			if (cursor_1->first < cursor_2->first) {
				new_set.insert(*cursor_1);
				cursor_1++;
			} else {
				if (!(cursor_2->first < cursor_1->first)) {
					PointeeSet::ContainerType difference_data;
					std::set_difference(
						cursor_1->second.begin(), cursor_1->second.end(),
						cursor_2->second.begin(), cursor_2->second.end(),
						std::inserter(difference_data, difference_data.begin()));
					new_set.insert({cursor_1->first, difference_data});
					cursor_1++;
				}
				cursor_2++;
			}
		}

		return std::move(new_set);
	}

	PointsToSet set_insert_single(const value_type &p) {
		data.insert(p);
		return PointsToSet(std::move(data));
	}

	PointsToSet set_remove_single(const value_type &p) {
		data.erase(p.first);
		return PointsToSet(std::move(data));
	}

	PointsToSet insert_pointee(const key_type k, const ContainerType::mapped_type::value_type v) {
		data[k].insert(v);
		return std::move(data);
	}

	PointsToSet set_remove_single_key(const key_type &p) {
		data.erase(p);
		return PointsToSet(std::move(data));
	}

	bool contains(const key_type &p) {
		return data.count(p) > 0;
	}

	const ContainerType &get_value() const {
		return data;
	}

	bool operator<(const InterfaceBase &b) const {
		auto cursor_1 = data.begin();
		const auto &cursor_end_1 = data.end();
		auto cursor_2 = b.data.begin();
		const auto &cursor_end_2 = b.data.end();

		while (cursor_1 != cursor_end_1 && cursor_2 != cursor_end_2) {
			if (*cursor_1 < *cursor_2) {
				return true;
			}

			if (SetLess<
			    PointeeSet::ContainerType,
			    PointeeSet::value_type>()
			    (&cursor_1->second, &cursor_2->second)) {
				return true;
			}

			cursor_1++;
			cursor_2++;
		}

		return data.size() < b.data.size();
	}

	bool operator==(const InterfaceBase &b) const {
		if (data.size() != b.data.size()) {
			return false;
		}

		if (data.size() == 0) {
			return true;
		}

		auto cursor_1 = data.begin();
		const auto &cursor_end_1 = data.end();
		auto cursor_2 = b.data.begin();
		const auto &cursor_end_2 = b.data.end();

		while (cursor_1 != cursor_end_1) {
			if (cursor_1->first == cursor_2->first &&
			    SetEqual<
			    	PointeeSet::ContainerType,
			    	PointeeSet::value_type>()
			    	(&cursor_1->second, &cursor_2->second)) {
				cursor_1++;
				cursor_2++;
			} else {
				return false;
			}
		}

		return true;
	}

	std::size_t size() const {
		return data.size();
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