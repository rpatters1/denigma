---
name: enum-mappings
description: Denigma exporter enum mapping conventions. Use when adding, moving, reviewing, or modifying enum conversions for exporters such as MusicXML, MNX, MSS, SVG, or other format-specific code.
---

# Enum Mappings

When adding enum mappings for an exporter, put the mapping in that exporter's `<exporter>_enums.cpp` translation unit.

Use the existing enum conversion pattern:

```cpp
BEGIN_ENUM_CONVERSION(SourceEnum, TargetEnum)
    case SourceEnum::Value: return TargetEnum::value;
END_ENUM_CONVERSION
```

Rules:

- Implement mappings as `enumConvert` specializations through `BEGIN_ENUM_CONVERSION` and `END_ENUM_CONVERSION`.
- Include `src/formats/enum_conversion_macros.h` from the exporter enum TU for `DEFINE_ENUM_CONVERT_TEMPLATE`,
  `BEGIN_ENUM_CONVERSION`, and `END_ENUM_CONVERSION`.
- Expand `DEFINE_ENUM_CONVERT_TEMPLATE` and the conversion macros inside the namespace of the exporter converter.
- Declare the `enumConvert` template in the exporter header so other exporter TUs can call it.
- Keep enum class-name blocks alphabetized within each `<exporter>_enums.cpp`, ignoring namespaces.
- Do not add one-off switch helpers for enum conversion when an `enumConvert` specialization is appropriate.
- Include the target API header in the enum TU when the mapped target enum is not already visible there.
