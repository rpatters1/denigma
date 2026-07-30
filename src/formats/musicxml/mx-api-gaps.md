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

Needed API shape: clef size/font controls on `mx::api::ClefData`, or another supported mapping for MusicXML clef sizing that can represent Finale's percent value.

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

## Notes

### Artificial-harmonic technical detail

MusicXML's `<technical><harmonic>` element can specify `natural` or `artificial`, plus which pitch is displayed (`base-pitch`, `touching-pitch`, or `sounding-pitch`), in addition to the plain notehead shape used to notate the stopped and touched notes.

`mx::api::MarkData` supports `MarkType::harmonic`, but `NotationsWriter` only ever constructs a bare `core::Harmonic` with position data; it never calls `setChoice`/`setChoice2`, even though `core::Harmonic` (the raw schema layer) fully supports both. Denigma's entry-level classifier (`classify::classifyEntryNoteheads`) identifies artificial-harmonic note pairs, including the touch interval (fourth, major third, or fifth) and an optional third note at the theoretical sounding pitch when the source explicitly includes one, but none of that detail can be attached to the `<harmonic>` mark through the public API -- only an empty, undecorated `<harmonic/>` can be written.

Needed API shape: a harmonic payload in `MarkDataChoice` with natural/artificial and base-pitch/touching-pitch/sounding-pitch fields, written through to `core::Harmonic::setChoice`/`setChoice2`.

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

### Page text enclosure

MusicXML `<credit-words>` is type `formatted-text-id`, the same type as `<words>` and `<rehearsal>`, so it carries the whole `text-formatting` attribute group, `enclosure` included. Finale page text blocks can use a standard frame, which is exactly the case Denigma already resolves for measure text: `shapeId == 0 && stdLineThickness > 0` means a plain rectangle.

`mx::core::FormattedTextID` already exposes `enclosure()` and `setEnclosure()`, and `DirectionWriter::emitRehearsal()` calls that setter on this very class. The omission is confined to the API model: `mx::api::PageTextData` has no enclosure field, and `PageTextFunctions.cpp` reads and writes only `justify`. Denigma therefore drops the frame even when the source carries one MusicXML could represent exactly.

Needed API shape: an `Enclosure enclosure` field on `mx::api::PageTextData`, converted in both directions in `PageTextFunctions.cpp` beside the existing `justify` handling. Finale's custom frame geometry and text-block layout are a separate, non-mappable concern; see the downgrade-policy item in the [MusicXML feature roadmap](roadmap.md).

## Barlines and Endings

### Repeat-ending display text and multiple numbers

MusicXML `<ending>` has both a semantic `number` attribute and element text for the displayed ending label. These can differ, such as `number="1, 2, 3"` with displayed text `1-3`. Finale also allows custom ending text through `RepeatEndingText`, and musxdom exposes this via `RepeatEndingStart::createEndingText()`.

`mx::api::BarlineData` currently exposes `endingType` and one integer `endingNumber`, but it does not expose the ending text body or multiple ending numbers. Denigma can export structural volta starts and stops, but currently emits only the first pass number and cannot preserve custom or condensed display text.

Needed API shape: ending data with a string/list representation for the MusicXML `number` attribute and a separate display text value, plus reader/writer support for `core::Ending::setValue()`.

## Directions and Expressions

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

### Other dynamics SMuFL glyphs

MusicXML 4.0 defines `other-dynamics` as `other-text`, so it can carry a `smufl` attribute for preserving a specific SMuFL glyph name in addition to optional text content.

`mx::api::MarkData` exposes `name` for the text content of `other-dynamics`, and `mx::impl::DynamicsWriter` writes that value into the element body. It does not expose the `smufl` attribute. Denigma can therefore emit text-valued fallback dynamics such as `<other-dynamics>ffp</other-dynamics>`, but cannot preserve a single source glyph as `<other-dynamics smufl="dynamicNiente"/>` through `mx::api`.

Needed API shape: an other-dynamics payload in `MarkDataChoice` with a SMuFL glyph-name field, with dynamics reader/writer support for threading it through `core::OtherText::smufl`.

## Tuplets and Tremolos

### Other notation SMuFL glyphs

MusicXML's `other-articulation`, `other-technical`, `other-ornament`, and `other-notation` elements use the `other-placement-text` type, which can carry a `smufl` attribute for preserving a specific SMuFL glyph name.

`mx::api::MarkData` exposes `name` for the text content of `other-articulation`, `other-technical`, and `other-ornament`, but does not expose the `smufl` attribute. Denigma can therefore emit semantic marks and text-valued `other-*` fallbacks, but cannot preserve the source glyph name through `mx::api` when a Finale articulation is only representable as an `other-*` MusicXML notation.

Needed API shape: `MarkDataChoice` payloads with a SMuFL glyph-name field for `other-articulation`, `other-technical`, and `other-ornament`, and a corresponding public model for `other-notation` if MX intends to expose that notation category through `mx::api`.

Denigma keeps technique text as a words direction. Only the playback-style `arco`/`pizzicato` values are copied into `DirectionData::soundData.pizzicato`; the rest remain textual until `mx::api` grows richer playback or direction-technical modeling.

### Nested tuplet time-modification

MusicXML uses `<time-modification>` on notes for the cumulative timing effect of tuplets, with `<tuplet>` notations identifying the visual start and stop points.

`mx::api::NoteData` can store multiple `TupletStart` and `TupletStop` objects, and `mx::api::DurationData` has the single cumulative time-modification slot that MusicXML requires. However, `mx::impl::NoteWriter` currently searches sibling notes for exactly one tuplet start and exactly one tuplet stop while writing a note's `<time-modification>` normal-type data. Denigma can compute the cumulative ratio, but nested tuplets may still be unreliable through the current writer path.

Needed API shape: writer support for nested tuplets, probably by matching `TupletStart` / `TupletStop` by `numberLevel` and allowing `DurationData` to express cumulative time modification independently of the visual tuplet-start search.

### Unmeasured tremolos

MusicXML represents unmeasured tremolos with `<tremolo type="unmeasured">0</tremolo>`, optionally using the `smufl` attribute to name a specific tremolo glyph.

`mx::api` supports measured single- and multi-note tremolos, but does not expose the MusicXML `unmeasured` type or its optional SMuFL glyph. Denigma therefore cannot express Finale unmeasured tremolo glyphs through the public API. For now, Denigma emits a visible 3-slash single-note tremolo and logs the downgrade.

Needed API shape: a tremolo payload in `MarkDataChoice` that exposes MusicXML tremolo type (`single`, `start`, `stop`, `unmeasured`), mark count, and optional SMuFL glyph for unmeasured tremolos.
