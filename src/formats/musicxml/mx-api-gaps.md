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

### Transposition changes after the first measure

MusicXML encodes `<transpose>` inside measure `<attributes>`, so transposition can change during the score.

`mx::api::PartData::transposition` is a convenience field that writes the initial transposition in the first measure only. The API comment explicitly says subsequent transposition changes are not currently supported.

Needed API shape: positionable transposition data on measures, with enough information to write `<transpose>` in any measure where it changes.

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
