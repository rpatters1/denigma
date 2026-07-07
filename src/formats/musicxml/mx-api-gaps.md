# MusicXML MX API Gaps

Notes collected while implementing Denigma's MusicXML exporter. These are MusicXML features or Finale/MUSX requirements that are currently difficult or impossible to express through `mx::api`.

## Staff Details

### Staff-details fields beyond staff-lines and staff-size

MusicXML `<staff-details>` can represent more than line count and staff size, including staff type, staff tuning, capo, and print controls.

`mx::api::StaffData` currently exposes `staffLines` and `staffSize`. Denigma can export simple staff-line and staff-size changes, but cannot express the rest of Finale's staff details through `mx::api`.

Needed API shape: a staff details data object with optional fields for the MusicXML `<staff-details>` children and attributes that MX intends to support.

### Mid-measure staff details

MusicXML allows `<attributes>` elements mid-measure, and `<attributes>` may include `<staff-details>`. The spec says mid-measure attributes affect the music in score order.

`mx::api::StaffData::staffLines` and `staffSize` are currently single scalars with no tick position. MX writes them during the measure-start attributes phase, so Denigma can only express measure-start staff detail changes through the current API.

Needed API shape: positionable staff details data, likely a vector on `StaffData`, with `tickTimePosition` and fields such as `staffLines` and `staffSize`.

## Clefs

### Forced or additional clefs

MusicXML `<clef>` supports an `additional` attribute for clefs that should be displayed even when they are not needed by the normal clef-change sequence. Finale can force a clef display with `ShowClefMode::Always`.

`mx::api::ClefData` can represent the clef identity, position, staff number, and `print-object`, but it does not expose the MusicXML `additional` attribute. Denigma can suppress hidden clefs with `print-object="no"`, but cannot accurately express Finale's forced/additional clef display through the current API.

Needed API shape: an optional `additional` field on `mx::api::ClefData`, written as the MusicXML `additional` attribute.

### Clef size percent

MusicXML `<clef>` supports size-related attributes such as symbolic `size` and font-size/style attributes. Finale mid-measure clef entries include a percent value.

`mx::api::ClefData` does not expose clef size or font-size controls. Denigma cannot currently export Finale's per-clef percentage setting through `mx::api`.

Needed API shape: clef size/font controls on `mx::api::ClefData`, or another supported mapping for MusicXML clef sizing that can represent Finale's percent value.

## Ties

### Notation-only ties and one-ended visual ties

MusicXML separates sounding ties (`<tie>`) from notated ties (`<notations><tied>`). The `<tied>` element supports `start`, `stop`, `continue`, and `let-ring`. For visually one-ended ties that are not let-ring ties, the MusicXML guidance is to write both `<tied type="start"/>` and `<tied type="stop"/>` on the same note, in that order. This is used for ties into or out of repeats, endings, or codas. These visual ties should not necessarily produce a corresponding `<tie>` element, because `<tie>` represents playback semantics.

`mx::api::NoteData` exposes `isTieStart` and `isTieStop`, and `NoteWriter` maps those booleans to both `<tie>` and `<tied>` together, only as normal start/stop ties. Denigma therefore cannot express notation-only ties or same-note start/stop visual ties through the public API.

Needed API shape: a tie-notation data model separate from playback tie booleans, with support for `start`, `stop`, `continue`, and same-note start/stop ordering. The API should also allow notation-only ties without writing `<tie>`, and should allow playback `<tie>` / `time-only` semantics to be modeled separately when Denigma can infer them.

## Smart Shapes And Spanners

### Spanner number assignment in serialized order

MusicXML `number` attributes for slurs, wedges, and similar spanners disambiguate simultaneously open spanners of the same element type. For slurs, "open" is effectively determined by serialized MusicXML note order, not only by musical time. When voices are written with `<backup>` elements, a slur that has musically ended may still appear open to a streaming reader until the later voice containing its stop is serialized.

Denigma can identify matching MUSX smart-shape start and stop endpoints, but `mx::api` controls final note serialization order. Denigma currently uses a part-local musical-range heuristic for smart-shape number assignment, which cannot fully match serialized-order cases such as non-overlapping slurs split across voices.

Needed API shape: `mx::api` should assign or normalize spanner `number` attributes during writing, using the actual serialization order it controls and the paired start/stop data in the API model. If that becomes available, Denigma should remove its local smart-shape number heuristic.

### Ottava number attributes

MusicXML `<octave-shift>` supports the same `number` attribute pattern used by other spanners, which is needed to disambiguate overlapping ottavas.

`mx::api::OttavaStart` stores its number in `spannerStart.numberLevel`, and ottava stops use `mx::api::SpannerStop::numberLevel`. The reader path preserves this via the generic spanner helpers, but `mx::impl::DirectionWriter::emitOttavaStart()` and `emitOttavaStop()` currently do not write the `number` attribute. Denigma can assign ottava number levels, but they are dropped when serialized through `mx::api`.

Needed API shape: write `OttavaStart::spannerStart.numberLevel` and `SpannerStop::numberLevel` as MusicXML `<octave-shift number="...">` when the value is positive, matching the existing slur, wedge, and generic spanner behavior.

### Ottava stop size for importer compatibility

MusicXML allows the `<octave-shift>` `size` attribute to be omitted, including on `type="stop"` elements. However, some importers use the stop `size` to reconstruct the active octave-shift timeline.

`mx::api::OttavaStart` carries an explicit ottava type, so `mx::impl::DirectionWriter::emitOttavaStart()` can write `size="8"` or `size="15"`. Ottava stops are currently represented only as `mx::api::SpannerStop`, and `mx::impl::DirectionWriter::emitOttavaStop()` writes `type="stop"` without a `size` attribute. Denigma therefore cannot emit stop sizes for ottavas through the current API, even when doing so is needed for compatibility with importers such as MuseScore.

Needed API shape: either an ottava-specific stop type that carries the same 8va / 8vb / 15ma / 15mb amount as the start, or writer support that derives and emits the matching stop `size` for `<octave-shift type="stop">`.

### Optional wedge color

MusicXML `<wedge>` supports a `color` attribute, but omitting it is the normal way to request the default score color.

`mx::api::WedgeStart` has a `ColorData colorData` field but no `isColorSpecified` flag, and `mx::impl::DirectionWriter::emitWedgeStart()` always calls `setAttributesFromColorData`. Denigma therefore cannot emit a colorless wedge start through the current API. Leaving `ColorData` default-constructed writes `color="#FFFFFF"`, which can make hairpins invisible in importers such as MuseScore.

Needed API shape: add an optional color flag to `WedgeStart`, matching other API objects that carry `ColorData`, and write the MusicXML `color` attribute only when it is explicitly specified.

## Notes

### Non-arpeggiate endpoints

MusicXML `<non-arpeggiate>` is attached only to the top and bottom notes of the bracket. It requires a `type` attribute (`top` or `bottom`) and may need a shared `number` to distinguish simultaneous brackets.

`mx::api::MarkType::nonArpeggiate` can emit a `<non-arpeggiate>` element, but `mx::api::MarkData` does not expose the required endpoint `type` or `number`. Denigma can resolve Finale non-arpeggio expressions to the correct top note of the top entry and bottom note of the bottom entry, but cannot write the correct paired MusicXML notation through the public API.

Needed API shape: note-attached non-arpeggiate data with endpoint type (`top` / `bottom`) and optional number, or equivalent fields on `MarkData` that are only meaningful for `MarkType::nonArpeggiate`.

### Cross-staff note staff override

MusicXML represents cross-staff notation by keeping the note in its logical part/voice sequence and writing a per-note `<staff>` element that names the visual target staff within the part. This is especially important for cross-staff chords, where moving individual notes into different staff containers would break chord ordering and timing.

`mx::api::NoteData` currently has no per-note staff override. `mx::impl::NoteWriter::setStaffAndVoice()` writes `<staff>` from the current `StaffData` container cursor, so Denigma cannot correctly emit notes crossed to another staff through the public API.

Needed API shape: an optional 0-based `staffIndex` override on `mx::api::NoteData`, defaulting to "use the containing staff cursor." `NoteWriter` should prefer the note override when writing `<staff>`, and the reader should preserve the MusicXML note staff when possible.

## Time Signatures

### Per-staff time signatures

MusicXML allows multiple `<time>` elements in `<attributes>`, and the `number` attribute can identify the staff that a time signature applies to.

`mx::api::MeasureData` currently has a single `TimeSignatureData timeSignature` with no staff index and no vector. Denigma can emit one part-wide time signature per measure, but cannot express Finale files where different staves within a part have independent time signatures.

Needed API shape: either a vector of `TimeSignatureData` values on `MeasureData`, or a per-staff time signature collection, with an optional staff index matching MusicXML's `number` attribute.

### Mid-measure time signatures

MusicXML allows `<attributes>` mid-measure, so time signatures can theoretically change mid-measure in score order.

`mx::api::TimeSignatureData` has no `tickTimePosition`. Denigma can currently only express time signatures at the measure-start attributes position.

Needed API shape: positionable time signature data, likely with `tickTimePosition`.

### Composite and complex time signatures

MusicXML supports richer time signatures than a single `beats` / `beat-type` pair, including multiple beat groups and interchangeable or compound forms.

`mx::api::TimeSignatureData` currently models one `beats` string and one `beatType` string plus a small symbol enum. Denigma can use text such as `3+2` in `beats`, but cannot model the full MusicXML time-signature structure when multiple `<beats>` / `<beat-type>` groups or other complex forms are needed.

Needed API shape: a richer time signature representation that can preserve the MusicXML structure beyond the common single-pair case.

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

`mx::api::SoundData` models common scalar attributes only. The header notes that these nested child elements are intentionally not modeled.

Needed API shape: optional support for nested sound children that matter for Denigma's playback export, especially if Finale data requires instrument-specific playback changes.

## Part Groups and Display

### Part-group display positioning

MusicXML name-display elements can carry formatting and position data, including placement controls that matter when multiple brackets or braces overlap.

`mx::api::PartGroupData` currently stores group names, abbreviations, bracket type, group barline, and number, but not display positioning data for group names or symbols.

Denigma will probably not try to export part-name or part-group positioning overrides, or will export very few of them. This is therefore a low-priority gap for Denigma, but it remains a possible API limitation for applications that need exact layout round-tripping.

Needed API shape: position/print data for group-name-display, group-abbreviation-display, and possibly group-symbol placement.

## Barlines and Endings

### Repeat-ending display text and multiple numbers

MusicXML `<ending>` has both a semantic `number` attribute and element text for the displayed ending label. These can differ, such as `number="1, 2, 3"` with displayed text `1-3`. Finale also allows custom ending text through `RepeatEndingText`, and musxdom exposes this via `RepeatEndingStart::createEndingText()`.

`mx::api::BarlineData` currently exposes `endingType` and one integer `endingNumber`, but it does not expose the ending text body or multiple ending numbers. Denigma can export structural volta starts and stops, but currently emits only the first pass number and cannot preserve custom or condensed display text.

Needed API shape: ending data with a string/list representation for the MusicXML `number` attribute and a separate display text value, plus reader/writer support for `core::Ending::setValue()`.

## Directions and Expressions

### Direction words justification

MusicXML `<words>` supports both `halign` and `justify`. Finale text repeats use their justification setting for both horizontal alignment and text justification, and Finale's MusicXML export emits `justify` for right-justified jump text such as segno / D.S. markings.

`mx::api::WordsData` exposes `PositionData::horizontalAlignmnet`, which MX writes as `halign`, but it does not expose a separate `justify` field for direction words. Denigma can set `halign` from `TextRepeatDef::justification`, but cannot currently emit the parallel `justify` attribute through `mx::api`.

Needed API shape: add a `justify` field to `mx::api::WordsData`, parallel to `PageTextData::justify`, and have `DirectionWriter::emitWords()` set `FormattedTextID::setJustify()`.

### Interleaved words and symbols

MusicXML direction types can interleave `<words>` and `<symbol>` elements in the same direction-type group. This is the correct representation for Finale expressions that mix normal formatted text with SMuFL music glyphs, such as dynamic expressions with text before or after dynamic glyphs, or arbitrary text expressions containing embedded music symbols.

The generated MX core model represents this as `DirectionTypeChoiceChoice` with `words` and `symbol` alternatives. `mx::api::DirectionData` currently exposes `WordsData`, but does not expose a public formatted-symbol direction item or an ordered mixed run of words/symbol chunks. Denigma can therefore emit plain words and semantic dynamics, but cannot yet preserve mixed formatted text plus glyph expressions through `mx::api`.

Needed API shape: a public direction text model that can represent an ordered run of formatted words and formatted SMuFL symbols, preserving order and per-chunk formatting. This could either extend `DirectionData` with a mixed words/symbol collection or add a higher-level formatted direction text object that writes MusicXML `<words>` / `<symbol>` siblings in order.

### Other dynamics SMuFL glyphs

MusicXML 4.0 defines `other-dynamics` as `other-text`, so it can carry a `smufl` attribute for preserving a specific SMuFL glyph name in addition to optional text content.

`mx::api::MarkData` exposes `name` for the text content of `other-dynamics`, and `mx::impl::DynamicsWriter` writes that value into the element body. It does not expose the `smufl` attribute. Denigma can therefore emit text-valued fallback dynamics such as `<other-dynamics>ffp</other-dynamics>`, but cannot preserve a single source glyph as `<other-dynamics smufl="dynamicNiente"/>` through `mx::api`.

Needed API shape: a SMuFL glyph-name field on dynamic mark data for `MarkType::otherDynamics`, with dynamics reader/writer support for threading it through `core::OtherText::smufl`.

## Tuplets and Tremolos

### Other notation SMuFL glyphs

MusicXML's `other-articulation`, `other-technical`, `other-ornament`, and `other-notation` elements use the `other-placement-text` type, which can carry a `smufl` attribute for preserving a specific SMuFL glyph name.

`mx::api::MarkData` exposes `name` for the text content of `other-articulation`, `other-technical`, and `other-ornament`, but does not expose the `smufl` attribute. Denigma can therefore emit semantic marks and text-valued `other-*` fallbacks, but cannot preserve the source glyph name through `mx::api` when a Finale articulation is only representable as an `other-*` MusicXML notation.

Needed API shape: a SMuFL glyph-name field on mark data for `other-articulation`, `other-technical`, and `other-ornament`, and a corresponding public model for `other-notation` if MX intends to expose that notation category through `mx::api`.

### Nested tuplet time-modification

MusicXML uses `<time-modification>` on notes for the cumulative timing effect of tuplets, with `<tuplet>` notations identifying the visual start and stop points.

`mx::api::NoteData` can store multiple `TupletStart` and `TupletStop` objects, and `mx::api::DurationData` has the single cumulative time-modification slot that MusicXML requires. However, `mx::impl::NoteWriter` currently searches sibling notes for exactly one tuplet start and exactly one tuplet stop while writing a note's `<time-modification>` normal-type data. Denigma can compute the cumulative ratio, but nested tuplets may still be unreliable through the current writer path.

Needed API shape: writer support for nested tuplets, probably by matching `TupletStart` / `TupletStop` by `numberLevel` and allowing `DurationData` to express cumulative time modification independently of the visual tuplet-start search.

### Double-note tremolos

Finale represents some double-note tremolos as invisible two-entry tuplets. MusicXML represents these with `<tremolo type="start">` and `<tremolo type="stop">` ornaments, plus the appropriate duration and time-modification semantics.

`mx::api::MarkType` currently exposes only single-note tremolo marks. The generated MX core model supports MusicXML's tremolo type values, but there is no public API representation for double-note tremolo start/stop.

Needed API shape: a note attachment model for double-note tremolos with mark count and type (`start`, `stop`), separate from single-note tremolo glyph marks.

### Unmeasured tremolos

MusicXML represents unmeasured tremolos with `<tremolo type="unmeasured">0</tremolo>`, optionally using the `smufl` attribute to name a specific tremolo glyph.

`mx::api::MarkType` currently exposes only single-note tremolo slash counts and always writes `type="single"`. Denigma therefore cannot express Finale unmeasured tremolo glyphs through the public API. For now, Denigma emits a visible 3-slash single-note tremolo and logs the downgrade.

Needed API shape: tremolo attachment data that exposes MusicXML tremolo type (`single`, `start`, `stop`, `unmeasured`), mark count, and optional SMuFL glyph for unmeasured tremolos.
