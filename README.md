# denigma command line utility

This utility as extracts and converts Enigma XML from a Finale `.musx` file. It is a successor the original denigma project by [Chris Roode](https://github.com/chrisroode).

**This project is not affiliated with or endorsed by Finale or its parent company.**

- It is an independent open-source utility designed to help users access and convert their own data in the absence of Finale, which has been discontinued.
- It does not contain any Finale source code.
- It is not capable of writing Finale files, only reading them.
- Nothing in this repository circumvents digital copy protection on the Finale application.

Use the `--help` option to get a full list of commands:

```
denigma --help
```

## Setup Instructions

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

## Build Instructions


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

## Visual Studio Code Setup

See [`.vscode_template/README.md`](.vscode_template/README.md) for OS-specific templates (`macos`, `linux`, `windows`) with `launch.json` and `tasks.json`.
