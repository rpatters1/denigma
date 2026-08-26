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
- Do not link classifiers to `denigma_smufl_support`. It parses SMuFL metadata with JSON and would make the classifier library transitively depend on `nlohmann_json`. For glyph-name lookup, prefer the common classifier helper; extend or add a common helper when needed instead of scattering direct `smufl_mapping` calls.
- When adding domain namespaces, qualify the domain-specific payload classes and enums inside the namespace, but keep the top-level classifier return types and classifier functions directly under `denigma::classify`.
- Classify what Finale's own editing model can produce. Where a shape's data contradicts a constraint the Finale UI enforces, treat it as unmodeled rather than inferring a meaning for it: return no classification and let the exporter report it.

## Scope: data Finale itself can author

A classifier's job is to recognize what a Finale user can create. Combinations the UI does not permit are outside that job, even when the file format could in principle hold them.

A plugin can write data the UI would not, so such combinations are not strictly impossible. In practice, though, plugin authors work within the constraints the UI expresses, and the shapes a hypothetical one might construct are not a design target. Inferring intent from data no user action could have produced means guessing at an author who does not exist.

Check reachability before writing a classification path, and prefer a source that settles it: what the UI offers, what `musxdom` documents, and what real fixtures contain. A path no fixture can exercise cannot be validated, and one that can never run is worse than absent, because it reads as though some file somewhere depends on it.

The cost of declining is low. An unmodeled shape reaches the exporter as `std::monostate` and is logged there with its type and cmper, so a genuine counterexample arrives as a diagnostic naming the file rather than as silently invented output. That is the outcome to aim for: leave the shape unclassified, keep the log, and revisit if a real document ever produces one.

This rule is why the glissando classifier recognizes only the glissando and tab slide tools. Finale attaches a line to noteheads for those and for bends alone; a plain custom line is always beat-attached, so a heuristic for "a note-attached line drawn with the plain line tool" had no reachable input and was removed.
