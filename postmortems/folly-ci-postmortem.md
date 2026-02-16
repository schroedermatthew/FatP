# Folly CI Build Postmortem: Death by a Thousand Dependencies

## Background

FatP is a zero-dependency, header-only C++ library. It compiles with a single `g++ -std=c++20` invocation on any platform. The CI benchmark infrastructure, however, needs to build *competitor* libraries — Boost, Abseil, Folly, LLVM, EASTL — from source across GCC-14, Clang-17, and MSVC, caching them in GitHub Actions for reuse.

Folly (`facebook/folly`) proved to be the most painful dependency by far. What should have been a straightforward `git clone && cmake && make` turned into a multi-session debugging marathon spanning 8+ CI runs, each taking 15–30 minutes to fail. Every fix uncovered the next hidden problem. And this was an AI doing the debugging with full log access and search capabilities — imagine a human doing this on a home computer with no SSH access to the runner.

## The Cascade of Failures

Each issue below was only discoverable after the previous one was fixed, because each one killed the build at a different stage.

### 1. Missing `boost_regex`

Folly requires `boost_regex` as a compiled component. Our initial Boost build only included `context`, `filesystem`, `program_options`, and `thread`. CMake bailed at configure time.

**Fix:** Add `--with-regex` to the `b2` build command.

### 2. `boost_system` Stub Removed in Boost 1.89+

`boost::system` has been header-only since Boost 1.69. Boost 1.89 finally removed the stub library and its CMake config target. Folly's `folly-deps.cmake` still lists `system` as a REQUIRED component. With Boost 1.91 (latest `main`), cmake configure fails immediately. As of February 2026, the Folly team has not merged a fix (GitHub issue #2489).

**Fix:** `sed -i '/^\s*system\s*$/d' /tmp/folly-src/CMake/folly-deps.cmake` after cloning.

### 3. Boost Static/Shared Mismatch

Folly's CMake has an internal `BOOST_LINK_STATIC` variable that defaults to `OFF` on non-MSVC platforms. This silently overrides `-DBoost_USE_STATIC_LIBS=ON` — the standard CMake flag that every other project uses. CMake finds the static `.a` files, then rejects them.

**Fix:** Pass `-DBOOST_LINK_STATIC=ON` (Folly's own nonstandard cache variable) instead of the standard CMake flag.

### 4. `fast_float` Version Mismatch

Folly's `Conv.cpp` uses `fast_float::chars_format::allow_leading_plus`, introduced in fast_float v6.0. Ubuntu 24.04's `libfast-float-dev` ships v5.x. The compile error is a cryptic template failure deep inside `Conv.cpp` with no hint that it's a version issue.

**Fix:** Build fast_float v8.0.0 from source and add `/usr/local` to `CMAKE_PREFIX_PATH`.

### 5. `sudo cmake` Creates Root-Owned Build Artifacts

The fast_float source build uses `sudo cmake` (needed to install to `/usr/local`). This creates build artifacts owned by root in `/tmp/fast_float/build/`. The subsequent `rm -rf /tmp/fast_float` runs as the normal user, fails with "Permission denied" on every file, and `set -e` kills the entire step — before the Folly build even starts. The fast_float install was 100% successful; the cleanup command killed it.

**Fix:** `sudo rm -rf /tmp/fast_float`.

### 6. `libfast-float-dev` Doesn't Exist on Ubuntu 22.04

The Clang job originally ran on `ubuntu-22.04`. The apt package `libfast-float-dev` only exists in Ubuntu 24.04 (Noble). `apt-get install` fails at step 3, before any builds even begin. We didn't need the package anyway since we build from source, but it was left in the apt-get line.

**Fix:** Remove `libfast-float-dev` from both apt-get lines.

### 7. Ubuntu 22.04 glibc Missing POSIX Functions for Folly

After fixing the apt issue, Folly's cmake `check_function_exists()` calls failed for 8 basic POSIX functions on Ubuntu 22.04: `clock_gettime`, `accept4`, `getrandom`, `preadv`, `pwritev`, `pipe2`, `pthread_atfork`, and `malloc_usable_size`. Folly's `Time.cpp` then refuses to compile without `clock_gettime`. Ubuntu 24.04's newer glibc exposes these directly in libc; on 22.04 they require explicit `-lrt`/`-lpthread` that cmake's check macros don't provide.

**Fix:** Switch the Clang job from `ubuntu-22.04` to `ubuntu-24.04`. Install Clang-17 via `apt install clang-17` (available in Noble repos) instead of the `llvm.sh` script.

## Summary of All Patches

| # | Issue | Root Cause | Fix |
|---|-------|-----------|-----|
| 1 | Missing `boost_regex` | Not in `b2 --with-*` list | Add `--with-regex` |
| 2 | `boost_system` not found | Stub removed in Boost 1.89 | `sed` out `system` from `folly-deps.cmake` |
| 3 | Static Boost rejected | Folly overrides `Boost_USE_STATIC_LIBS` | Use `-DBOOST_LINK_STATIC=ON` |
| 4 | `allow_leading_plus` error | Distro `fast_float` too old | Build fast_float v8 from source |
| 5 | `rm` permission denied | `sudo cmake` creates root-owned files | `sudo rm -rf` |
| 6 | `libfast-float-dev` missing | Package doesn't exist on 22.04 | Remove from apt-get |
| 7 | POSIX functions not found | 22.04 glibc + cmake check limitations | Switch to ubuntu-24.04 |

## Lessons Learned

Every one of these failures required a full CI round-trip (15–30 minutes) to discover. None are documented in Folly's README. The library assumes you'll use their `getdeps.py` script which builds its own private copies of everything — great for Facebook's monorepo, useless for anyone integrating Folly as a system dependency.

Seven sequential, invisible, undocumented failures. An AI with full log parsing caught and fixed each one in about 4 hours of wall-clock time. A human developer doing this manually — reading raw CI logs, making one-line fixes, waiting 20 minutes, repeat — could easily burn a full day or two.

This is exactly the kind of dependency hell that FatP's zero-dependency design philosophy exists to avoid.

