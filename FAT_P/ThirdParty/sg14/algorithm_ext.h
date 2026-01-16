#pragma once

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

namespace sg14 {

template<class FwdIt>
void destroy(FwdIt begin, FwdIt end) {
  typedef typename std::iterator_traits<FwdIt>::value_type T;
  while (begin != end) {
    begin->~T();
    ++begin;
  }
}

template<class SrcIt, class Sentinel, class FwdIt>
FwdIt uninitialized_move(SrcIt SrcBegin, Sentinel SrcEnd, FwdIt Dst) {
  typedef typename std::iterator_traits<FwdIt>::value_type T;
  FwdIt current = Dst;
  try {
    while (SrcBegin != SrcEnd) {
      ::new (static_cast<void*>(std::addressof(*current))) T(std::move(*SrcBegin));
      ++current;
      ++SrcBegin;
    }
    return current;
  } catch (...) {
    sg14::destroy(Dst, current);
    throw;
  }
}

template<class FwdIt, class Sentinel>
FwdIt uninitialized_value_construct(FwdIt first, Sentinel last) {
  typedef typename std::iterator_traits<FwdIt>::value_type T;
  FwdIt current = first;
  try {
    while (current != last) {
      ::new (static_cast<void*>(std::addressof(*current))) T();
      ++current;
    }
    return current;
  } catch (...) {
    sg14::destroy(first, current);
    throw;
  }
}

template<class FwdIt, class Sentinel>
FwdIt uninitialized_default_construct(FwdIt first, Sentinel last) {
  typedef typename std::iterator_traits<FwdIt>::value_type T;
  FwdIt current = first;
  try {
    while (current != last) {
      ::new (static_cast<void*>(std::addressof(*current))) T;
      ++current;
    }
    return current;
  } catch (...) {
    sg14::destroy(first, current);
    throw;
  }
}

template<class BidirIt, class UnaryPredicate>
#if __cplusplus >= 201703L
[[nodiscard]]
#endif
BidirIt unstable_remove_if(BidirIt first, BidirIt last, UnaryPredicate p) {
  while (true) {
    // Find the first instance of "p"...
    while (true) {
      if (first == last) {
        return first;
      }
      if (p(*first)) {
        break;
      }
      ++first;
    }
    // ...and the last instance of "not p"...
    while (true) {
      --last;
      if (first == last) {
        return first;
      }
      if (!p(*last)) {
        break;
      }
    }
    // ...and move the latter over top of the former.
    *first = std::move(*last);
    ++first;
  }
}

template<class BidirIt, class Val>
#if __cplusplus >= 201703L
[[nodiscard]]
#endif
BidirIt unstable_remove(BidirIt first, BidirIt last, const Val& v) {
  while (true) {
    // Find the first instance of "v"...
    while (true) {
      if (first == last) {
        return first;
      }
      if (*first == v) {
        break;
      }
      ++first;
    }
    // ...and the last instance of "not v"...
    while (true) {
      --last;
      if (first == last) {
        return first;
      }
      if (!(*last == v)) {
        break;
      }
    }
    // ...and move the latter over top of the former.
    *first = std::move(*last);
    ++first;
  }
}

} // namespace sg14
