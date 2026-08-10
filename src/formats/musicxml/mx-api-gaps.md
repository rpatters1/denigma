# MusicXML MX API Gaps

Notes collected while implementing Denigma's MusicXML exporter. These are MusicXML features or Finale/MUSX requirements that are currently difficult or impossible to express through `mx::api`. Denigma implementation work and MusicXML specification limitations belong in the [MusicXML feature roadmap](roadmap.md), not here.

## Staff Details

### Staff-details fields beyond staff-lines and staff-size

MusicXML `<staff-details>` can represent more than line count and staff size, including staff type, staff tuning, capo, and print controls.

`mx::api::StaffData` currently exposes `staffLines`, `staffSize`, and `staffScaling`. Denigma can export those simple staff-detail values, but cannot express the rest of Finale's staff details through `mx::api`.

Needed API shape: a staff details data object with optional fields for the MusicXML `<staff-details>` children and attributes that MX intends to support.

### Mid-measure staff details

MusicXML allows `<attributes>` elements mid-measure, and `<attributes>` may include `<staff-details>`. The spec says mid-measure attributes affect the music in score order.

`mx::api::StaffData::staffLines`, `staffSize`, and `staffScaling` are currently single scalars with no tick position. MX writes them during the measure-start attributes phase, so Denigma can only express measure-start staff detail changes through the current API.

Needed API shape: positionable staff details data, likely a vector on `StaffData`, with `tickTimePosition` and fields such as `staffLines`, `staffSize`, and `staffScaling`.

## Clefs

### Clef size percent

MusicXML `<clef>` supports size-related attributes such as symbolic `size` and font-size/style attributes. Finale mid-measure clef entries include a percent value.

`mx::api::ClefData` does not expose clef size or font-size controls. Denigma cannot currently export Finale's per-clef percentage setting through `mx::api`.

The shared symbolic `size` attribute belongs to the cross-element gap below. The remaining clef-specific need is a font-size control, or another supported mapping that can represent Finale's arbitrary percentage value.

## Symbol Sizes

### Consistent `symbol-size` support across applicable elements

MusicXML uses the shared `symbol-size` values `full`, `cue`, `grace-cue`, and `large` on several elements, including note `<type>`, `<accidental>`, `<accidental-mark>`, `<clef>`, and `<level>`. This visual sizing is distinct from the semantic meaning of the element.

`mx::api` does not expose this attribute consistently. In particular, `mx::api::DurationData` carries the note type but not its `size`, while `mx::api::NoteData::isCue` only requests the semantic `<cue/>` element. The writer therefore cannot emit an explicit `<type size="cue">` or `<type size="grace-cue">`.

MusicXML defines an implicit cue size for a note containing `<cue/>`, and an implicit grace-cue size when `<grace>` and `<cue/>` are both present. However, importers do not all honor those defaults consistently. Denigma should retain `<cue/>` for cue semantics and also explicitly set `<type size="cue">` on ordinary cue notes and rests and `<type size="grace-cue">` on cue grace notes. The size must remain independently controllable so that the API can also represent visually cue-sized notes that are not semantic cues.

Needed API shape: one public symbol-size enum, exposed by each API data type whose MusicXML element supports the shared `size` attribute, with complete reader, writer, and comparison support. For notes, the field should belong to the data that writes `<type>` and remain independent of `NoteData::isCue` and grace state.

This should be addressed as one cross-element MX API feature after the pending MX API pull requests have merged, rather than as a cue-note-only field. Once available, Denigma should explicitly populate it for both cue notes and cue grace notes.

## Ties

### Notation-only ties and one-ended visual ties

MusicXML separates sounding ties (`<tie>`) from notated ties (`<notations><tied>`). The `<tied>` element supports `start`, `stop`, `continue`, and `let-ring`. For visually one-ended ties that are not let-ring ties, the MusicXML guidance is to write both `<tied type="start"/>` and `<tied type="stop"/>` on the same note, in that order. This is used for ties into or out of repeats, endings, or codas. These visual ties should not necessarily produce a corresponding `<tie>` element, because `<tie>` represents playback semantics.

`mx::api::NoteData` exposes `isTieStart` and `isTieStop`, and `NoteWriter` maps those booleans to both `<tie>` and `<tied>` together, only as normal start/stop ties. The generic `CurveType::tie` attachment path can emit notation-only start, stop, and continue elements, so a simple visual tie can be represented by treating it as a generic curve. However, that path is separate from the semantic tie booleans and does not solve same-note start/stop ordering. Denigma therefore has no unified public API for independently controlling the playback and notation sides of a tie.

Needed API shape: a tie-notation data model separate from playback tie booleans, with support for `start`, `stop`, `continue`, and same-note start/stop ordering. The API should also allow notation-only ties without writing `<tie>`, and should allow playback `<tie>` / `time-only` semantics to be modeled separately when Denigma can infer them.

### Curve orientation on a regular (paired) tie

Finale can freeze a tie's curvature direction on a per-note basis (`details::TieAlterStart`), and its own MusicXML export writes this as `<tied orientation="over|under" type="start"/>` even on an ordinary paired tie (one with a matching `<tie>`/`<tied type="stop">` elsewhere).

`mx::api::NoteData::isTieStart`/`isTieStop` write both `<tie>` and an undecorated `<tied>` together, with no orientation field. The only other channel for `<tied>` orientation is `mx::api::TieLetRing` (which hardcodes `type="let-ring"`, so it cannot represent a paired tie) or a `CurveType::tie` entry in `noteAttachmentData.curveStarts`/`curveStops` (which `NotationsWriter` emits as its own separate `<notations>` block, independent of `isTieStart`/`isTieStop`). Setting both on the same note does not merge the two — it produces two `<notations>` blocks, each with its own `<tied type="start">`, i.e. duplicated/conflicting notation rather than one decorated `<tied>`. Switching to the `CurveType::tie` path instead of `isTieStart` would fix the duplication but silently drop the sound-level `<tie>`, since that element is only ever written from `isTieStart`/`isTieStop`. Denigma therefore currently applies `TieAlterStart` orientation only to let-ring ties (`applyTieAlterStart` in `musicxml_notes.cpp`), and drops it for regular paired ties.

Needed API shape: the same tie-notation data model requested above should also carry the shared curve attributes (orientation, placement, position, color) available on `CurveStart`/`CurveStop`, so a paired tie can be decorated without a second, conflicting `<notations>` block or the loss of `<tie>`.

## Smart Shapes And Spanners

### Paired wavy-line start/stop (trill extensions and vibrato lines)

MusicXML represents trill extensions and vibrato lines as `<ornaments><wavy-line type="start|continue|stop">` pairs attached to notes.

`mx::api::MarkData` includes `MarkType::wavyLine`, but `NotationsWriter` never sets the required `type` attribute on the emitted `<wavy-line>`, so only an untyped (default) element can be written and start/stop cannot be paired. Denigma exports the `trill-mark` for trill lines that include the tr symbol and omits the extension; pure trill-extension lines and vibrato lines are omitted entirely.

Needed API shape: wavy-line start/stop data on `MarkData` (or a dedicated paired-spanner model for note-attached wavy lines).

### Note-attached glissandi and slides

MusicXML represents both markings as note-attached paired spanners inside `<notations>`: `<glissando type="start|stop">` and `<slide type="start|stop">`. The two carry the same information and differ only in what they imply about the pitch motion between the notes. A glissando defaults to a wavy line and covers the discrete case, while a slide is continuous between the two pitches and defaults to a solid line. Both take an optional `number` for overlapping spans, an optional `line-type` with dash and space lengths, the usual print-style attributes, and optional text content that is printed alongside the line, which is where a marking such as "gliss." belongs.

MX's generated core layer models both elements, and `NotationsChoice` already admits them, but `mx::api` exposes neither. `CurveType` offers only `slur` and `tie`, and `NotationsWriter` skips any entry in `curveStarts`, `curveContinuations`, or `curveStops` whose type is not one of those two, so the generic curve path cannot carry a glissando. `MarkData` has no glissando or slide member either. `NoteAttachmentData` reserves `spannerStarts` and `spannerStops` vectors, but they are commented out and unimplemented. Denigma therefore has no way to write either element, and drops all Finale glissandi and tab slides.

Needed API shape: a note-attached paired spanner model on `NoteAttachmentData`, which the reserved spanner vectors anticipate. It should carry the element kind (glissando or slide), start/stop state, spanner number, the optional printed text, `LineData` for line type and dash and space lengths, and the shared placement, position, and color attributes. A model general enough to host the wavy-line pairs described above would serve both gaps.

## Lyrics

### Word-extension endpoints

MusicXML `<extend>` carries a `type` of `start`, `continue`, or `stop`. A word extension spanning several notes is written as `<extend type="start"/>` on the syllable and a text-less `<lyric>` holding `<extend type="stop"/>` on the note where the line ends, which is how Finale's own export encodes it. A bare `<extend/>` is the legacy form and says only that the syllable extends, not how far.

`mx::api::LyricData` exposes `bool hasExtend` and nothing else, and its writer emits the bare form. There is also no way to express the terminating lyric, since a `LyricData` with empty `text` and no syllabic would have to write an `<extend>`-only element, which the current model cannot request. Denigma therefore emits a bare `<extend/>` on the syllable and the extension's length is unrecoverable by the importer.

The source data is available. `details::LyricAssign::calcWordExtensionEndpoint` returns the terminating entry directly, so mapping start and stop would be straightforward once the API can express them. One case would remain unmappable regardless: the function returns null for documents using legacy rather than smart word extensions, where `LyricAssign::wext` holds a raw Evpu length instead of an endpoint. Resolving a length into a terminating note requires knowing note spacing, which is engraved layout, so legacy extensions keep the bare `<extend/>` in any event.

`LyricData` is thin in other respects that matter less but would likely be addressed by the same work: it models no `<elision>`, none of `<humming>`, `<laughing>`, `<end-line>`, or `<end-paragraph>`, and carries `positionData` and `printData` but no `FontData`.

Needed API shape: an extend model on `LyricData` carrying the MusicXML type alongside the existing boolean, and a way to write a lyric whose only content is an `<extend>`, so a multi-note extension can name both of its ends.

## Measures

### Multimeasure-rest attributes

`mx::api::MeasureData` supports the multimeasure-rest span and the `multiple-rest` `use-symbols` attribute. Denigma exports matching part-scoped `others::MultimeasureRest` records, including Finale's effective choice between rest symbols and a multimeasure-rest shape.

MusicXML also permits the enclosing `measure-style` `number` (staff) attribute, which `mx::api` does not expose. Finale/MUSX additionally records symbol spacing, a custom multimeasure-rest shape and dimensions, and number visibility and placement. Those layout details have no direct MusicXML equivalent.

### Alternate notation: measure repeats and slash notation

Finale's effective staff setting `others::Staff::AlternateNotation` can request one-bar repeats, two-bar repeats, slash notation on beats, or rhythmic notation. It can be changed by a staff style over a measure range and can apply to one Finale layer. MUSX supplies the effective notation, target layer, compound-meter slash-dot setting, rhythmic stem direction, and several options for hiding the affected or other layers' content.

MusicXML has direct measure-style vocabulary for the principal display modes: `<measure-repeat type="start">1</measure-repeat>` and `2` for one- and two-bar repeats, and `<slash type="start" use-stems="no|yes">` for slash-on-beats and rhythmic notation. These styles require a corresponding `type="stop"` at the first measure after the effective range. The actual music must remain in every MusicXML measure; measure-style controls its display rather than replacing the musical content.

MX's generated core layer models all three elements, but `mx::api` has no public data model for `measure-repeat`, `beat-repeat`, or `slash`, and its reader and writer do not handle them. Denigma consequently exports the underlying entries as ordinary notation and loses the alternate display mode.

Needed API shape: a staff-scoped, positionable measure-style collection on `MeasureData`. It should model start/stop state; measure-repeat pattern length and optional slash count; slash or beat-repeat `useStems`, `useDots`, display beat, and excluded voices; and the optional MusicXML staff number. This should supersede the scalar multimeasure-rest field with a common measure-style data object, or coexist with it while sharing the same writer path.

Full Finale fidelity is not possible through measure style alone. `altLayer` has no general equivalent for measure repeats, while slash/beat-repeat can only exclude other MusicXML voices. `Blank` and `BlankWithRests`, and Finale's independent hide-articulation, lyrics, expressions, and smart-shape settings, require selective `print-object="no"` handling in addition to any measure style. The alternate-notation slash and number fonts, glyph-position options, and two-bar-repeat number offset are likewise Finale-specific layout data with no direct standard MusicXML mapping.

## Transposition

### Concert-score `for-part`

MusicXML supports `<for-part>` in `<attributes>` for concert scores with transposed parts. This allows a concert-score file to describe transpositions for extracted or rendered parts.

`mx::api` currently exposes `PartData::transposition`, but not an API for `<for-part>`.

Needed API shape: an API model for `<for-part>` under measure attributes, including the target part identity and associated transposition data.

## Instruments and Sound

### Instrument changes within a part

MusicXML supports multiple instruments per part and can represent changes via score instruments, MIDI instruments, and sound/playback data.

`mx::api::PartData` currently has one `InstrumentData instrumentData` for the part. Denigma can set initial part/instrument metadata, but cannot express Finale staff-style instrument changes or multiple simultaneous instruments within the same part through the current simple field.

Needed API shape: multiple score instruments per part, plus positionable instrument-change or playback-change data that can be emitted where the instrument changes.

### Nested sound children

MusicXML `<sound>` can include nested child elements such as `<midi-instrument>`, `<midi-device>`, `<play>`, `<swing>`, and `<offset>`.

`mx::api::SoundData` models common scalar attributes and `<swing>`, but not the other nested child elements.

Needed API shape: optional support for nested sound children that matter for Denigma's playback export, especially if Finale data requires instrument-specific playback changes.

## Part Groups and Display

### Part-group display positioning

MusicXML name-display elements can carry formatting and position data, including placement controls that matter when multiple brackets or braces overlap.

`mx::api::PartGroupData` currently stores group names, abbreviations, bracket type, group barline, and number, but not display positioning data for group names or symbols.

Denigma will probably not try to export part-name or part-group positioning overrides, or will export very few of them. This is therefore a low-priority gap for Denigma, but it remains a possible API limitation for applications that need exact layout round-tripping.

Needed API shape: position/print data for group-name-display, group-abbreviation-display, and possibly group-symbol placement.

## Page Text and Credits

### Mixed formatting within a credit

Finale page-attached text can change font, size, and style within one text block. MusicXML can preserve this with multiple ordered `<credit-words>` elements in a single `<credit>`, each carrying its own formatting.

`mx::api::PageTextData` contains one string and one `FontData`, and its writer emits exactly one `<credit-words>`. Denigma therefore concatenates all visible Enigma text chunks and applies the first visible chunk's font to the complete credit. Hidden-font chunks are omitted.

Needed API shape: an ordered collection of formatted credit words/symbol chunks within one `PageTextData`, with independent font and position data for each chunk.

## Barlines and Endings

### Repeat-ending appearance and visibility

MusicXML `<ending>` carries, besides `number`, `type`, and its display text, the appearance attributes `print-object`, `end-length`, `text-x`, `text-y`, `system`, and the whole `print-style` group. Finale supplies a value for each of them: `RepeatEndingStart::hidden` for `print-object`, `endLineVPos` for `end-length`, `textHPos` and `textVPos` for `text-x` and `text-y`, `topStaffOnly` for `system`, the bracket corner offsets for `default-x` and `default-y`, and `FontOptions::FontType::Ending` for the font. Finale's own export writes them, as in `<ending default-y="40" end-length="30" font-size="8.5" number="4" print-object="yes" system="only-top" type="start">`.

`mx::api::EndingData` exposes `type`, `numbers`, and `text` only, so Denigma exports the ending's structure and label but none of its placement, size, or visibility. Hidden endings are the notable loss, because they are structural in Finale rather than decorative: the bracket is suppressed while the repeat still governs playback.

Needed API shape: `PositionData` and `FontData` members on `EndingData` in line with other positionable API types, plus a print-object flag, an end-length value, the text offsets, and the `system` placement enum, with reader and writer support for each.

## Directions and Expressions

### Per-minute font in metronome marks

MusicXML lets `<per-minute>` carry its own font attributes, independently of the enclosing
`<metronome>`. Finale uses that distinction when the metronome note and its number come from
different fonts. In `metronome_marks.musx`, Finale writes Finale Maestro Text on `<metronome>` and
Times New Roman Bold on `<per-minute>` for the split-font whole-note mark.

`mx::api::TempoData::fontData` can represent the enclosing metronome font, but
`mx::api::BeatsPerMinute` stores only the per-minute string. `MetronomeReader` consequently
discards the child font attributes, and `DirectionWriter` cannot emit them. Denigma can preserve a
single font on the enclosing metronome, but cannot fully preserve a split-font mark through the
current API.

Needed API shape: add `FontData` for the per-minute value to `BeatsPerMinute`, with reader, writer,
and comparison support. A default-constructed `FontData` should leave the child attributes
unstated so it inherits the enclosing metronome font.

### Keyboard pedal appearance, identity, and playback

Finale custom-line smart shapes can use independent start, continuation, and end text; visible or blank lines;
ordinary hooks; four custom pedal-cap shapes; and solid, dashed, or character-based line bodies. MusicXML pedal
directions also support `line`, `sign`, `abbreviated`, and `number` attributes.

`mx::api::PedalLineData` exposes the complete MusicXML pedal-line event vocabulary, including sostenuto, change,
continue, discontinue, and resume, but carries only the event kind, tick, and position. Its writer always emits
`line="yes"` and does not expose sign selection, abbreviation, or identity numbers. MusicXML 3.1 and later use
the `number` attribute to distinguish simultaneous lines such as damper and sostenuto pedals, and
`mx::core::Pedal` supports it, but `mx::api` does not.

MusicXML does not provide dash or hook geometry on `<pedal>`, nor a visual pedal type for una corda / Pedal III;
visible notation for those cases uses words and bracket/dashes directions. It does represent una-corda playback through
`<sound soft-pedal="...">`, including numeric half-pedal values, but `mx::api::SoundData` does not expose that
attribute. Denigma can preserve visible una-corda text and brackets through general direction words and lines, but
cannot preserve its playback semantics. Finale custom hook geometry, continuation text, and character-based line
bodies have no direct MusicXML pedal equivalent.

Needed API shape: extend `PedalLineData` with `sign`, `abbreviated`, and `number`, with reader/writer support and
pedal-aware number resolution. Correct `SpannerNumberResolver`'s MusicXML-3.0-era claim that `<pedal>` has no
`number` attribute. `SoundData` should also expose MusicXML's `soft-pedal` playback attribute.

### Direction-level technique playback

Finale technique text such as `pizz.`, `arco`, and `mute` carries playback meaning as well as visible text.

Denigma keeps technique text as a words direction. Only the playback-style `arco`/`pizzicato` values are copied into `DirectionData::soundData.pizzicato`; the rest remain textual until `mx::api` grows richer playback or direction-technical modeling.

Needed API shape: direction-level playback or technical modeling for the remaining technique vocabulary, so a recognized technique can carry its playback effect alongside its words.

## Tuplets

### Nested tuplet time-modification

MusicXML uses `<time-modification>` on notes for the cumulative timing effect of tuplets, with `<tuplet>` notations identifying the visual start and stop points.

`mx::api::NoteData` can store multiple `TupletStart` and `TupletStop` objects, and `mx::api::DurationData` has the single cumulative time-modification slot that MusicXML requires. However, `mx::impl::NoteWriter` currently searches sibling notes for exactly one tuplet start and exactly one tuplet stop while writing a note's `<time-modification>` normal-type data. Denigma can compute the cumulative ratio, but nested tuplets may still be unreliable through the current writer path.

Needed API shape: writer support for nested tuplets, probably by matching `TupletStart` / `TupletStop` by `numberLevel` and allowing `DurationData` to express cumulative time modification independently of the visual tuplet-start search.
