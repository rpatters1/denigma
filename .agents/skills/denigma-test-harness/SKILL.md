---
name: denigma-test-harness
description: Denigma repository test harness conventions. Use when building Denigma, running Denigma tests, interpreting Denigma test failures, or choosing the correct working directory for the test executable.
---

# Denigma Test Harness

Run the test executable from `tests/data`, not from the repository root.

The GoogleTest binary expects the current working directory basename to be `data` and resolves inputs/outputs relative to that directory. Running it from the repository root causes broad false failures such as missing input files, `Unknown or misplaced option: --mnx`, or assertions from `tests/denigmatests.cpp` that the cwd is not `data`.

Preferred commands:

```bash
cmake --build build
```

```bash
../../build/tests/denigma_tests
```

Run the second command with working directory:

```text
tests/data
```

`ctest --test-dir build` may report no registered tests even when `build/tests/denigma_tests` exists, so run the executable directly unless CTest registration has been verified.
