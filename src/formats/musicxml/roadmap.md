# MusicXML Feature Backlog

This is the implementation backlog for larger user-facing MusicXML export features. It is not a version plan or a dependency order. Keep concrete `mx::api` limitations in [mx-api-gaps.md](mx-api-gaps.md).

## Compressed MusicXML (`.mxl`) output

Export standards-compliant compressed MusicXML in addition to uncompressed `.musicxml` files. Each archive should contain an uncompressed `mimetype` entry first, a `META-INF/container.xml` file that identifies the root MusicXML document, and the compressed score document. The existing ZIP utilities and MXL massage support should supply most of the required archive infrastructure.

Initially, write one `.mxl` archive for each score or part document that Denigma currently emits separately. Packaging the score and all linked parts into one MusicXML 4.0 archive through `<part-link>` can be considered as a later extension rather than a prerequisite for basic compressed output.

## Whole-rest position convention

Add a MusicXML export option that selects whether an explicitly positioned whole rest preserves Finale's nominal rest position or shifts upward one staff space to the SMuFL glyph origin. MusicXML does not define which interpretation `<display-step>` and `<display-octave>` require; see [MusicXML issue #681](https://github.com/w3c-cg/musicxml/issues/681) and its predecessor, [issue #5](https://github.com/w3c-cg/musicxml/issues/5).

The default should remain Finale-compatible and apply no adjustment, matching round trips through Finale and MuseScore. The optional SMuFL-origin behavior should use `calcFinaleToSmuflRestPositionOffset`, matching Dorico's current interpretation. Apply the selected convention consistently to ordinary and complete-measure whole rests; other rest durations are unaffected.

## Nontraditional and microtonal key signatures

Export Finale nontraditional key signatures through MusicXML's ordered `<key-step>`, `<key-alter>`, and optional `<key-accidental>` values instead of degrading them to zero fifths. Begin with custom 12-EDO signatures, for which MUSX DOM can provide the key map and `mx::api::KeyData::nonTraditional` already provides a writer path.

Extend the same mapping to microtonal key signatures by converting Finale's EDO divisions into MusicXML semitone alterations and suitable accidental values. Preserve the effective written or concert-pitch signature independently for each staff, as the exporter already does for traditional keys.

## Visible cue notes and rests

Export cue material that is visible in the target score or part as MusicXML `<cue/>` notes and rests. Skip cue material hidden in that target context. `mx::api::NoteData::isCue` already writes and reads cue, grace-cue, and cue-rest forms, so this is a Denigma policy and mapping task rather than an MX API feature.

MusicXML export identifies cue layers, suppresses cue entries and their associated expressions when hidden in the requested context, and sends visible cue entries and expressions through their normal mapping paths. Notes and rests set `isCue`. Cue ties remain blocked on the visual-tie API limitation documented in [mx-api-gaps.md](mx-api-gaps.md); Denigma does not maintain a separate workaround path. MNX keeps its current cue-discard behavior because it does not yet support cues.

## MIDI channels

Export each part's MIDI playback assignment — channel, and with it program and bank where available — as MusicXML `<midi-instrument>` data. `mx::api::PartData::midiData` already models channel, program, bank, volume, and pan, so no MX API work is needed.

The source data requires musxdom effort first: Finale's channel assignments live in its playback system (instrument definitions and their staff/layer routing), which musxdom does not yet model. Once musxdom exposes that to some degree, this becomes a Denigma mapping task.

## Percussion

Export Finale percussion staves using their effective percussion maps rather than treating every staff as one pitched instrument. MUSX DOM exposes the staff or staff-style percussion map, each `PercussionNoteInfo` assignment, per-note `PercussionNoteCode` overrides, and the underlying percussion note-type metadata. Use these together to determine each note's displayed staff position and notehead, semantic instrument identity, and playback mapping in both the score and linked parts.

Represent drum kits and other multi-instrument staves with separate MusicXML `<score-instrument>` / `<midi-instrument>` definitions and a matching `<instrument id="…">` on each unpitched note. Preserve `midi-unpitched`, effective staff-style map changes, duplicated note types distinguished by their order IDs, and custom notehead glyphs. The current `mx::api` model exposes only one `InstrumentData` per part and no per-note instrument ID, so full per-position instrument assignment requires additional MX API support.

Also export Finale percussion pictogram expressions as semantic MusicXML `<percussion>` directions. Add an exporter-neutral classifier for exact `pict*` SMuFL glyphs, including valid beater and stick combinations with tip direction, material, parentheses, dashed circles, and strike location. Map Finale expression enclosures where supported, retain canonical SMuFL overrides, and leave mixed text or unrecognized glyph sequences as general text. Do not infer direction pictograms from percussion-note assignments; note identity and performance-direction symbols are separate concerns.

## Alternate notation: measure repeats and slash notation

Export effective staff alternate notation as MusicXML measure styles: one- and two-bar repeats, slash notation on beats, and rhythmic notation. This requires start/stop ranges per staff and, for slash notation, dots, stems, and where possible voice exclusions. It also needs a new public MX measure-style API and writer path.

Layer-only behavior, blank notation, and Finale's independent hiding of articulations, lyrics, expressions, and smart shapes are separate fidelity work. See [mx-api-gaps.md](mx-api-gaps.md) for the detailed limitation analysis.

## Fretboard diagrams

Export Finale fretboard diagrams through MusicXML `<frame>` data on their associated `<harmony>` elements. This does not refer to simultaneous note chords: those are already represented by MusicXML `<chord/>` note groups.

MUSX DOM exposes the fretboard groups, styles, and diagrams referenced by `details::ChordAssign`. `mx::api::ChordData::frameData` can represent the basic string/fret grid, first fret, barre, and fingering data.

First export the basic diagram and its note/fingering/barre details, then assess specialized frame appearance, diagram placement, and capo behavior.

## Additional direction types

Use the principal-voice, other-direction, image, and accordion-registration models now exposed by `mx::api`.

Begin with accordion registrations. Classify single-glyph Finale expressions using the `accdnRH3Ranks*` SMuFL names and map their 4′, 8′, and 16′ stops to `AccordionRegistrationData::high`, `middle`, and `low`. Assess the right-hand four-rank and left-hand glyph families separately where MusicXML cannot preserve their exact rank layout.

For principal voice, obtain representative Finale Hauptstimme and Nebenstimme smart-line fixtures before implementing the mapping. The exporter must identify both ends of the span and choose the appropriate principal-voice symbol; an isolated `analyticsHauptstimme` or `analyticsNebenstimme` expression glyph is not enough to infer a matching stop.

Use `OtherDirectionData` only for recognized direction semantics that lack a dedicated MusicXML direction type. Preserve a canonical SMuFL name and useful fallback text where available, but do not convert every unrecognized expression glyph into `<other-direction>`.

Export measure-attached Finale graphics from `details::MeasureGraphicAssign` as MusicXML `<image>` directions. Resolve embedded and external graphic sources, emit required image files through the multi-output callback, determine MIME types, and convert Finale position and size values to MusicXML tenths. Page graphics and graphics embedded in Shape Designer objects remain separate mapping tasks.

## Compound dynamics

Export dynamics outside MusicXML's dedicated element vocabulary as `mx::api::MarkType::compoundDynamics` with an ordered `CompoundDynamicsData`, so that a marking such as `ffz` or `sffffz` writes one `<dynamics>` element whose children spell it out and whose non-standard components keep their own SMuFL glyph names. Denigma currently flattens these to a single text-valued `<other-dynamics>` child, and only carries the `smufl` attribute when the marking resolves to exactly one glyph.

`classify::dynamics::Mark::composition` already reports the reinforcement syllable, level, forzato, and residual level of a marking, and `Mark::glyphs` gives the source glyph for each letter, so this is a Denigma mapping task rather than an MX API feature.

## Text and custom-line fidelity

Convert eligible music-font characters in expression text to `SymbolData` within the ordered `DirectionChoice::wordsRun` model, particularly for legacy symbol fonts that may not be installed on the receiving system. Preserve unknown or intentionally font-specific characters as `WordsData`.

Use page-specific `PageData` layout overrides when computing absolute credit anchors. The exporter currently uses the score's default odd/even page size and margins, so credits on pages with Finale layout overrides may be misplaced.

Define intentional downgrade policies for Finale text and line features that MusicXML cannot represent. These include full and forced-full text justification; arbitrary Shape Designer text frames; page and measure text-block geometry such as fixed dimensions, insets, corner radius, line spacing, and word wrapping; custom-line continuation text shown after system breaks; and center full/abbreviated text on general bracket or dashes lines. Preserve the closest standard appearance where possible and log material omissions.
