# Testing and verification

Applies when designing tests, selecting checks, or interpreting their results.
The profile defines the actual harness, supported configurations, and acceptance
requirements. The core owns the rules for verification claims.

## Test the contract

- Exercise externally meaningful behavior, invariants, and failure handling.
  Avoid tests that only repeat implementation steps or assert incidental layout.
- For defect fixes, use a regression that distinguishes the faulty behavior from
  the intended contract when a practical automated test exists. Otherwise record
  the concrete reproduction and the verification limitation.
- Select boundary, empty, invalid-input, lifecycle, and concurrency cases based
  on the component's actual risks. Do not demand irrelevant categories as ritual.
- Use independent expected results where feasible. Reusing the implementation's
  algorithm as the oracle can reproduce the same defect in both places.
- Keep cases reproducible: isolate mutable fixtures, control randomness, record
  seeds, and use bounded waits with useful diagnostics for asynchronous work.

## Keep the test mechanism trustworthy

- Use the established harness and discovery mechanism. Ensure a newly added case
  is registered and actually selected by the command being claimed as evidence.
  An empty discovery result is not success for a suite expected to contain tests.
- Assertions must remain effective in the configurations used for verification.
  Expected exceptions must not swallow an assertion failure or accept an unrelated
  exception as proof of the intended behavior.
- Define numerical comparisons: absolute and relative tolerance, behavior near
  zero, NaN, and infinities. Unexpected NaN must fail. Do not assume a comparison
  that only rejects differences greater than a tolerance also rejects NaN.
- An expected-failure or compile-fail check must establish the intended rejection.
  Validate prerequisites and a corresponding valid control under the same relevant
  configuration. Inspect the expected diagnostic or failure category; a missing
  include, failed tool invocation, or unrelated link error is not proof.
- Do not disable a check, expand a tolerance, or change an expected result merely
  to obtain a pass. Any intended contract change needs its own justification.

## Match the evidence to the claim

Choose unit, integration, end-to-end, manual, static, sanitizer, or deployment
checks according to the changed behavior and the profile's requirements.

Distinguish syntax checks from builds, builds from executions, and substitutes
from real dependencies. A stub, mock, emulator, or development machine can answer
useful questions without proving real integration, hardware behavior, or deployed
performance. Identify that boundary explicitly.

Source inspection can establish some properties that runtime cases do not, and
runtime tests can expose behavior that inspection misses. Use both where warranted;
do not claim either proves every architectural requirement.

## Complete the required verification

Run focused checks while iterating, then the acceptance checks required for the
change's scope. Broaden the checks when shared dependencies or configurations are
affected. Do not claim a full gate passed from a passing subset.

Report failures, skips, unavailable environments, and unexecuted checks separately.
Keep relevant diagnostics and state what remains unresolved. For low-impact
documentation or formatting changes, direct inspection, link validation, and the
configured format check may be sufficient; do not add tests that merely mirror text.
