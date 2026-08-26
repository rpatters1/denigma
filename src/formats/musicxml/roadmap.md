# MusicXML Feature Backlog

This is the implementation backlog for larger user-facing MusicXML export features. It is not a version plan or a dependency order. Keep concrete `mx::api` limitations in [mx-api-gaps.md](mx-api-gaps.md).

## Compressed MusicXML (`.mxl`) output

Export useful standards-compliant compressed MusicXML in addition to uncompressed `.musicxml` files. A useful Denigma archive must package the score together with all linked part documents that the exporter already knows how to produce, and must preserve the relationships between them. The archive should contain an uncompressed `mimetype` entry first, a `META-INF/container.xml` file that identifies the root MusicXML document, and the linked score and part documents.

The existing ZIP utilities and MXL massage support can supply archive mechanics, but they do not solve the packaging model. A score-only archive is technically possible, but it provides no added user value for Denigma's current multi-output workflow and should not be treated as the feature's initial completion criterion. See [mx-api-gaps.md](mx-api-gaps.md) for the missing score/part relationship support.

## Microtonal key signatures

Extend the nontraditional key-signature mapping to microtonal signatures by converting Finale's EDO divisions into MusicXML semitone alterations and suitable accidental values. Preserve the effective written or concert-pitch signature independently for each staff, as the exporter already does for traditional keys.

## Instruments, transpositions, and instrument changes

Treat a part's instrument as one subject rather than the handful of unrelated fields it is today. `populatePartMetadata` sets `instrumentData.uniqueId`, its required name, and, when the staff's `instUuid` is recognized, `instrumentData.soundID`. It emits `soloOrEnsemble` only when both solo and ensemble UUIDs map to that SoundID and the value is needed to disambiguate them.

The instrument name uses Finale's playback route name when one is available, matching Finale's own MusicXML export. It falls back to the standardized SoundID string and then to the raw instrument UUID. Part and instrument display names are intentionally excluded from this fallback chain.

Instrument sound coverage is partial in a way that hides itself. `musicXmlInstrumentSoundFromUuid` maps a fixed table of Finale instrument uuids, several entries are commented out, and an unrecognized uuid simply yields no `<instrument-sound>` with no diagnostic. Decide whether an unmapped instrument deserves a Verbose log, and extend the table.

Transposition is exported once per part, from the staff that was current when the part was built, and gated on `showTransposed`. Finale can change an instrument's transposition mid-piece through a staff style, and nothing captures that. The related concert-score `<for-part>` support is an MX API gap; see [mx-api-gaps.md](mx-api-gaps.md).

Instrument changes are the largest piece and are blocked upstream: `mx::api::PartData` holds exactly one `InstrumentData` for the whole part, so neither a mid-piece instrument change nor two simultaneous instruments can be expressed. That limitation, and the multiple-`<score-instrument>` model needed to lift it, are described under instrument changes within a part in [mx-api-gaps.md](mx-api-gaps.md). Sound coverage and initial transposition can proceed ahead of it.

Two neighbouring items overlap this one and should stay separate. MIDI channels below covers the playback assignment that hangs off the instrument, and percussion covers the per-note instrument identity that drum kits need.

## MIDI channels

Export each part's MIDI playback assignment — channel, and with it program and bank where available — as MusicXML `<midi-instrument>` data. `mx::api::PartData::midiData` already models channel, program, bank, volume, and pan, so no MX API work is needed.

The source data requires musxdom effort first: Finale's channel assignments live in its playback system (instrument definitions and their staff/layer routing), which musxdom does not yet model. Once musxdom exposes that to some degree, this becomes a Denigma mapping task.

## Percussion

Export Finale percussion staves using their effective percussion maps rather than treating every staff as one pitched instrument. MUSX DOM exposes the staff or staff-style percussion map, each `PercussionNoteInfo` assignment, per-note `PercussionNoteCode` overrides, and the underlying percussion note-type metadata. Use these together to determine each note's displayed staff position and notehead, semantic instrument identity, and playback mapping in both the score and linked parts.

Represent drum kits and other multi-instrument staves with separate MusicXML `<score-instrument>` / `<midi-instrument>` definitions and a matching `<instrument id="…">` on each unpitched note. Preserve `midi-unpitched`, effective staff-style map changes, duplicated note types distinguished by their order IDs, and custom notehead glyphs. This half is blocked upstream: `mx::api` exposes one `InstrumentData` per part and no per-note instrument reference, so no note can be pointed at an instrument. The unpitched note itself, its display position, `<midi-unpitched>`, and the `<percussion>` pictogram are all already expressible; see per-note instrument assignment in [mx-api-gaps.md](mx-api-gaps.md) for what is and is not available.

Also export Finale percussion pictogram expressions as semantic MusicXML `<percussion>` directions. Add an exporter-neutral classifier for exact `pict*` SMuFL glyphs, including valid beater and stick combinations with tip direction, material, parentheses, dashed circles, and strike location. Map Finale expression enclosures where supported, retain canonical SMuFL overrides, and leave mixed text or unrecognized glyph sequences as general text. Do not infer direction pictograms from percussion-note assignments; note identity and performance-direction symbols are separate concerns.

## Alternate notation: measure repeats and slash notation

Export effective staff alternate notation as MusicXML measure styles: one- and two-bar repeats, slash notation on beats, and rhythmic notation. This requires start/stop ranges per staff and, for slash notation, dots, stems, and where possible voice exclusions. It also needs a new public MX measure-style API and writer path.

Layer-only behavior, blank notation, and Finale's independent hiding of articulations, lyrics, expressions, and smart shapes are separate fidelity work. See [mx-api-gaps.md](mx-api-gaps.md) for the detailed limitation analysis.

## Fretboard diagrams

Export Finale fretboard diagrams through MusicXML `<frame>` data on their associated `<harmony>` elements. This does not refer to simultaneous note chords: those are already represented by MusicXML `<chord/>` note groups.

MUSX DOM exposes the fretboard groups, styles, and diagrams referenced by `details::ChordAssign`. `mx::api::ChordData::frameData` can represent the basic string/fret grid, first fret, barre, and fingering data.

First export the basic diagram and its note/fingering/barre details, then assess specialized frame appearance, diagram placement, and capo behavior.

## Accordion registration: combining glyph sequences and legacy fonts

Denigma classifies and exports only a single precomposed accordion-registration glyph today (e.g. `accdnRH3RanksClarinet`), matching the standard right-hand three-/four-rank and left-hand two-/three-rank diagrams. SMuFL also defines combining glyphs (`accdnCombRH3RanksEmpty`, `accdnCombDot`, and their siblings) meant for a custom diagram outside that fixed set, but Denigma does not attempt to reassemble one from source glyphs.

An earlier version of this classifier tried to recognize a combining sequence by reading multiple font-character runs from a Finale Text Expression, using each run's baseline/superscript shift as a dot's position. That was removed: nothing confirmed it matches how Finale users actually build these diagrams, and Shape Designer's `DrawChar` instruction is arguably the more natural tool for placing dots at arbitrary positions, which would make it a Shape Expression instead. `classifyAssignedShapeExpression` has no accordion case, and `musx::dom::KnownShapeDefType` has no accordion entry, so that route is equally unhandled.

Revisit this only when a real Finale file exhibits a custom accordion registration, and let that fixture settle which route (Shape Designer shape vs. baseline-shifted text runs, or both) is worth supporting. The same discipline applies to legacy, pre-SMuFL accordion registration fonts: `smufl_mapping` has no such font mappings today, and none should be added speculatively — add one only against a real fixture that needs it.

## Additional direction types

Use the principal-voice, other-direction, and image models now exposed by `mx::api`.

For principal voice, obtain representative Finale Hauptstimme and Nebenstimme smart-line fixtures before implementing the mapping. The exporter must identify both ends of the span and choose the appropriate principal-voice symbol; an isolated `analyticsHauptstimme` or `analyticsNebenstimme` expression glyph is not enough to infer a matching stop.

Use `OtherDirectionData` only for recognized direction semantics that lack a dedicated MusicXML direction type. Preserve a canonical SMuFL name and useful fallback text where available, but do not convert every unrecognized expression glyph into `<other-direction>`.

Export measure-attached Finale graphics from `details::MeasureGraphicAssign` as MusicXML `<image>` directions. Resolve embedded and external graphic sources, emit required image files through the multi-output callback, determine MIME types, and convert Finale position and size values to MusicXML tenths. Page graphics and graphics embedded in Shape Designer objects remain separate mapping tasks.

## Tuplet numbering scope

Tuplet `numberLevel` comes from the tuplet's index within its entry frame (`applyTupletData` in
`musicxml_notes.cpp`). That is stable for a tuplet's whole extent, so a start always pairs with its
stop, but a frame is one layer of one staff while MusicXML's `number` is scoped to the part. Two
layers each numbering from 1 can therefore hand the same level to two unrelated tuplets, and
nothing currently prevents it.

No fixture demonstrates this yet, so it is a latent risk rather than a known defect; a two-layer
measure with a tuplet in each layer would settle it. The fix is a number allocated per part and
released when a tuplet ends, which is what `mx::impl::SpannerResolver` already does for every other
spanner family. Tuplets are excluded from it because `TupletStart` and `TupletStop` carry a raw
`numberLevel` int rather than an `api::SpannerNumber`, so either Denigma allocates part-scoped
levels itself or MX extends the resolver to tuplets. Note that MusicXML makes `number` optional and
defaults it to 1, and MX omits the attribute when the level is unspecified, so a measure with no
overlapping tuplets needs no numbering at all.

Two neighbouring tuplet defects are MX's and are filed there, not here: `<normal-type>` is written
from a sibling search rather than from the API field ([webern/mx#428](https://github.com/webern/mx/issues/428)),
and a single-note tuplet has its stop written before its start
([webern/mx#429](https://github.com/webern/mx/issues/429)). `MusicXmlTuplets` in
`tests/musicxml/test_tuplets.cpp` carries a disabled test for each, asserting the intended output
and naming the issue to re-enable it with.

## Tablature staves

Export Finale tablature staves as MusicXML tablature rather than as ordinary pitched staves.
Nothing reads `others::Staff::notationStyle` today, so a TAB staff exports as though it were
standard: the pitches are correct and everything that makes it tablature is dropped, with no
diagnostic. `Staff::NotationStyle` distinguishes `Standard`, `Percussion`, and `Tablature`, and it
can be overridden by a staff style, so the effective value is what matters.

Four pieces make up the feature:

- The TAB clef. MusicXML spells it `<clef><sign>TAB</sign>`, and its presence is what tells a
  reader that noteheads are fret numbers; the spec notes that a TAB clef alone is sufficient to
  imply that, so no per-note text is needed. `Staff::showTabClefAllSys` says whether Finale repeats
  it on every system.
- Staff details. `<staff-tuning>` per line, from the `others::FretInstrument` at
  `Staff::fretInstId`, whose `StringInfo::pitch` gives each open string's MIDI pitch and whose
  `nutOffset` shifts it. `<capo>` comes from `Staff::capoPos`, and `<show-frets>` from
  `Staff::useTabLetters`, which selects letters over numbers. `Staff::lowestFret` and `numFrets`
  have no direct MusicXML equivalent on a staff.
- Per-note string and fret, as `<technical><string>` and `<fret>`. This is the hard part, see
  below.
- Layout specifics with no MusicXML equivalent: `Staff::vertTabNumOff`,
  `breakTabLinesAtNotes`, and `hideTuplets`. Decide a downgrade policy rather than attempting them.

The per-note half needs real work in MUSX DOM terms. Finale does not store fret numbers.
`details::TablatureNoteMods` ("tabAlter") records only a `stringNumber`, and only for notes whose
string assignment was overridden; its documentation states that Finale derives the fret from the
open-string pitch and the fret intervals in the staff's `FretInstrument`. So Denigma must compute
the fret from the note's pitch, the string's open pitch and nut offset, and the capo, and must also
reproduce Finale's automatic string assignment for every note that carries no override. That
algorithm is not documented in MUSX DOM and would need to be established, ideally as a musxdom
helper rather than in the exporter, so MNX can share it.

The export half is additionally blocked upstream: `mx::api` models neither `<string>` nor `<fret>`.
See the tablature entry in [mx-api-gaps.md](mx-api-gaps.md).

Two neighbours are related but separate. Fretboard diagrams above cover `<frame>` on `<harmony>`,
which is chord diagrams rather than staff content, though both read `FretInstrument`. Bends
(`BendHat`, `BendCurve`) are guitar technique under `<technical>` and are noted with the glissando
work below.

Percussion staves have a parallel gap and are covered by the percussion item above. One detail
belongs here because it is shared: any future notion of "the same note" on a percussion staff would
have to compare percussion note type, not merely staff position, since one position can host
different instruments and one instrument can move position. Nothing depends on that today.

## Bends

Export Finale's `BendHat` and `BendCurve` smart shapes as MusicXML `<technical><bend>`, which the
spec describes as "used in guitar notation and tablature". A bend carries `<bend-alter>` for the
interval, an optional `<pre-bend>` or `<release>`, `<with-bar>` for whammy-bar notation, and a
`bend-shape` attribute distinguishing the angled symbols of standard notation from the curved ones
common to tablature. `<bend>` also shares the `bend-sound` attribute group with `<slide>`, so its
playback approximation is expressible.

These are the third and last family of note-attached Finale lines, alongside glissandi and tab
slides, and the only remaining one Finale attaches to noteheads. They are dropped today: neither
shape type is classified, so both reach the exporter as `std::monostate` and are logged by the
unclassified-shape path.

Nothing here is blocked upstream in an obvious way, but `mx::api` coverage of `<bend>` has not been
surveyed. Check it before starting, and record whatever is missing in
[mx-api-gaps.md](mx-api-gaps.md). Tablature staves above is a neighbour rather than a prerequisite:
a bend is notated in standard notation too, so this does not wait on TAB support.

## Shape-replaced stems

Export the Finale custom stems from `details::CustomStem` (`CustomUpStem` / `CustomDownStem`) that replace the stem with a Shape Designer shape. A custom stem that merely hides the stem already exports as `<stem>none</stem>`; a shape-replaced one keeps its ordinary direction, and the shape itself is dropped, as it is in Finale's own export.

An arbitrary stem shape has no MusicXML equivalent, so the case worth pursuing is a shape drawing one of SMuFL's combining tremolo stems, such as `stemPendereckiTremolo`. That is a tremolo in every sense except how Finale stores it, and it could export as `MarkType::tremoloUnmeasured` carrying the glyph name, exactly like the equivalent articulation. Recognizing it requires shape recognition in MUSX DOM, the same upstream dependency described below.

## Stacked single-note tremolos

Export single-note tremolos with six, seven, or eight slashes. `mx::api` models the whole MusicXML range through `MarkType::tremoloSingleSix`, `tremoloSingleSeven`, and `tremoloSingleEight`, but Denigma supports only one through five, because SMuFL precomposes only `tremolo1` through `tremolo5` and a Finale articulation is a single character or a Shape Designer shape. Both the classification and the exporter mapping remain to be done.

Finale can spell the higher counts only by stacking: a Shape Designer shape that draws several tremolo glyphs, or two tremolo articulations assigned to one entry. Recognizing the first requires a new `KnownShapeDefType` and recognizer in MUSX DOM, and a stack with variable count and spacing is a fuzzier recognition target than the fixed patterns already there. Recognizing the second requires entry-level aggregation plus vertical-offset geometry, since two tremolo articulations on one entry may equally well be two separate marks.

This is gated on evidence. Revisit it when a real-world Finale file actually spells such a tremolo; that file also settles which of the two routes is worth supporting.

## Extend the font-availability assertion to MNX

The shared `allFontsAvailable` export option and its `--all-fonts-available` CLI spelling tell Denigma that every source font will be available where the output is read. MusicXML uses it to select `utils::SmuflSymbolPolicy::PreserveText`; its default remains `SplitSmufl`.

Apply the same option to MNX once MNX has official formatted strings. Until then, leave its current `PreferSmufl` behavior alone rather than designing around a provisional text representation. At that point, reconcile MNX with MusicXML's two meaningful choices: without the fonts use `SplitSmufl`; with them use `PreserveText`. The intermediate `PreferSmufl` has no identified long-term use case.

The assertion may eventually inform other portability choices too. Generic font-family fallbacks are the obvious neighbor because they exist for the same missing-font case, but changing them should be decided separately rather than folded into formatted-text preservation.

## Text and custom-line fidelity

Convert eligible music-font characters in expression text to `SymbolData` within the ordered `DirectionChoice::wordsRun` model, particularly for legacy symbol fonts that may not be installed on the receiving system. Preserve unknown or intentionally font-specific characters as `WordsData`.

Use page-specific `PageData` layout overrides when computing absolute credit anchors. The exporter currently uses the score's default odd/even page size and margins, so credits on pages with Finale layout overrides may be misplaced.

Define intentional downgrade policies for Finale text and line features that MusicXML cannot represent. These include full and forced-full text justification; arbitrary Shape Designer text frames; page and measure text-block geometry such as fixed dimensions, insets, corner radius, line spacing, and word wrapping; custom-line continuation text shown after system breaks; and center full/abbreviated text on general bracket or dashes lines. Preserve the closest standard appearance where possible and log material omissions.
