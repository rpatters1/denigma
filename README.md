# denigma command line interface (cli) utility

This utility extracts and converts Enigma XML from a Finale `.musx` file. It is a successor to the original denigma project by [Chris Roode](https://github.com/chrisroode).

**This project is not affiliated with or endorsed by Finale or its parent company.**

- It is an independent open-source utility designed to help users access and convert their own data in the absence of Finale, which has been discontinued.
- It does not contain any Finale source code.
- It is not capable of writing Finale files, only reading them.
- Nothing in this repository circumvents digital copy protection on the Finale application.

## Library layout

Denigma is split into small reusable libraries as well as the CLI utility:

- `denigma_classify` provides independent classification helpers for clefs, articulations, dynamics, and jumps.
- `denigma_format_enigmaxml` handles MUSX archive extraction and EnigmaXML pass-through.
- `denigma_format_mnx` converts MUSX content to MNX.
- `denigma_format_mss` converts MUSX content to MSS.
- `denigma_format_musicxml` converts MUSX content to MusicXML.
- `denigma_format_svg` converts MUSX content to SVG.

The documentation site for the library API is [https://openmusx.github.io/denigma/](https://openmusx.github.io/denigma/).

## Linking

Denigma can also be consumed as libraries from another CMake project. Link only the narrow library or libraries you need:

```cmake
target_link_libraries(my_tool PRIVATE denigma::classify)
target_link_libraries(my_tool PRIVATE denigma::mnx)
target_link_libraries(my_tool PRIVATE denigma::mss)
target_link_libraries(my_tool PRIVATE denigma::musicxml)
target_link_libraries(my_tool PRIVATE denigma::svg)
target_link_libraries(my_tool PRIVATE denigma::enigmaxml)
```

Use `denigma::classify` when you only need classification helpers. Use one of the format targets when you need a specific converter.

Applications may register linked formats and call `ConverterRegistry::convert` when runtime format selection or owned output buffers are useful:

```cpp
#include "denigma/formats/mnx.h"

denigma::ConverterRegistry registry;
denigma::formats::mnx::registerConverters(registry);

denigma::BufferRandomAccessReader input(musxBytes);
denigma::formats::mnx::Options options;
options.common.sourceName = "score.musx";

auto artifact = registry.convert(
	denigma::FormatId::Musx,
	denigma::FormatId::MnxJson,
	input,
	denigma::ConversionRequest{ &options });
```

`ConversionArtifact` owns the generated documents in emission order and preserves the converter's `ConversionResult`. Multi-output converters retain each suggested filename. The format-specific typed converters and their `convert` overloads remain a first-class alternative.

The companion [denigma-examples](https://github.com/openmusx/denigma-examples) repository demonstrates this from separate native and WebAssembly projects using CMake `FetchContent` or a local Denigma checkout.

When Denigma is added as a CMake subproject, its CLI and tests are disabled by default. Consumers can override either option before making Denigma available:

```cmake
set(denigma_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(denigma_BUILD_TESTING OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
	denigma
	GIT_REPOSITORY https://github.com/openmusx/denigma.git
	GIT_TAG        <release-tag-or-commit>
)
FetchContent_MakeAvailable(denigma)
```

Standalone Denigma builds continue to build the CLI and tests by default.

## Command line usage

Use the `--help` option to get a full list of commands:

```
denigma --help
```

## Setup instructions

Clone the GitHub repository and clone all submodules.

### macOS-Specific

Install the latest cmake:

```bash
brew install cmake
brew install ninja
```

---

### Windows-Specific

You must install cmake and xxd. The easiest way is with a package manager such as Choclatey (`choco`).

[Choclatey install instructions](https://chocolatey.org/install)

Install the latest cmake and xxd

```bat
choco install cmake
choco install ninja
choco install xxd
```
---

## Build instructions


```bash
cmake -P build.cmake
```

or (for Linux or macOS)

```bash
./build.cmake
```
---

You can clean the build directory with

```bash
cmake -P build.cmake -- clean
```

or (for Linux or macOS)

```bash
./build.cmake -- clean
```

## WebAssembly build

Denigma also builds a WebAssembly module exposing MUSX inspection and conversion to EnigmaXML, MusicXML, and MNX through a small C ABI (`src/wasm/denigma_wasm.cpp`). It is the module that [denigma-online](https://github.com/openmusx/denigma-online) runs in the browser. Building it requires [Emscripten](https://emscripten.org/) (CI uses 5.0.7):

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=MinSizeRel -DDENIGMA_CXX_STANDARD=20
cmake --build build-wasm --target denigma_wasm
node tests/wasm/smoke.mjs build-wasm/wasm/denigma.js build-wasm/wasm/denigma.wasm
```

Build the `denigma_wasm` target rather than `all`: the module links only the EnigmaXML, MusicXML, and MNX converters, and naming the target keeps the text-measuring converters (SVG and MSS) and their font dependencies out of the build. Under Emscripten the CLI and tests are off by default and `denigma_BUILD_WASM` is on. The module lands in `build-wasm/wasm/` as `denigma.js` and `denigma.wasm`; CI builds and smoke-tests the module on every pull request, uploads the pair as the `denigma-wasm` artifact of every push to `main`, and attaches it to every published release as `denigma.<tag>.wasm.zip` alongside the native binaries.

## Visual Studio Code setup

See [`.vscode_template/README.md`](.vscode_template/README.md) for OS-specific templates (`macos`, `linux`, `windows`) with `launch.json` and `tasks.json`.
