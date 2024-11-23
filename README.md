# musx2mnx command line utility

This convert as  extracts the Enigma XML from a Finale `.musx` file. It has no external dependencies.

## Setup Instructions

Clone the GitHub repository and clone all submodules.

### macOS-Specific

Install the latest cmake:

```bash
brew install cmake
```

---

### Windows-Specific

You must install cmake. The easiest way is with a package manager such as Choclatey (`choco`).

[Choclatey install instructions](https://chocolatey.org/install)

Install the latest cmake

```bat
choco install cmake
```
---

## Build Instructions


```bash
cmake -P build.cmake
```musx2xml

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

3. Use the following `.vscode/launch.json` for debugging:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "C++ Debug (codeLLDB)",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/build/build/musx2mnx",
            "args": [], // specify command line arguments here for testing
            "cwd": "${workspaceFolder}",
            "stopOnEntry": false,
            "env": {},
            "preLaunchTask": "build" // Optional: specify a task to build your program before debugging
        }
    ]
}
```
