# AGENTS.md

Repository guidance for agents working in `denigma`.

## Purpose

`denigma` is a C++23 CMake project that converts Finale MUSX content into Enigma XML and related formats.
The repository builds a CLI plus reusable libraries for classification, massage, export, and format conversion.

## Project Layout

- `src/core` contains shared domain code and the main library entry points.
- `src/classify` contains clef, articulation, dynamic, expression, and jump classification helpers.
- `src/formats/enigmaxml`, `src/formats/mnx`, `src/formats/mss`, and `src/formats/svg` contain the format-specific converters.
- `src/massage` contains MusicXML transformation helpers.
- `src/export` contains export-related code shared by tests and production targets.
- `src/io` and `src/utils` contain lower-level helpers.
- `tests` contains the GoogleTest suite and fixture data.
- `tests/data/inputs` contains checked-in input fixtures.
- `tests/data/inputs/reference` contains checked-in expected-output fixtures.
- `tests/data/outputs` contains generated output artifacts and should be treated as disposable unless a test update explicitly requires it.

## Build Rules

- Use an out-of-source build only. The top-level `CMakeLists.txt` rejects in-source builds.
- The normal build entry point is `build.cmake`:
  - `cmake -P build.cmake`
  - `./build.cmake`
- To clean the build tree:
  - `cmake -P build.cmake -- clean`
  - `./build.cmake -- clean`
- The build downloads third-party dependencies through `FetchContent`, including `pugixml`, `nlohmann_json`, `zlib`, and `googletest`.
- If you need a local MUSX DOM checkout, set `MUSX_LOCAL_PATH` in CMake rather than editing dependency logic.

## Test Rules

- Build the test target through CMake, then run the test binary from `tests/data`.
- The test executable is `denigma_tests` and is emitted under `build/tests`.
- Preferred test flow:
  - `cmake --build build`
  - `../../build/tests/denigma_tests`
- Run `../../build/tests/denigma_tests` with `tests/data` as the working directory.
- The GoogleTest binary expects the current working directory basename to be `data`; running it from the repository root causes broad false failures.
- `ctest --test-dir build` may report no registered tests even when `build/tests/denigma_tests` exists.
- `ctest --test-dir build/tests` can be used only after confirming the discovered tests have the correct `WORKING_DIRECTORY` registration.
- When investigating a focused regression, prefer a narrow GoogleTest filter on the direct executable before a full suite.
- Use `ctest -R ...` only after confirming CTest registration and working directories are correct.
- Test fixtures are intentionally broad and include generated comparison files. Update them only when the behavioral change is intended and verified.

## Editing Rules

- Keep changes localized to the narrowest relevant library or test area.
- Do not modify generated artifacts unless the task explicitly requires regenerating expected outputs.
- If a change affects converter behavior, update the corresponding reference fixtures under `tests/data/inputs/reference` and verify the diff carefully.
- Preserve the existing CMake target structure and naming conventions.
- Do not remove or rewrite third-party dependency wiring unless the task is specifically about build configuration.
- Prefer a local lambda over a file-scope one-off helper when the logic is only used in one function and does not improve readability as a named abstraction.
- Do not place project-internal design notes in top-level `docs`; that directory is primarily for Doxygen/external-library documentation. Keep implementation notes near the relevant source area unless asked otherwise.

## Verification

- For source changes, at minimum run the relevant targeted test subset.
- For converter or fixture changes, run the most specific affected test target first, then widen to `ctest` if needed.
- If you change build logic, verify both configure and build steps still succeed.

## Practical Notes

- The repository is cross-platform but currently has macOS-specific logic in the top-level CMake file.
- Warnings are treated as errors in both production and test builds.
- The project targets C++23 by default, with a minimum supported standard of C++20.
