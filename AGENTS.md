# AGENTS.md

Repository guidance for agents working in `denigma`.

## Skills

This repository's conventions live in `.agents/skills/<name>/SKILL.md`. They are not
stored under any single agent's own directory, so read them from `.agents/skills`
regardless of which agent you are. A skill overrides general practice for the work it
covers, so read the relevant one before starting that kind of work rather than after.

- `accidental-style` — rendering accidentals in names: Unicode for exported content, ASCII for output filenames and log messages.
- `classifier-design` — creating or editing classifiers in `src/classify`, or shared classification helpers used by exporters.
- `code-comments` — writing or revising a Doxygen comment in a public header, an implementation comment anywhere in `src`, or reviewing the comments in a change.
- `denigma-test-harness` — building, running tests, interpreting test failures, or choosing the test executable's working directory.
- `enum-mappings` — adding or modifying enum conversions in any `<exporter>_enums.cpp`.
- `marking-categories` — reading any field that both a musx `MarkingCategory` and a `TextExpressionDef`/`ShapeExpressionDef` carry (positioning, fonts, `useCategoryPos`/`useCategoryFonts`).
- `mnx-optional-export` — exporting MNX `OPTIONAL` and `OPTIONAL_WITH_DEFAULT` properties, with a preference for omitting values when possible.
- `nested-namespaces` — any C++ namespace declaration.
- `optional-usage` — `std::optional` fields or return values, especially for enum or bool state.
- `string-lookups` — hard-coded string-to-value lookups or dispatch, especially repeated literal comparisons.
- `windows-minmax` — any call to `std::min`, `std::max`, `std::clamp`, or `numeric_limits<T>::min`/`max`.

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
- `src/wasm` contains the WebAssembly C ABI wrapper built by the `denigma_wasm` target.
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
- The WebAssembly module (`src/wasm`, target `denigma_wasm`, option `denigma_BUILD_WASM`) is built with Emscripten:
  - `emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=MinSizeRel -DDENIGMA_CXX_STANDARD=20`
  - `cmake --build build-wasm --target denigma_wasm`
  - `node tests/wasm/smoke.mjs build-wasm/wasm/denigma.js build-wasm/wasm/denigma.wasm`
- Always name the `denigma_wasm` target for that build. The `all` target also compiles the text-measuring converters and `denigma_textmetrics`, which the module does not link and which do not compile under Emscripten.
- The exported function list in `src/wasm/CMakeLists.txt` and the C ABI in `src/wasm/denigma_wasm.cpp` are the contract with `denigma-online` and `viritura`, which consume the module built from a pinned Denigma commit. Changing either changes those consumers.

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
- A `*-ref.musicxml` fixture is Finale's own export of the neighboring `.musx`. Finale is discontinued, so these cannot be regenerated indefinitely and are kept as reference material whether or not a test currently loads one. Never delete one for appearing unused, and never regenerate one to match Denigma's output; the point is that it records what Finale produced.

## Editing Rules

- Keep changes localized to the narrowest relevant library or test area.
- Do not modify generated artifacts unless the task explicitly requires regenerating expected outputs.
- If a change affects converter behavior, update the corresponding reference fixtures under `tests/data/inputs/reference` and verify the diff carefully.
- Preserve the existing CMake target structure and naming conventions.
- Do not remove or rewrite third-party dependency wiring unless the task is specifically about build configuration.
- Prefer a local lambda over a file-scope one-off helper when the logic is only used in one function and does not improve readability as a named abstraction.
- Prefer horizontal function calls up to about 135 columns. When wrapping is needed, keep as much of the call as practical on the next indented line.
- Strongly prefer named constants, existing domain constants, or computed values over hardcoded numeric literals other than `0`.
- Do not place project-internal design notes in top-level `docs`; that directory is primarily for Doxygen/external-library documentation. Keep implementation notes near the relevant source area unless asked otherwise.
- Record deferred feature work in the relevant `roadmap.md`, concrete third-party API limitations in the matching gaps document, such as `src/formats/musicxml/mx-api-gaps.md`, and deliberate policy choices in the matching design-decisions document, such as `src/formats/musicxml/design-decisions.md`. A decision recorded there is settled; reverse it by deleting the entry, not by leaving it to contradict the code. Findings that fit none of those, notably how other applications treat output believed correct, go in the matching implementation-notes document, such as `src/formats/musicxml/implementation_notes.md`.
- Write code that compiles at the minimum supported C++ standard, not merely at the one this repository builds with. The default build selects C++23, but consumers may select the minimum: `denigma-online` forces C++20 through `DENIGMA_CXX_STANDARD`. A construct requiring a newer standard therefore passes the local build and breaks them. Where a newer feature is genuinely needed, raise the minimum deliberately rather than by accident.
- Do not leave a `/// @todo` comment pointing at a roadmap item. Roadmap work may never be done, and the comment becomes clutter. Reserve `/// @todo` for a specific limitation local to the code it sits in.

## Verification

- For source changes, at minimum run the relevant targeted test subset.
- For converter or fixture changes, run the most specific affected test target first, then widen to `ctest` if needed.
- If you change build logic, verify both configure and build steps still succeed.
- The default build cannot catch a violation of the minimum C++ standard, because it compiles at the newer one. When a change uses a recent library or language feature, build it at the minimum as well, for instance by configuring `denigma-online` against the local checkout.

## Practical Notes

- The repository is cross-platform but currently has macOS-specific logic in the top-level CMake file.
- Warnings are treated as errors in both production and test builds.
- The project targets C++23 by default, with a minimum supported standard of C++20. Both matter; see the editing rule on the minimum standard.
