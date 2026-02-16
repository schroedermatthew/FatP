# Contributing to FAT-P

## How This Project Works

FAT-P is developed through a multi-AI collaborative pipeline. All code, architecture, documentation, and governance are authored by AI systems — primarily Claude (Anthropic), with contributions from ChatGPT, Gemini, and Grok. The human maintainer provides direction, judgment, and accountability but does not write code.

This means the contribution model is different from a typical open-source project. Please read this before opening issues or pull requests.

## Bug Reports — Always Welcome

If you find a bug, please open an issue. The more specific you are, the faster it gets fixed. A good bug report includes:

- **What you observed** — the actual behavior
- **What you expected** — the correct behavior
- **A minimal reproducer** — the smallest code that demonstrates the problem
- **Your environment** — compiler, version, OS, build flags

If you can identify the root cause in the source, even better. Quote the specific lines and explain why they're wrong.

**Example of a useful bug report:**

```
Bug: CircularBuffer::size() returns incorrect value after wrap-around

Environment: GCC 13.2, Ubuntu 24.04, -O2 -std=c++20

Reproducer:
  fat_p::CircularBuffer<int, 4> buf;
  buf.push_back(1); buf.push_back(2); buf.push_back(3); buf.push_back(4);
  buf.pop_front();
  buf.push_back(5);
  assert(buf.size() == 4);  // Fails: size() returns 1

Evidence: Line 287 computes size as (mTail - mHead), which wraps
incorrectly when mTail < mHead.
```

## Feature Requests — Welcome With Context

If you think FAT-P should include a component it doesn't have, or an existing component should gain a capability, open an issue. Useful context includes:

- **The problem you're solving** — not just "add X" but "I need X because Y"
- **What you currently use instead** — and what's wrong with it
- **Competitors** — if other libraries solve this, name them so we can benchmark

Feature requests that align with the project's design philosophy (C++20, header-only, zero dependencies, HPC-oriented, policy-based) are more likely to be adopted.

## Pull Requests — Read This First

FAT-P code goes through a specific development pipeline: parallel design across multiple AI systems, cross-review and synthesis, adversarial review, context reset cycles, and human judgment. This process is described in [the methodology document](Read_Me/Fat-P_AI_Collaborative_Development_Methodology.md).

Because of this pipeline, **unsolicited code PRs are unlikely to be merged directly.** This is not a judgment on code quality — it's a process constraint. Code that enters the repository needs to go through the same multi-AI review and context reset cycle as everything else.

What *will* happen with a good PR:

- The problem it solves will be acknowledged
- The approach will be evaluated and may influence the AI-pipeline implementation
- If the fix is correct and targeted (e.g., a one-line bug fix with a reproducer), it may be merged directly
- Credit will be given in the commit message regardless of the merge path

**PRs that are likely to be merged directly:**

- Bug fixes with a reproducer and evidence
- Build system fixes (CMake, CI workflow corrections)
- Documentation typos and factual corrections
- Test cases that expose real bugs

**PRs that will go through the pipeline instead:**

- New components or major features
- Architectural changes
- API redesigns
- Performance optimizations (these need benchmark verification across compilers and platforms)

## Code Style

If you do submit code, the project uses:

- C++20 minimum
- Allman braces, 4-space indent, 100-column target (120 hard limit)
- `mPascalCase` for class members, `camelCase` for functions
- `#pragma once`, no `#ifndef` guards
- All code in `namespace fat_p`

The full coding standards are in [Development Guidelines](Read_Me/Fat-P_Library_Development_Guidelines.md). A `.clang-format` file is provided at the repository root.

## Questions and Discussion

Open an issue. There is no mailing list, Discord, or forum at this time.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
