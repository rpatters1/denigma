---
name: nested-namespaces
description: Denigma C++ namespace style. Use when adding, editing, reviewing, or refactoring C++ namespace declarations in this repository.
---

# Nested namespaces

Use nested namespace blocks, never C++17 concatenated namespace declarations.

```cpp
namespace denigma {
namespace classify {

// Declarations and definitions.

} // namespace classify
} // namespace denigma
```

Do not write `namespace denigma::classify {`.
