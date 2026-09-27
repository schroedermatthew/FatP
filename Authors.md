# Authors

FAT-P is developed through human–AI collaboration. Its architecture includes both
human and AI contributions.

## Human Contributions

**Matthew Schroeder** — Direction, architecture, judgment, and accountability.

Matthew established the project's goals and constraints, drawing on his C++
experience and his interest in HPC and scientific computing. He also contributes
to the architecture, including the introduction of **BoneId** and related design
work in the [Skeleton family](include/fat_p/Skeleton.h).

His role includes choosing what to build, accepting or rejecting proposals,
correcting mistakes, and taking responsibility for the published library.
Architectural input is a substantive contribution even when AI writes the
implementation.

## AI Contributions

The project uses Claude (Anthropic), ChatGPT (OpenAI), Gemini (Google), and Grok
(xAI). Their contributions include:

- Proposing architectures, APIs, algorithms, and technical trade-offs.
- Writing and revising implementations, tests, benchmarks, and documentation.
- Reviewing designs and code, investigating failures, and verifying changes.
- Authoring and maintaining the guidelines that preserve decisions across sessions.

Claude has served as the primary implementation and documentation author, with
ChatGPT, Gemini, and Grok contributing designs, reviews, algorithms, testing
strategies, and API feedback. Contributions vary by task; these roles are not
exclusive.

## How the Collaboration Evolved

FAT-P began as an experiment in learning HPC and scientific computing while
exploring how much of a library AI systems could design and build. AI systems
originated substantial architecture and governance, including the policy-based
design, layer taxonomy, and FATP_META system described in the earlier project
account.

The introduction of BoneId and related human design contributions means that
account no longer describes the whole project's architectural authorship. Current
attribution recognizes both sources of design work. It does not transfer credit
for earlier AI-originated decisions to the human, or treat human architectural
input as merely approving AI proposals.

The [development methodology](documents/Fat-P_AI_Collaborative_Development_Methodology.md)
preserves the historical account. Its descriptions of an exclusively AI-authored
architecture and a human role limited to direction and judgment belong to that
earlier account. Current contributor instructions start at
[guidelines/README.md](guidelines/README.md).

## Attribution and Project Status

Credit should distinguish the origin of a design from the authorship of its
implementation, and preserve the contributions of both people and AI systems.
Vendored code in [ThirdParty/](ThirdParty/) retains its upstream authorship and
licenses.

FAT-P remains a prerelease project. Authorship does not establish correctness or
production readiness. The [current verification record](guidelines/CURRENT_VERIFICATION.md)
and linked workflow results describe checked configurations and their limits.
