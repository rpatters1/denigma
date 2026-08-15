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

## Glissandi and slides

Export Finale glissandi as MusicXML `<glissando>` and `<slide>` notations. Nothing is exported today: these are entry-attached smart shapes, and `classifySmartShape` has no case for them, so they classify as `std::monostate` and the MusicXML smart-shape visitor drops them without even a Verbose log. Adding that log is a worthwhile interim step, since a silent omission is currently indistinguishable from a shape Denigma never saw.

Two `others::SmartShape::ShapeType` values matter. `Glissando` is the obvious one. `TabSlide` is the one easily missed: it is a solid line intended for tablature, but it is frequently used as a note-attached glissando on ordinary staves, so it must map as well rather than being treated as a tablature-only feature. Keep both on one classification path so a tab slide on a standard staff still produces a glissando-family marking.

Neither shape type describes what was actually drawn. Each shape instance captures the line definition that was in effect when it was created and keeps it in its own `lineStyleId`. Configure the glissando tool as a straight line and assign one, then reconfigure it as a wavy line and assign another: the first stays straight and the second stays wavy, though both are `ShapeType::Glissando` and neither changes when the tool is reconfigured again. `options::SmartShapeOptions::ssLineStyleCmpGlissando` and `ssLineStyleCmpTabSlide` therefore name only what those tools would draw now, not what the shapes already in the document look like. The `others::SmartShapeCustomLine` each shape references is the authority for that shape's appearance, and it is what should drive both the choice between `<glissando>` and `<slide>` and the emitted `line-type`. MusicXML's own defaults give the reading: a wavy or character line is a glissando, a plain straight line is a slide.

Classification belongs in `src/classify`, alongside the other smart-shape classifiers and therefore available to MNX as well. It should resolve both endpoint notes, not merely their entries, since MusicXML attaches the start and stop to specific notes; note that a glissando may end on a grace note, for which MUSX DOM provides `Entry::calcIsGlissToGraceEntry`. It should also surface the custom line's appearance and its text, since the line style is where Finale keeps a "gliss." label. That text has a home in MusicXML only here: `appendGeneralLine` currently logs that custom-line center text has no MusicXML equivalent "outside of glissando text", and this is that exception.

A third source is less certain: the plain line tool. Any built-in solid or dashed line shape, and any custom line, can be entry-attached, and users draw slide and glissando marks that way instead of reaching for the dedicated tools. Those shapes are dropped today for the same reason the dedicated ones are: `classifyGeneralLine` returns no classification for any entry-based shape, and `classifySmartShape` keeps entry-based shapes out of the custom-line path, so they reach the exporter as `std::monostate`.

No single signal makes such a line certainly a glissando, and the current-style cmpers are worth little here for the reason given above: they describe what the glissando and tab slide tools would draw now, so a match is suggestive while a mismatch proves nothing. The available evidence is in the shape and its line definition together: text that names the marking, `startNoteId` and `endNoteId` both resolving to notes of differing pitch, the absence of hooks and arrowheads, and a line that is not forced horizontal, since a horizontal line is not a pitch slide. Gate this on real fixtures rather than inference, and let an entry-attached line with no corroborating evidence keep whatever general-line treatment is decided for it. That treatment is its own open question: such a line currently vanishes silently, and the beat-attached bracket or dashes path in `appendGeneralLine` is the obvious fallback, at the cost of the note attachment the user drew.

Bends are related but separate. `BendHat` and `BendCurve` are entry-attached shapes with their own MusicXML vocabulary under `<technical>`, and they should not be folded into this work.

The export half is gated on MX API support. `mx::api` models neither element, so this cannot be completed until the note-attached spanner model described in [mx-api-gaps.md](mx-api-gaps.md) exists. Classification, endpoint resolution, and fixtures can proceed ahead of it.

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
