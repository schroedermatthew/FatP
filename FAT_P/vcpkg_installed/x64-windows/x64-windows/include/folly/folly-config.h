/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#define FOLLY_HAS_LIBURING 0
#define FOLLY_HAS_LIBAIO 0

#ifdef __APPLE__
#include <TargetConditionals.h> // @manual
#endif

#if !defined(FOLLY_MOBILE)
#if defined(__ANDROID__) || \
    (defined(__APPLE__) &&  \
     (TARGET_IPHONE_SIMULATOR || TARGET_OS_SIMULATOR || TARGET_OS_IPHONE))
#define FOLLY_MOBILE 1
#else
#define FOLLY_MOBILE 0
#endif
#endif // FOLLY_MOBILE

/* #undef FOLLY_HAVE_PTHREAD */
/* #undef FOLLY_HAVE_PTHREAD_ATFORK */

#define FOLLY_HAVE_LIBGFLAGS 1

#define FOLLY_HAVE_LIBGLOG 1

/* #undef FOLLY_USE_JEMALLOC */

#if __has_include(<features.h>)
#include <features.h>
#endif

/* #undef FOLLY_HAVE_ACCEPT4 */
#define FOLLY_HAVE_GETRANDOM 0
/* #undef FOLLY_HAVE_PREADV */
/* #undef FOLLY_HAVE_PWRITEV */
/* #undef FOLLY_HAVE_CLOCK_GETTIME */
/* #undef FOLLY_HAVE_PIPE2 */

/* #undef FOLLY_HAVE_IFUNC */
#define FOLLY_HAVE_UNALIGNED_ACCESS 1
/* #undef FOLLY_HAVE_VLA */
#define FOLLY_HAVE_WEAK_SYMBOLS 0
/* #undef FOLLY_HAVE_LINUX_VDSO */
/* #undef FOLLY_HAVE_MALLOC_USABLE_SIZE */
/* #undef FOLLY_HAVE_INT128_T */
#define FOLLY_HAVE_WCHAR_SUPPORT 1
/* #undef FOLLY_HAVE_EXTRANDOM_SFMT19937 */
/* #undef HAVE_VSNPRINTF_ERRORS */

/* #undef FOLLY_HAVE_LIBUNWIND */
/* #undef FOLLY_HAVE_DWARF */
/* #undef FOLLY_HAVE_ELF */
/* #undef FOLLY_HAVE_SWAPCONTEXT */
/* #undef FOLLY_HAVE_BACKTRACE */
/* #undef FOLLY_USE_SYMBOLIZER */
#define FOLLY_DEMANGLE_MAX_SYMBOL_SIZE 1024

/* #undef FOLLY_HAVE_SHADOW_LOCAL_WARNINGS */

/* #undef FOLLY_HAVE_LIBLZ4 */
#define FOLLY_HAVE_LIBLZMA 1
/* #undef FOLLY_HAVE_LIBSNAPPY */
#define FOLLY_HAVE_LIBZ 1
/* #undef FOLLY_HAVE_LIBZSTD */
/* #undef FOLLY_HAVE_LIBBZ2 */

#define FOLLY_LIBRARY_SANITIZE_ADDRESS 0

/* #undef FOLLY_SUPPORT_SHARED_LIBRARY */

#define FOLLY_HAVE_LIBRT 0

#define FOLLY_HAVE_VSOCK 0
