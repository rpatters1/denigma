---
name: string-lookups
description: Denigma convention for hard-coded string-to-value lookups and dispatch. Use when adding, editing, reviewing, or refactoring C++ code that compares an input string against multiple string literals.
---

# String Lookups

Prefer a function-local `static const std::unordered_map` when hard-coded strings select values or behavior. A lookup avoids accumulating a linear sequence of repeated string comparisons, especially on the common negative path where an `if` chain must exhaust every option.

```cpp
static const std::unordered_map<std::string_view, Value> values = {
    { "first", Value::First },
    { "second", Value::Second }
};
const auto found = values.find(text);
return found != values.end() ? std::optional{ found->second } : std::nullopt;
```

An `if` sequence is acceptable for two or three comparisons when it is clearer. Also keep conditionals when cases require non-equality tests, different side effects, or other logic that a lookup table would obscure.

Apply this convention to code already being changed. Do not refactor unrelated lookup code solely to conform.
