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

1. Install the following extensions:
   - C/C++ (from Microsoft)
   - C/C++ Extension Pack (from Microsoft)
   - C/C++ Themes (from Microsoft)
   - CMake (from twxs)
   - CMake Tools (from Microsoft)
   - codeLLDB (from Vadim Chugunov)
2. Use the following `.vscode/tasks.json` file:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build",
            "type": "shell",
            "command": "cmake --build build",
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
    ]
}
```

3. Use the following `.vscode/launch.json` for debugging on macOS:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "C++ Debug (codeLLDB)",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/build/build/Debug/denigma",
            "args": [], // specify command line arguments here for testing
            "cwd": "${workspaceFolder}",
            "stopOnEntry": false,
            "env": {},
            "preLaunchTask": "build" // Optional: specify a task to build your program before debugging
        }
    ]
}
```

on Windows:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "C++ Debug (codeLLDB)",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/build/Debug/denigma.exe",
            "args":[],  // specify command line arguments here for testing
            "console": "externalTerminal",
            "cwd": "${workspaceFolder}",
            "stopAtEntry": false,
            "environment": [],
            "preLaunchTask": "build" // Optional: specify a task to build your program before debugging
        }
    ]
}
```

on Ubuntu:

```json
{
    "version": "0.2.0",
    "configurations": [
      {
        "name": "Debug",
        "type": "cppdbg",
        "request": "launch",
        "program": "${workspaceFolder}/build/build/Debug/denigma",
        "args": [],  // specify command line arguments here for testing
        "stopAtEntry": false,
        "cwd": "${workspaceFolder}",
        "environment": [],
        "externalConsole": true,
        "MIMode": "gdb",
        "setupCommands": [
          {
            "description": "Enable pretty-printing for gdb",
            "text": "-enable-pretty-printing",
            "ignoreFailures": true
          }
        ],
        "preLaunchTask": "Build"
      }
    ]
}
```
