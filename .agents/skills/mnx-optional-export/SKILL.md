---
name: mnx-optional-export
description: MNX exporter conventions for serializing mnxdom OPTIONAL and OPTIONAL_WITH_DEFAULT properties. Use when adding, reviewing, or refactoring Denigma MNX output so semantically omittable values are omitted from JSON whenever the MNX DOM API permits it.
---

# MNX Optional Export

## Overview

Prefer the smallest valid MNX JSON. When a `mnxdom` property can be omitted without changing its meaning, export it in its omitted form rather than serializing an explicit default or empty value. This applies especially to properties declared with `MNX_OPTIONAL_PROPERTY_WITH_DEFAULT` and to values represented by `std::optional` for `MNX_OPTIONAL_PROPERTY`.

## Property rules

### `MNX_OPTIONAL_PROPERTY_WITH_DEFAULT`

Use `set_or_clear_NAME(value)`, not `set_NAME(value)`, for exporter values that may equal the MNX default:

```cpp
mnxObject.set_or_clear_symbol(enumConvert<mnxdom::BreathMarkSymbol>(source.type));
```

The generated `set_or_clear_` method clears the JSON property when `value == DEFAULT`; otherwise it serializes the value. This is the required pattern for MNX values such as `Auto`, `false`, `0`, and other declared defaults.

Use `set_NAME(value)` only when the explicit default is semantically required by the source or by a deliberate exporter policy. Document that policy near the call if it is not obvious.

### `MNX_OPTIONAL_PROPERTY`

These properties have no DOM-level default. Set them only when the source contains a meaningful value; otherwise leave them absent or call `clear_NAME()` when reusing an object.

```cpp
if (sourceValue) {
    mnxObject.set_name(*sourceValue);
}
```

Do not invent a sentinel value merely to populate an optional property. Keep the property omitted when the source value is unavailable and omission is schema-valid.

## Export workflow

When adding or changing MNX output:

1. Inspect the `mnxdom` declaration to determine whether the target is `MNX_OPTIONAL_PROPERTY_WITH_DEFAULT` or `MNX_OPTIONAL_PROPERTY`, and identify its exact default.
2. Convert the source value in the exporter-specific enum mapping when an enum conversion is needed.
3. Use `set_or_clear_NAME` for defaulted properties; use conditional `set_NAME` for non-defaulted optional properties.
4. Verify serialized JSON, not only the typed DOM value. Confirm default-valued properties are absent and non-default values remain present.
5. Add or update focused exporter tests when the omission behavior is observable or regression-prone.

Do not confuse a DOM accessor's returned default with a serialized property. A defaulted accessor can report `Auto` even when the JSON key is absent; that absent key is usually the preferred export.
