---
name: classifier-design
description: Denigma classifier design conventions. Use when creating, editing, reviewing, or refactoring classifiers in src/classify or shared classification helpers used by exporters.
---

# Classifier Design

Classifiers must remain `musxdom`-facing and neutral about the output format that consumes them.

Rules:

- Classifier names, enum values, structs, and comments should describe Finale/MUSX or general notation concepts, not MNX, MusicXML, MSS, SVG, or another export format.
- Keep exporter-specific interpretation in the exporter. A classifier may identify a marking, semantic category, placement, or source metadata; it should not decide which target element, property, or vocabulary an exporter uses.
- If an existing classifier exposes target-format concepts, first factor those concepts into neutral source-domain terms, then adapt each exporter at the call site.
- Prefer adding source-domain detail to the classifier over adding parallel exporter-specific classifiers when the underlying MUSX detection logic is shared.
- Do not add one-off target-format switches inside `src/classify`; put those mappings in the relevant exporter code.
