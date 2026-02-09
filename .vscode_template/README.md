# VS Code Templates

These templates provide starter VS Code configs for macOS, Linux, and Windows.
Each template includes launch configurations for both the `denigma` app and `denigma_tests`.

## Suggested extensions
- C/C++ (`ms-vscode.cpptools`)
- CMake (`twxs.cmake`)
- CMake Tools (`ms-vscode.cmake-tools`)
- CodeLLDB (`vadimcn.vscode-lldb`) (macOS)

## Setup

1. Pick the template folder for your OS:
   - `macos/`
   - `linux/`
   - `windows/`
2. Copy the files into your workspace `.vscode/` directory:

```bash
cp .vscode_template/<os>/*.json .vscode/
```

3. Adjust launch arguments and paths for your local environment as needed.

Notes:
- App launch config targets `build/build/Debug/denigma` (or `.exe` on Windows).
- Test launch config targets `build/tests/denigma_tests` (or `build\\tests\\Debug\\denigma_tests.exe` on Windows).
- Build task uses the existing `build` directory.

## Optional: local dependency overrides

If you are editing the dependencies simultaneosly, create `.vscode/settings.json` with:

```json
{
    "cmake.configureArgs": [
        "-DMUSX_LOCAL_PATH=/path/to/musxdom",
        "-DMNXDOM_LOCAL_PATH=/path/to/mnxdom",
        "-DSMUFL_MAPPING_LOCAL_PATH=/path/to/smufl-mapping"
    ]
}
```
