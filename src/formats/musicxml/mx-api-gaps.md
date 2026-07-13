# MusicXML MX API Gaps

Notes collected while implementing Denigma's MusicXML exporter. These are MusicXML features or Finale/MUSX requirements that are currently difficult or impossible to express through `mx::api`. Larger user-facing work is tracked in the [MusicXML feature roadmap](roadmap.md).

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

### Curve orientation on a regular (paired) tie

Finale can freeze a tie's curvature direction on a per-note basis (`details::TieAlterStart`), and its own MusicXML export writes this as `<tied orientation="over|under" type="start"/>` even on an ordinary paired tie (one with a matching `<tie>`/`<tied type="stop">` elsewhere).

`mx::api::NoteData::isTieStart`/`isTieStop` write both `<tie>` and an undecorated `<tied>` together, with no orientation field. The only other channel for `<tied>` orientation is `mx::api::TieLetRing` (which hardcodes `type="let-ring"`, so it cannot represent a paired tie) or a `CurveType::tie` entry in `noteAttachmentData.curveStarts`/`curveStops` (which `NotationsWriter` emits as its own separate `<notations>` block, independent of `isTieStart`/`isTieStop`). Setting both on the same note does not merge the two — it produces two `<notations>` blocks, each with its own `<tied type="start">`, i.e. duplicated/conflicting notation rather than one decorated `<tied>`. Switching to the `CurveType::tie` path instead of `isTieStart` would fix the duplication but silently drop the sound-level `<tie>`, since that element is only ever written from `isTieStart`/`isTieStop`. Denigma therefore currently applies `TieAlterStart` orientation only to let-ring ties (`applyTieAlterStart` in `musicxml_notes.cpp`), and drops it for regular paired ties.

Needed API shape: the same tie-notation data model requested above should also carry the shared curve attributes (orientation, placement, position, color) available on `CurveStart`/`CurveStop`, so a paired tie can be decorated without a second, conflicting `<notations>` block or the loss of `<tie>`.

## Smart Shapes And Spanners

### Spanner numbers are pooled per staff instead of per part

MusicXML's `number-level` type states that spanner `number` attributes distinguish "concurrent objects of the same type when the objects overlap in MusicXML document order" within "a single musical part" — the staff plays no role in that scoping. The spec's own worked example is a piano part in which a cross-staff slur "will need a separate number from the mid-measure slurs because it overlaps those slurs in MusicXML document order," even though they are on different staves. Readers accordingly pair slur starts and stops by number within the part (MuseScore, for example, keeps one open-slur slot per number per part).

`mx::impl::SpannerNumberResolver` (added by the unified spanner numbering change) draws numbers from a pool per *staff* per spanner class (`occupied[staffIndex][number]`). Two identity slurs that overlap in document order on different staves of the same part therefore both receive number 1. Minimal repro: a two-staff part with one whole-note slur per staff spanning measures 1-2 serializes as start@staff1, start@staff2, stop@staff1, stop@staff2 — all four emitted as `number="1"`. Readers mispair or drop the second slur, which presents as slurs attached to the wrong staff on multistaff instruments; cross-staff notes increase document-order overlap between the staves' slurs and make the collisions more frequent. Denigma supplies correct identity-based `SpannerNumber` values; the collision happens at write time inside mx.

Needed API shape: none — this is a writer bug. The resolver's pools should be scoped per part and spanner class only (drop the staff dimension from `occupied`). The special-case logic that reserves a cross-staff spanner's number "in every staff pool it touches" then becomes unnecessary rather than needing repair.

Status: filed as mx issue #350 with a PR; the fix is already applied on the rpatters1/mx fork that Denigma builds against, and `MusicXmlSmartShapes.OverlappingSlursAcrossStavesCarryDistinctNumbers` pins it. Remove this entry when the PR merges upstream.

### Octave shifts beyond two octaves

MusicXML `<octave-shift>` supports any shift via the `size` attribute (8, 15, 22, ...). Finale has no built-in 22ma smart shape, but custom lines classified as three-octave shifts ("22ma"/"22mb") can occur.

`mx::api::OttavaType` enumerates only `o8va`, `o8vb`, `o15ma`, and `o15mb`. Denigma skips three-octave shifts with a warning.

Needed API shape: either additional `OttavaType` values for 22ma/22mb or a numeric octave-shift model (direction plus size).

### Octave-shift start drops position and print data

MusicXML `<octave-shift>` accepts the position and print-object/print-style attributes on the start element.

`mx::api::OttavaStart::spannerStart` carries `positionData` and `printData`, but `DirectionWriter::emitOttavaStart` applies only `LineData` and the spanner number (the number drop was fixed by the unified spanner numbering change), so the start's `positionData` and `printData` are still silently dropped. Denigma does not currently populate these fields on ottava starts, so this is a low-priority gap.

Needed API shape: none — this is a writer bug. `emitOttavaStart` should apply position and print data like the bracket, dashes, and ottava-stop writers do.

### Paired wavy-line start/stop (trill extensions and vibrato lines)

MusicXML represents trill extensions and vibrato lines as `<ornaments><wavy-line type="start|continue|stop">` pairs attached to notes.

`mx::api::MarkData` includes `MarkType::wavyLine`, but `NotationsWriter` never sets the required `type` attribute on the emitted `<wavy-line>`, so only an untyped (default) element can be written and start/stop cannot be paired. Denigma exports the `trill-mark` for trill lines that include the tr symbol and omits the extension; pure trill-extension lines and vibrato lines are omitted entirely.

Needed API shape: wavy-line start/stop data on `MarkData` (or a dedicated paired-spanner model for note-attached wavy lines).

### Keyboard pedal spanner fidelity

Finale pedal custom lines carry text/sign choices, hook and custom cap geometry, dashed and character line bodies, and pump (release-and-reengage) changes. MusicXML `<pedal>` supports `type="start|stop|change|continue|sostenuto"`, `line`, `sign`, and `abbreviated` attributes.

`mx::api`'s pedal writer forces `line="yes" sign="yes"`, ignores `LineData`, and supports only start/stop. There is also no way to request sostenuto or una-corda pedal types, so Denigma refuses to export those rather than misrepresent them as damper pedal. Pump changes cannot be expressed as `type="change"`.

Needed API shape: pedal data with the full MusicXML pedal event types and attribute control (`line`, `sign`, `abbreviated`), honoring `LineData` for dashed pedal lines.

### Pedal spanner numbers

MusicXML 3.1 added a `number` attribute (type `number-level`) to `<pedal>` so simultaneous pedal lines — such as a sostenuto line running under a damper line — can be paired unambiguously. The attribute is present in the 3.1, 4.0, and 4.1 schemas; only 3.0 lacks it.

`mx::core::Pedal` (generated from the MusicXML 4.0 schema) supports the attribute via `setNumber`, but `mx::api` never serializes it: the pedal writer does not call `core::Pedal::setNumber`, and `SpannerNumberResolver` deliberately excludes pedals from writer-side number assignment. The resolver's comment claims the `<pedal>` element has no number attribute, which is true only of MusicXML 3.0. Denigma therefore sets no spanner number on pedal starts/stops (see the `@todo` in `appendKeyboardPedal`).

This is low-priority while `mx::api` can express only damper start/stop spanners, since two overlapping damper lines on one staff are musically rare. It becomes necessary as soon as the other pedal event types land (see "Keyboard pedal events and spanners" under Directions and Expressions): sostenuto-under-damper is exactly the collision the attribute exists to resolve.

Needed API shape: honor `SpannerStart::number`/`SpannerStop::number` in the pedal writer, give pedals a pool in `SpannerNumberResolver`, and correct the resolver's pedal-exclusion rationale.

### Custom line continuation and center text

Finale custom lines can carry left-continuation text (shown after system breaks) and center full/abbreviated text (shown mid-line). MusicXML has no continuation-text concept for brackets or dashes, and center text has no home outside of `<glissando>`/`<slide>` element text.

This is primarily a MusicXML specification limit rather than an `mx::api` gap, but it is recorded here because it silently drops Finale content: Denigma logs and omits continuation and center text when exporting general lines.

## Notes

### Notehead fill and SMuFL glyph refinement

MusicXML `<notehead>` carries a shape value plus two independent refinements: a `filled="yes|no"` attribute (the spec's default is white for half notes and longer, filled otherwise, but Finale can override this per note), and a `smufl` attribute that selects a specific glyph when the base shape value is ambiguous (e.g. distinguishing a small rhythmic slash from a large chord-symbol slash, both of which share the `slash` shape value).

`mx::core::Notehead` (the raw schema layer) supports both `filled` and `smufl` attributes, but `mx::api::NoteData::notehead` is a bare `Notehead` shape enum with no fill or glyph-refinement field, and `NoteWriter::setNotehead()` never calls the underlying `setFilled`/`setSmufl`. Denigma's notehead classifier (`classify::classifyNotehead`) tracks fill (filled/unfilled) as a first-class result, and further distinguishes `notehead::Shape::SmallSlash` from `notehead::Shape::LargeSlash`, but neither can be expressed through `mx::api`: both slash shapes collapse to `mx::api::Notehead::slash`, and fill is silently dropped, which can produce a wrong-looking notehead whenever Finale's explicit fill state disagrees with the MusicXML viewer's duration-based default.

The `smufl` gap is more severe for `notehead::Shape::Other`: this is the classifier's catch-all for a real, recognized SMuFL notehead glyph (shape notes, cluster noteheads, arrow noteheads, etc.) that isn't one of Denigma's specifically-modeled shapes, and the classifier already records the exact glyph name (`NoteheadClassification::glyphName`). Today that glyph name is simply dropped and the note exports as a bare `<notehead>other</notehead>`, which by itself carries no visual information at all -- unlike the slash case, which at least renders as some kind of slash. A `smufl` field would let Denigma pass the real glyph name straight through for every `Shape::Other` note.

Needed API shape: optional `filled` (yes/no) and `smufl` (glyph name) fields on `mx::api::NoteData` (or a richer notehead data struct), written through to `core::Notehead::setFilled`/`setSmufl`.

### Artificial-harmonic technical detail

MusicXML's `<technical><harmonic>` element can specify `natural` or `artificial`, plus which pitch is displayed (`base-pitch`, `touching-pitch`, or `sounding-pitch`), in addition to the plain notehead shape used to notate the stopped and touched notes.

`mx::api::MarkData` supports `MarkType::harmonic`, but `NotationsWriter` only ever constructs a bare `core::Harmonic` with position data; it never calls `setChoice`/`setChoice2`, even though `core::Harmonic` (the raw schema layer) fully supports both. Denigma's entry-level classifier (`classify::classifyEntryNoteheads`) identifies artificial-harmonic note pairs, including the touch interval (fourth, major third, or fifth) and an optional third note at the theoretical sounding pitch when the source explicitly includes one, but none of that detail can be attached to the `<harmonic>` mark through the public API -- only an empty, undecorated `<harmonic/>` can be written.

Needed API shape: natural/artificial and base-pitch/touching-pitch/sounding-pitch fields on `MarkData`, meaningful only for `MarkType::harmonic`, written through to `core::Harmonic::setChoice`/`setChoice2`.

## Measures

### Multimeasure rests

MusicXML represents a multimeasure rest by placing `<measure-style><multiple-rest>N</multiple-rest></measure-style>` in the starting measure's `<attributes>`, where `N` is the span in measures. MUSX exposes the corresponding part-scoped `others::MultimeasureRest` records, including the start measure and span, so Denigma has the information needed to export the basic semantic form.

`mx::api::MeasureData` already has `multiMeasureRest` for this purpose, and `MeasureReader` populates it. However, `MeasureWriter` and `PropertiesWriter` never consume it, so `mx::api` cannot currently write a `<multiple-rest>` element. Consequently, Denigma does not yet export any multimeasure-rest spans to MusicXML.

Needed MX work: this is a writer implementation gap rather than a new basic API. Add a `PropertiesWriter` operation that writes a `MeasureStyle` containing `MultipleRest`, and call it when `MeasureData::multiMeasureRest` is positive. Add an API writer/round-trip test.

The existing integer field is not sufficient for full fidelity. MusicXML also permits the `multiple-rest` `use-symbols` attribute and the enclosing `measure-style` `number` (staff) attribute, neither of which `mx::api` exposes. Finale/MUSX additionally records whether to use symbols, the symbol threshold and spacing, custom H-bar shape and dimensions, and number visibility and placement. Some of those layout details have no direct MusicXML equivalent; the interoperable subset would need at least optional `useSymbols` and staff fields, ideally in a dedicated multimeasure-rest data object rather than extending the scalar span field.

### Alternate notation: measure repeats and slash notation

Finale's effective staff setting `others::Staff::AlternateNotation` can request one-bar repeats, two-bar repeats, slash notation on beats, or rhythmic notation. It can be changed by a staff style over a measure range and can apply to one Finale layer. MUSX supplies the effective notation, target layer, compound-meter slash-dot setting, rhythmic stem direction, and several options for hiding the affected or other layers' content.

MusicXML has direct measure-style vocabulary for the principal display modes: `<measure-repeat type="start">1</measure-repeat>` and `2` for one- and two-bar repeats, and `<slash type="start" use-stems="no|yes">` for slash-on-beats and rhythmic notation. These styles require a corresponding `type="stop"` at the first measure after the effective range. The actual music must remain in every MusicXML measure; measure-style controls its display rather than replacing the musical content.

MX's generated core layer models all three elements, but `mx::api` has no public data model for `measure-repeat`, `beat-repeat`, or `slash`, and its reader and writer do not handle them. Denigma consequently exports the underlying entries as ordinary notation and loses the alternate display mode.

Needed API shape: a staff-scoped, positionable measure-style collection on `MeasureData`. It should model start/stop state; measure-repeat pattern length and optional slash count; slash or beat-repeat `useStems`, `useDots`, display beat, and excluded voices; and the optional MusicXML staff number. This should supersede the scalar multimeasure-rest field with a common measure-style data object, or coexist with it while sharing the same writer path.

Full Finale fidelity is not possible through measure style alone. `altLayer` has no general equivalent for measure repeats, while slash/beat-repeat can only exclude other MusicXML voices. `Blank` and `BlankWithRests`, and Finale's independent hide-articulation, lyrics, expressions, and smart-shape settings, require selective `print-object="no"` handling in addition to any measure style. The alternate-notation slash and number fonts, glyph-position options, and two-bar-repeat number offset are likewise Finale-specific layout data with no direct standard MusicXML mapping.

### Measure-numbering text and staff attribute

MusicXML `<measure-numbering>` can carry display text as element content and a `staff` attribute. The element text can differ from the measure's `number` attribute, and `staff` selects the reference staff for vertical positioning.

`mx::api::MeasureData` now exposes the basic measure/system style, `multiple-rest-always`, `multiple-rest-range`, and the `system` relation, which Denigma exports. It does not expose the element text or `staff` attribute.

Needed API shape: fields on `MeasureData` for measure-numbering element text and its optional `staff` attribute.

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

## Page Text and Credits

### Mixed formatting within a credit

Finale page-attached text can change font, size, and style within one text block. MusicXML can preserve this with multiple ordered `<credit-words>` elements in a single `<credit>`, each carrying its own formatting.

`mx::api::PageTextData` contains one string and one `FontData`, and its writer emits exactly one `<credit-words>`. Denigma therefore concatenates all visible Enigma text chunks and applies the first visible chunk's font to the complete credit. Hidden-font chunks are omitted.

Needed API shape: an ordered collection of formatted credit words/symbol chunks within one `PageTextData`, with independent font and position data for each chunk.

### Page text frames and block layout

Finale page text blocks may have standard or custom frames, fixed dimensions, insets, rounded corners, line spacing, and word wrapping. `mx::api::PageTextData` exposes neither MusicXML's formatted-text enclosure attribute nor a richer credit layout model. MusicXML enclosure values would cover only a subset of Finale's standard frames, and arbitrary Shape Designer frames have no direct MusicXML representation.

Denigma currently exports the resolved text and its anchor but drops the frame and text-block layout properties.

Needed API shape: expose the MusicXML formatted-text enclosure attributes on page text. Custom frame geometry and fixed text layout would still require an extension or an intentional downgrade policy.

### Full justification

Finale supports full and forced-full text justification. MusicXML's `justify` attribute only supports left, center, and right, so these values cannot be represented even though `mx::api::PageTextData::justify` exposes the complete MusicXML vocabulary. Denigma omits `justify` for full and forced-full page text.

### Page-specific layout coordinates

Denigma currently computes page-text anchors from the score's default odd/even page size and margins. Finale can override page layout on individual pages. `mx::api::PageData` can carry per-page layout changes, but the MusicXML exporter does not yet populate them, so credits on overridden pages may have inaccurate absolute coordinates.

## Barlines and Endings

### Repeat-ending display text and multiple numbers

MusicXML `<ending>` has both a semantic `number` attribute and element text for the displayed ending label. These can differ, such as `number="1, 2, 3"` with displayed text `1-3`. Finale also allows custom ending text through `RepeatEndingText`, and musxdom exposes this via `RepeatEndingStart::createEndingText()`.

`mx::api::BarlineData` currently exposes `endingType` and one integer `endingNumber`, but it does not expose the ending text body or multiple ending numbers. Denigma can export structural volta starts and stops, but currently emits only the first pass number and cannot preserve custom or condensed display text.

Needed API shape: ending data with a string/list representation for the MusicXML `number` attribute and a separate display text value, plus reader/writer support for `core::Ending::setValue()`.

## Directions and Expressions

### Keyboard pedal events and spanners

Finale custom-line smart shapes can represent damper, sostenuto, and una-corda pedals using independent start,
continuation, and end text; visible or blank lines; ordinary hooks; and four custom pedal-cap shapes. The two
compound custom caps represent a pedal pump: release followed immediately by re-engagement. MusicXML can model
damper and sostenuto events, including `start`, `stop`, `sostenuto`, `change`, `continue`, `discontinue`, and
`resume`, together with independent `line`, `sign`, and `abbreviated` attributes.

`mx::api::DirectionData` currently exposes sign-only damper start/stop marks and line-based damper start/stop
spanners. The writer forces spanners to `line="yes" sign="yes"`, ignores their `LineData`, and cannot emit the
other MusicXML pedal event types or sign controls. Denigma therefore exports blank-line Ped/* custom lines as
sign-only damper marks and visible damper lines as start/stop spanners. It leaves pedal expressions as formatted
words, omits sostenuto and una-corda spanners rather than misrepresenting them, and cannot preserve pump changes,
half-pedal and special-release glyphs, custom hook geometry, continuation text, or dashed/character pedal lines.

Needed API shape: a general pedal-event object exposing every MusicXML pedal type plus `line`, `sign`,
`abbreviated`, `number`, positioning, and line-style data, with matching reader/writer support. This should replace
the current semantic split between pedal marks and pedal start/stop spanner vectors.

### Harp pedal diagrams

MusicXML `<harp-pedals>` is a direction-type used for harp pedal diagrams, with ordered pedal-tuning children and print / position attributes.

MX's generated core model includes `core::HarpPedals`, but the public `mx::api` layer does not expose a harp-pedals direction model. The current reader stub also drops this content rather than mapping it into `DirectionData`. Denigma therefore has no supported public API path for exporting or round-tripping Finale harp pedal diagrams through `mx::api`.

Needed API shape: a public harp-pedals direction data object on `mx::api::DirectionData` (or equivalent ordered direction-item model), plus reader/writer support that preserves the ordered pedal tunings and print / position attributes.

### Direction words justification

MusicXML `<words>` supports both `halign` and `justify`. Finale text repeats use their justification setting for both horizontal alignment and text justification, and Finale's MusicXML export emits `justify` for right-justified jump text such as segno / D.S. markings.

`mx::api::WordsData` exposes `PositionData::horizontalAlignment`, which MX writes as `halign`, but it does not expose a separate `justify` field for direction words. Denigma can set `halign` from `TextRepeatDef::justification`, but cannot currently emit the parallel `justify` attribute through `mx::api`.

Needed API shape: add a `justify` field to `mx::api::WordsData`, parallel to `PageTextData::justify`, and have `DirectionWriter::emitWords()` set `FormattedTextID::setJustify()`.

### Direction text polygon enclosures beyond square / oval / triangle / diamond

Finale text-expression enclosures can use higher-sided polygons such as pentagon, hexagon, heptagon, and octagon. MusicXML's underlying enclosure vocabulary supports more shapes, but `mx::api::RehearsalEnclosure` currently exposes only `rectangle`, `square`, `oval`, `circle`, `bracket`, `triangle`, `diamond`, and `none`.

Denigma currently degrades MUSX pentagon-through-octagon text enclosures to `square` for both rehearsal marks and generic words directions, since that is the closest public `mx::api` shape. This preserves that the text is enclosed, but not the exact polygon geometry.

Needed API shape: expose the additional MusicXML direction-text enclosure shapes through `mx::api`, so Denigma can round-trip MUSX polygon enclosures without downgrading them to `square`.

### Direction words enclosure serialization

`mx::api::WordsData` exposes an `enclosure` field, which Denigma now uses for generic expression text and standard-framed measure text. However, MX's current `DirectionWriter::emitWords()` implementation does not serialize that field into MusicXML `<words enclosure="...">`.

This means Denigma can build the correct pre-serialization `ScoreData`, but exported MusicXML still drops generic word enclosures unless the downstream MX writer is updated.

Needed API shape: have `DirectionWriter::emitWords()` write `WordsData.enclosure` to the MusicXML `<words>` enclosure attribute, parallel to how rehearsal enclosures are already serialized.

### Interleaved words and symbols

MusicXML direction types can interleave `<words>` and `<symbol>` elements in the same direction-type group. This is the correct representation for Finale expressions that mix normal formatted text with SMuFL music glyphs, such as dynamic expressions with text before or after dynamic glyphs, or arbitrary text expressions containing embedded music symbols.

The generated MX core model represents this as `DirectionTypeChoiceChoice` with `words` and `symbol` alternatives. `mx::api::DirectionData` currently exposes `WordsData`, but does not expose a public formatted-symbol direction item or an ordered mixed run of words/symbol chunks. Denigma can therefore emit plain words and semantic dynamics, but cannot yet preserve mixed formatted text plus glyph expressions through `mx::api`.

Needed API shape: a public direction text model that can represent an ordered run of formatted words and formatted SMuFL symbols, preserving order and per-chunk formatting. This could either extend `DirectionData` with a mixed words/symbol collection or add a higher-level formatted direction text object that writes MusicXML `<words>` / `<symbol>` siblings in order.

### Direction system relation

MusicXML `<direction>` supports a `system` attribute with values such as `only-top` and `also-top`, distinguishing directions that belong to the system's top staff from ordinary staff-local directions. This is the correct semantic for Finale expressions assigned to TOP staff, and for grouped staff-list expressions where a top-staff assignment is later supplemented by a concrete staff assignment.

The generated MX core model exposes this as `core::SystemRelation` on `core::Direction`, but `mx::api::DirectionData` does not expose any public field for the direction `system` attribute. Denigma can currently approximate some TOP-assigned expression behavior structurally, but cannot emit the actual MusicXML `system="only-top"` / `system="also-top"` semantics through `mx::api`.

The missing API affects all expression directions that can originate from Finale TOP assignments. The desired behavior is:

- a standalone TOP assignment should export as `system="only-top"` with no explicit staff value
- if the same grouped expression is emitted both as TOP and as a concrete staff assignment, the TOP-owned direction should export as `system="also-top"` and the concrete assignment should still carry its staff ownership
- if the concrete staff assignment is encountered before the TOP assignment, the existing emitted direction should be upgraded from no `system` attribute to `system="also-top"` once the TOP companion is known

Without API support for `SystemRelation`, Denigma cannot represent those cases exactly in the emitted MusicXML even when the grouping logic can detect them.

Needed API shape: add a public direction system-relation field to `mx::api::DirectionData`, with reader/writer support for MusicXML `system="only-top|also-top|none"`.

### Other dynamics SMuFL glyphs

MusicXML 4.0 defines `other-dynamics` as `other-text`, so it can carry a `smufl` attribute for preserving a specific SMuFL glyph name in addition to optional text content.

`mx::api::MarkData` exposes `name` for the text content of `other-dynamics`, and `mx::impl::DynamicsWriter` writes that value into the element body. It does not expose the `smufl` attribute. Denigma can therefore emit text-valued fallback dynamics such as `<other-dynamics>ffp</other-dynamics>`, but cannot preserve a single source glyph as `<other-dynamics smufl="dynamicNiente"/>` through `mx::api`.

Needed API shape: a SMuFL glyph-name field on dynamic mark data for `MarkType::otherDynamics`, with dynamics reader/writer support for threading it through `core::OtherText::smufl`.

## Tuplets and Tremolos

### Other notation SMuFL glyphs

MusicXML's `other-articulation`, `other-technical`, `other-ornament`, and `other-notation` elements use the `other-placement-text` type, which can carry a `smufl` attribute for preserving a specific SMuFL glyph name.

`mx::api::MarkData` exposes `name` for the text content of `other-articulation`, `other-technical`, and `other-ornament`, but does not expose the `smufl` attribute. Denigma can therefore emit semantic marks and text-valued `other-*` fallbacks, but cannot preserve the source glyph name through `mx::api` when a Finale articulation is only representable as an `other-*` MusicXML notation.

Needed API shape: a SMuFL glyph-name field on mark data for `other-articulation`, `other-technical`, and `other-ornament`, and a corresponding public model for `other-notation` if MX intends to expose that notation category through `mx::api`.

Denigma keeps technique text as a words direction. Only the playback-style `arco`/`pizzicato` values are copied into `DirectionData::soundData.pizzicato`; the rest remain textual until `mx::api` grows richer playback or direction-technical modeling.

### Nested tuplet time-modification

MusicXML uses `<time-modification>` on notes for the cumulative timing effect of tuplets, with `<tuplet>` notations identifying the visual start and stop points.

`mx::api::NoteData` can store multiple `TupletStart` and `TupletStop` objects, and `mx::api::DurationData` has the single cumulative time-modification slot that MusicXML requires. However, `mx::impl::NoteWriter` currently searches sibling notes for exactly one tuplet start and exactly one tuplet stop while writing a note's `<time-modification>` normal-type data. Denigma can compute the cumulative ratio, but nested tuplets may still be unreliable through the current writer path.

Needed API shape: writer support for nested tuplets, probably by matching `TupletStart` / `TupletStop` by `numberLevel` and allowing `DurationData` to express cumulative time modification independently of the visual tuplet-start search.

### Unmeasured tremolos

MusicXML represents unmeasured tremolos with `<tremolo type="unmeasured">0</tremolo>`, optionally using the `smufl` attribute to name a specific tremolo glyph.

`mx::api` supports measured single- and multi-note tremolos, but does not expose the MusicXML `unmeasured` type or its optional SMuFL glyph. Denigma therefore cannot express Finale unmeasured tremolo glyphs through the public API. For now, Denigma emits a visible 3-slash single-note tremolo and logs the downgrade.

Needed API shape: tremolo attachment data that exposes MusicXML tremolo type (`single`, `start`, `stop`, `unmeasured`), mark count, and optional SMuFL glyph for unmeasured tremolos.
