---
name: optional-usage
description: Denigma std::optional usage conventions. Use when adding, reviewing, or modifying std::optional fields or return values, especially for enum or bool state.
---

# Optional Usage

Prefer explicit domain states over `std::optional` when the missing state is part of the model.

Rules:

- When Denigma controls an enum type, avoid `std::optional<Enum>` when possible.
- Add a first enum value such as `Unspecified`, `Float`, `NotApplicable`, or the best domain-specific unengaged value.
- Use that first enum value to represent the otherwise not-engaged state.
- If Denigma does not control the enum, `std::optional<Enum>` may be appropriate when the enum has no value that corresponds to not engaged.
- Apply the same rule to `std::optional<bool>`.
- Prefer a new or existing three-state enum over `std::optional<bool>` when the state is really yes / no / unengaged.
- Keep the unengaged enum value first so default construction is meaningful.
