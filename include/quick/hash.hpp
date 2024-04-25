// Copyright: 2019 Mohit Saini
// Author: Mohit Saini (mohitsaini1196@gmail.com)

#ifndef QUICK_HASH_HPP_
#define QUICK_HASH_HPP_

// Implements hash_impl by extending std::hash. Supports hashing for complex
// types.
// Sample usage:
// qk::hash<pair<int, map<int, string>>> hasher;
// std::size_t hashed_value = hasher(make_pair(....));
// using ComplexType = vector<pair<string, vector<int>>>;
// qk::hash<tuple<int, string, set<ValueTypeEnum>, ComplexType>>;
// struct ComplexClass {
//   ....
//   std::size_t GetHash() const {
//     .....
//     return ....
//   };
// };
// qk::hash<pair<ComplexType, vector<ComplexClass>>> hasher2;

#include <utility>
#include <unordered_set>
#include <set>
#include <vector>
#include <string>
#include <map>
#include <tuple>
#include <list>
#include <boost/functional/hash.hpp>

namespace quick {
namespace detail_hash_impl {
template<typename...> using void_t = void;

// DummyType argument is useful for defining specialization of Hash based on
// custom type-trait conditions, with use of `enable_if` and `void_t`.
// - Look at how hash<T> is used for enum types.
template<typename T, typename DummyType = void>
struct hash_impl: public std::hash<T> {};

// Ordered sequence is a container for which: if a == b then iterator sequence
// for a and b will be identical. This is necessary condition for the following
// implementation of hash function. o.w. hash function will violate the
// consistency constraint (a == b   ===>  hash(a) == hash(b))
//
// std::vector and std::set are ordered sequence.
// std::unordered_set is not ordered sequence.
//
// Note: Following implementation requires `.size()` member function on
// container. Althougt `.size()` member exists for std containers but if need
// to support for generic container, then this implementation need to be
// tweaked.
template <typename Container>
std::size_t OrderedSequenceHash(const Container& input) {
  auto hasher = hash_impl<typename Container::value_type>();
  size_t hash = hash_impl<size_t>()(input.size());
  for (auto& e : input) {
    boost::hash_combine(hash, hasher(e));
  }
  return hash;
}

template <typename MapContainer>
std::size_t OrderedMapHash(const MapContainer& input) {
  auto key_hasher = hash_impl<typename MapContainer::key_type>();
  auto value_hasher = hash_impl<typename MapContainer::mapped_type>();
  size_t hash = 0;
  for (auto& e : input) {
    boost::hash_combine(hash, key_hasher(e.first));
    boost::hash_combine(hash, key_hasher(e.second));
  }
  return hash;
}



template <typename T1, typename T2>
std::size_t PairHash(const std::pair<T1, T2>& p) {
  auto hash1 = hash_impl<T1>()(p.first);
  auto hash2 = hash_impl<T2>()(p.second);
  boost::hash_combine(hash1, hash2);
  return hash1;
}

// Prereq to know/read:
//  1. std::index_sequence, std::make_index_sequence
//  2. iteration over variable arguments in C++
//  3. std::tuple_element<2, tuple>::type denotes the type of 2nd element of
//     tuple.
template<typename... Ts, std::size_t... index>
std::size_t TupleHashImplHelper(const std::tuple<Ts...> &input,
                                std::index_sequence<index...>) {
  std::vector<std::size_t> hashed_elements = {
    hash_impl<typename std::tuple_element<index, std::tuple<Ts...>>::type>()(
      std::get<index>(input))...
  };
  return OrderedSequenceHash(hashed_elements);
}

template<typename... Ts>
std::size_t TupleHash(const std::tuple<Ts...>& input) {
  constexpr std::size_t num_tuple_elements
                            = std::tuple_size<std::tuple<Ts...>>::value;
  return TupleHashImplHelper(input,
                             std::make_index_sequence<num_tuple_elements>());
}

// ToDo(Mohit): Add default arguments (ex: Pred, Allocator) as well in
// std containers to make them more generic.

template<typename T>
struct hash_impl<std::vector<T>> {
  std::size_t operator()(const std::vector<T>& t) const {
    return OrderedSequenceHash(t);
  }
};

template<typename T>
struct hash_impl<std::list<T>> {
  std::size_t operator()(const std::list<T>& t) const {
    return OrderedSequenceHash(t);
  }
};

template<typename... Ts>
struct hash_impl<std::set<Ts...>> {
  std::size_t operator()(const std::set<Ts...>& t) const {
    return OrderedSequenceHash(t);
  }
};

template<typename... Ts>
struct hash_impl<std::map<Ts...>> {
  std::size_t operator()(const std::map<Ts...>& t) const {
    return OrderedMapHash(t);
  }
};

template<typename T1, typename T2>
struct hash_impl<std::pair<T1, T2>> {
  std::size_t operator()(const std::pair<T1, T2>& t) const {
    return PairHash(t);
  }
};

template<typename... Ts>
struct hash_impl<std::tuple<Ts...>> {
  std::size_t operator()(const std::tuple<Ts...>& t) const {
    return TupleHash(t);
  }
};

template<typename T>
struct hash_impl<T, std::enable_if_t<std::is_enum<T>::value>> {
  std::size_t operator()(const T& t) const {
    return static_cast<std::size_t>(t);
  }
};

// GetHash is special member function, which can be defined for a custom class.
// If GetHash is defined, class can be used as key of quick::unordered_map.
// If T::GetHash is not defined, substitution failure will look for other
// alternatives or fall back to default one.
//
// Warning-1: [Correctness] while defining GetHash method for custom classes,
//            one should guarantee that if (o1 == o2) then
//            `o1.GetHash() == o2.GetHash()` should be true for any object o1
//            and o2 of that class. (Assuming == is defined for that class)
// Warning-2: [Time Complexity] while defining GetHash method for custom classes
//            one should be mindful of collision.
//
// General mechanism to design GetHash method:
// struct A {
//   T1 t1;
//   T2 t2;
//   T3 t3;
//   ....
//   std::size_t GetHash() const {
//      return qk::HashFunction(t1, t2, t2);
//   }
// };
// And then define the GetHash for T1, T2, and T3 recursively.
template<typename T>
struct hash_impl<T, quick::detail_hash_impl::void_t<decltype(&T::GetHash)>> {
  std::size_t operator()(const T& t) const {
    return t.GetHash();
  }
};

}  // namespace detail_hash_impl

template<typename T> struct hash: public detail_hash_impl::hash_impl<T> {};

// Deprecated; use `HashFunction` instead.
template<typename T>
inline std::size_t HashF(const T& input) {
  return quick::hash<T>()(input);
}

inline std::size_t HashFunction() {
  return 0;
}

template<typename T>
inline std::size_t HashFunction(const T& input) {
  return quick::hash<T>()(input);
}

template<typename T1, typename T2, typename... Ts>
inline std::size_t HashFunction(const T1& i1, const T2& i2, const Ts&... is) {
  size_t hash = quick::HashFunction(i1);
  boost::hash_combine(hash, quick::HashFunction(i2));
  (void)std::initializer_list<int>{(boost::hash_combine(hash, quick::HashFunction(is)), 0)...};
  return hash;
}

}  // namespace quick

namespace qk = quick;

#endif  // QUICK_HASH_HPP_
