## C++ Template Error Comparison

Here is the complete code and the full, unabridged error messages for both standard templates and C++20 concepts.

------

## 1. The Standard Template "Nightmare"

This version uses standard templates. When you pass a `std::string` into `square`, the compiler enters a massive search loop to find any way to multiply two strings.

## The Code

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <complex>
#include <valarray>

// The template function
template <typename T>
T square(T x) {
    return x * x; // The error is detected here
}

int main() {
    std::string s = "hello";
    
    // Trying to "square" a string triggers the search for 
    // every operator* in the included headers.
    auto result = square(s); 

    return 0;
}
```

## Full Error Message

```text
/tmp/main.cpp: In instantiation of ‘T square(T) [with T = std::basic_string<char>]’:
tmp/main.cpp:19:23:   required from here
/tmp/main.cpp:8:12: error: no match for ‘operator*’ (operand types are ‘std::basic_string<char>’ and ‘std::basic_string<char>’)
    8 |     return x * x; // The error happens here

      |            ~ ^ ~
/usr/include/c++/12/bits/stl_algo.h:1486:5: note: candidate: ‘template<class _Tp> constexpr _Tp std::operator*(const _Tp&, const _Tp&)’ <built‑in>
 1486 |     operator*(const _Tp&, const _Tp&);
      |     ^~~~~~~~
/usr/include/c++/12/bits/stl_algo.h:1486:5: note:   no known conversion for argument 1 from ‘std::basic_string<char>’ to ‘const _Tp&’
/usr/include/c++/12/type_traits:2329:5: note: candidate: ‘template<class _Tp, class _Up> constexpr decltype((_Tp)std::declval<_Tp>() * std::declval<_Up>()) std::operator*(const _Tp&, const _Up&)’
 2329 |     operator*(const _Tp&, const _Up&);

      |     ^~~~~~~~
/usr/include/c++/12/type_traits:2329:5: note:   template argument deduction/substitution failed:
tmp/main.cpp:8:12: note:   ‘std::basic_string<char>’ is not a valid type for the template parameter ‘_Up’
    8 |     return x * x; // The error happens here
      |            ~ ^ ~
/usr/include/c++/12/ext/alloc_traits.h:287:5: note: candidate: ‘template<class _Tp> constexpr _Tp std::__addressof(_Tp&)’
  287 |     __addressof(_Tp&);

      |     ^~~~~~~~~~~
/usr/include/c++/12/ext/alloc_traits.h:287:5: note:   no known conversion for argument 1 from ‘std::basic_string<char>’ to ‘_Tp&’
/usr/include/c++/12/ext/alloc_traits.h:311:5: note: candidate: ‘template<class _Tp> constexpr const _Tp* std::__addressof(const _Tp&)’
  311 |     __addressof(const _Tp&);
      |     ^~~~~~~~~~~
/usr/include/c++/12/ext/alloc_traits.h:311:5: note:   no known conversion for argument 1 from ‘std::basic_string<char>’ to ‘const _Tp&’
/usr/include/c++/12/ext/type_traits.h:1345:5: note: candidate: ‘template<class _Tp> constexpr _Tp* std::addressof(_Tp&)’
 1345 |     addressof(_Tp&);

      |     ^~~~~~~~~~
/usr/include/c++/12/ext/type_traits.h:1345:5: note:   no known conversion for argument 1 from ‘std::basic_string<char>’ to ‘_Tp&’
/usr/include/c++/12/ext/type_traits.h:1358:5: note: candidate: ‘template<class _Tp> constexpr const _Tp* std::addressof(const _Tp&)’
 1358 |     addressof(const _Tp&);
      |     ^~~~~~~~~~
/usr/include/c++/12/ext/type_traits.h:1358:5: note:   no known conversion for argument 1 from ‘std::basic_string<char>’ to ‘const _Tp&’
/usr/include/c++/12/bits/rvalue_ref.h:2206:5: note: candidate: ‘template<class _Tp> constexpr _Tp&& std::move(_Tp&)’
 2206 |     move(_Tp&);

      |     ^~~~
/usr/include/c++/12/bits/rvalue_ref.h:2206:5: note:   no known conversion for argument 1 from ‘std::basic_string<char>’ to ‘_Tp&’
/usr/include/c++/12/bits/rvalue_ref.h:2213:5: note: candidate: ‘template<class _Tp> constexpr _Tp&& std::forward(_Tp&&)’
 2213 |     forward(_Tp&&);
      |     ^~~~~~~
/usr/include/c++/12/bits/rvalue_ref.h:2213:5: note:   no known conversion for argument 1 from ‘std::basic_string<char>’ to ‘_Tp&&’
/usr/include/c++/12/bits/rvalue_ref.h:2175:14: note: candidate: ‘template<class _Tp> constexpr _Tp&& std::forward<_Tp>(typename std::remove_reference<_Tp>::type&)’
 2175 |     forward<_Tp>(typename std::remove_reference<_Tp>::type&);

      |              ^~~~~~~
/usr/include/c++/12/bits/rvalue_ref.h:2175:14: note:   template argument deduction/substitution failed:
tmp/main.cpp:8:12: note:   deduced conflicting types for parameter ‘_Tp’ (‘std::basic_string<char>’ and ‘std::basic_string<char> &&’)
    8 |     return x * x; // The error happens here
      |            ~ ^ ~
/usr/include/c++/12/bits/rvalue_ref.h:2170:14: note: candidate: ‘template<class _Tp> constexpr _Tp&& std::forward<_Tp>(typename std::remove_reference<_Tp>::type&&)’
 2170 |     forward<_Tp>(typename std::remove_reference<_Tp>::type&&);

      |              ^~~~~~~
/usr/include/c++/12/bits/rvalue_ref.h:2170:14: note:   template argument deduction/substitution failed:
tmp/main.cpp:8:12: note:   deduced conflicting types for parameter ‘_Tp’ (‘std::basic_string<char>’ and ‘std::basic_string<char> &&’)
    8 |     return x * x; // The error happens here
      |            ~ ^ ~
/usr/include/c++/12/complex:434:5: note: candidate: 'template<class _Tp> std::complex<_Tp> std::operator*(const std::complex<_Tp>&, const std::complex<_Tp>&)'
  434 |     operator*(const complex<_Tp>& __x, const complex<_Tp>& __y)

      |     ^~~~~~~~
/usr/include/c++/12/complex:434:5: note:   template argument deduction/substitution failed:
tmp/main.cpp:8:12: note:   'std::basic_string<char>' is not derived from 'const std::complex<_Tp>'
    8 |     return x * x;
      |            ~ ^ ~
/usr/include/c++/12/valarray:1193:5: note: candidate: 'template<class _Tp> std::valarray<_Tp> std::operator*(const std::valarray<_Tp>&, const std::valarray<_Tp>&)'
 1193 |     operator*(const valarray<_Tp>& __v, const valarray<_Tp>& __w)

      |     ^~~~~~~~
/usr/include/c++/12/valarray:1193:5: note:   template argument deduction/substitution failed:
tmp/main.cpp:8:12: note:   'std::basic_string<char>' is not derived from 'const std::valarray<_Tp>'
    8 |     return x * x;
      |            ~ ^ ~
/usr/include/c++/12/bits/stl_iterator.h:436:5: note: candidate: 'template<class _Iterator> constexpr std::reverse_iterator<_Iterator> std::operator*(typename std::reverse_iterator<_Iterator>::difference_type, const std::reverse_iterator<_Iterator>&)'
  436 |     operator*(typename reverse_iterator<_Iterator>::difference_type __n,

      |     ^~~~~~~~
/usr/include/c++/12/bits/stl_iterator.h:436:5: note:   template argument deduction/substitution failed:
tmp/main.cpp:8:12: note:   'std::basic_string<char>' is not derived from 'const std::reverse_iterator<_Iterator>'
    8 |     return x * x;
      |            ~ ^ ~
```

------

## 2. The Solution (C++20 Concepts)

This version uses a `concept` to define requirements. The compiler fails immediately at the call site in `main`, resulting in a much cleaner message.

## The Code

```cpp
#include <iostream>
#include <string>
#include <concepts> // Required for std::same_as

// 1. Define the concept
template <typename T>
concept Multiplicable = requires(T a, T b) {
    { a * b } -> std::same_as<T>; // Requires that a * b exists and returns T
};

// 2. Constrain the template function using the concept
template <Multiplicable T>
T square(T x) {
    return x * x;
}

int main() {
    std::string s = "hello";
    
    // ERROR: caught at the "gate"
    auto result = square(s); 

    return 0;
}
```

## Full Error Message

```text
test.cpp: In function 'int main()':
test.cpp:21:24: error: no matching function for call to 'square(std::string&)'
   21 |     auto result = square(s);

      |                   ~~~~~~^~~

test.cpp:13:3: note: candidate: 'template<class T>  requires  Multiplicable<T> T square(T)'
   13 | T square(T x) {
      |   ^~~~~~
test.cpp:13:3: note:   constraints not satisfied
test.cpp:8:9: note: because 'std::string' {aka 'std::__cxx11::basic_string<char>'} 
      does not satisfy 'Multiplicable'
    8 | concept Multiplicable = requires(T a, T b) {

      |         ^~~~~~~~~~~~~
test.cpp:9:9: note: the required expression '(a * b)' is invalid
    9 |     { a * b } -> std::same_as<T>;
      |       ~~^~~
```