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

The documentation site for the library API is [https://rpatters1.github.io/denigma/](https://rpatters1.github.io/denigma/).

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

The companion [denigma-examples](https://github.com/rpatters1/denigma-examples) repository demonstrates this from separate native and WebAssembly projects using CMake `FetchContent` or a local Denigma checkout.

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

## Visual Studio Code setup

See [`.vscode_template/README.md`](.vscode_template/README.md) for OS-specific templates (`macos`, `linux`, `windows`) with `launch.json` and `tasks.json`.
