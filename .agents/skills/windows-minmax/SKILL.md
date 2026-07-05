---
name: windows-minmax
description: Denigma Windows compatibility convention for standard min/max calls. Use when adding, reviewing, or modifying any C++ code that calls std::min, std::max, std::clamp, numeric_limits<T>::min/max, or related standard-library min/max functions or members.
---

# Windows Min/Max

Protect standard-library min/max names from Windows `min` / `max` macros.

Rules:

- When calling any standard-library `min` or `max` function or member, wrap the qualified function name in parentheses.
- Use `(std::min)(a, b)` and `(std::max)(a, b)`, not `std::min(a, b)` or `std::max(a, b)`.
- Use `(std::numeric_limits<T>::min)()` and `(std::numeric_limits<T>::max)()`, not `std::numeric_limits<T>::min()` or `std::numeric_limits<T>::max()`.
- Prefer the same protection for related standard-library APIs if their names can collide with Windows macros.
- Do this even when the current file builds on non-Windows platforms, because Denigma should build when `NOMINMAX` is not defined.
