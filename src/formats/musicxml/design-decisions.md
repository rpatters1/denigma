# MusicXML Design Decisions

Deliberate choices the MusicXML exporter makes, and why. These are settled positions, not open work: an entry here exists so that a later reader who compares Denigma's output against Finale's own MusicXML export can tell a decision from a defect.

Deferred features belong in the [MusicXML feature roadmap](roadmap.md), and concrete `mx::api` limitations belong in [mx-api-gaps.md](mx-api-gaps.md). If a decision recorded here is ever reversed, delete the entry rather than leaving it to contradict the code.

## First principle: Denigma has no layout engine

Denigma reads a stored Finale document and converts it. It does not engrave music, and it will not acquire a layout engine in order to fill in MusicXML fields that describe an engraved result.

This matters most when comparing against Finale's own MusicXML export, which is the obvious reference and a misleading one. Finale exports from a score it has already laid out, so it emits a large body of information that is engraver output rather than anything the author entered: where each note sits horizontally, how wide each measure is, how far apart the systems are, which accidentals its heuristics chose to draw. Denigma has the document, not the rendering. Reproducing those values would mean reimplementing Finale's spacing, collision avoidance, accidental logic, and system breaking, and the result would be a body of brittle heuristics that is wrong in exactly the cases that matter.

The operating rule is therefore: **export what the document stores; do not synthesize what an engraver computes.** The dividing line is stored authorial content against computed layout result, not positional data against non-positional data. Finale stores absolute page coordinates for page text blocks, so those are exported; it computes a note's horizontal position during layout, so that is not.

Two corollaries are worth stating outright, because both have been mistaken for defects:

Attributes that encode notation rather than layout are still exported, when the source determines them. `placement`, above and below on a direction, and stem direction are notational facts a reader would name aloud, and they come from Finale data. Dropping one that a classifier did resolve is a defect, not an application of this principle.

The converse holds just as firmly, and is easy to get backwards. When a classifier cannot resolve such a value, the value is not emitted. A hairpin drawn between two staves is the standard case: `calcVerticalPlacementForBeatAttached` returns no placement, and the right output is a `<direction>` with no `placement` attribute. Supplying the conventional guess instead, on the reasoning that hairpins are usually below, would be synthesizing a fact the document does not contain, which is the same error as inventing geometry. An importer that has to decide for itself is better served by silence than by a plausible fabrication it cannot distinguish from real data.

The omissions below are not gaps to be closed. They should not be filed as roadmap items, and an importer's inability to re-derive one of them is not by itself a reason to revisit the principle. Every importer Denigma targets re-engraves the music on import, and approximate values are worse than absent ones, because an importer that trusts a bad number produces a worse result than one left to its own engraver.

## Consequences of having no layout engine

### No engraving geometry

A note's `default-x`, a `<measure>`'s `width`, a slur's `bezier-x` and `bezier-y`, a wedge's `spread`, and the per-system `<system-distance>`, `<top-system-distance>`, and `<staff-distance>` inside `<print>` are all layout results. None of them is exported.

Denigma does export position for anchors Finale stores directly. Page text blocks carry absolute page coordinates in the document, so `<credit-words>` receives real `default-x` and `default-y` values derived from the page margins and the text block's assignment. That is stored authorial placement, and it is the clearest illustration of where the line falls.

### System breaks come from authored sources only

`createSystemBreaks` derives `<print new-system="yes">` from the per-measure "Begin a New Staff System" flag and from `others::SystemLock`, and from nothing else. It never reads the resolved `others::StaffSystem` list.

A system boundary that exists only because Finale's engraver happened to fill a line there is layout, and re-emitting it freezes one particular rendering into a file the importer is about to re-flow. A lock or an explicit break flag is something the author asked for, and it survives the round trip.

The two sources also differ in trustworthiness. A part whose page layout Finale never calculated leaves zero-valued placeholders in `others::StaffSystem` and `others::Page`, so the resolved layout may describe systems the part does not have. The per-measure flag resolves nothing and stays valid; locks are dropped for such a part rather than exported against a layout that was never computed.

### Accidentals and stems are encoded as overrides only

`<encoding>` declares `<supports element="accidental" type="no"/>` and `<supports element="stem" type="no"/>`, and the exporter then writes only the cases an importer's own heuristics cannot derive: an accidental that Finale froze or parenthesized, and a stem direction that overrides the default.

Whether an accidental would be displayed by ordinary engraving heuristics is not something Denigma can determine, but whether the user forced one on is recorded in the document and is worth preserving. Declaring `type="no"` and writing only the overrides encodes exactly what Denigma knows and nothing it does not.

This is the correct use of `<supports>`, not a workaround. `type="no"` declares that the encoding does not include *all instances* of the element, so the absence of one is uninformative and the importer should apply its own heuristics. It does not forbid the element or instruct importers to ignore it: an `<accidental>` that is present is still honored. That reading was confirmed by Michael Good in [MusicXML issue #664](https://github.com/w3c-cg/musicxml/issues/664), which was opened against this exporter's behavior and resolved into a request to make the specification's wording explicit rather than a request for new vocabulary.

One limitation is accepted rather than solved. MusicXML can force an accidental to display but cannot force one to be absent, since absence is itself how "no accidental" is spelled. A Finale accidental hidden by the user is therefore lost, and `print-object` on `<accidental>` is not a usable substitute, because software predating the attribute would read the element as meaning the opposite. Importer behavior also varies and is outside Denigma's control: Finale honors this encoding as intended, MuseScore reaches the same result while ignoring `<supports>` entirely, and Dorico ignores both the declaration and the element and applies its own courtesy-accidental logic.

## Other encoding choices

These do not follow from the first principle. They are cases where Finale's own export takes a lossy or malformed shortcut and Denigma does not follow it.

### Hidden entries keep their rests

An entry hidden in the requested context exports as `<rest print-object="no">` with its real `<type>`, not as `<forward>`.

Finale's own export collapses hidden rests into `<forward>`, which advances the musical position and discards the fact that a rest is there at all. A `print-object="no"` rest keeps the entry addressable, so directions, lyrics, and spanner endpoints attached to it still have something to attach to, and an importer that later chooses to reveal hidden material has the rest's duration type rather than a bare duration.

### A floating rest keeps floating

`<display-step>` and `<display-octave>` are written for a rest that Finale positions explicitly, and omitted for one Finale floats.

Floating is itself the authorial choice, and preserving it matters more than reproducing any particular vertical position. Finale, Dorico, MuseScore, and almost certainly Sibelius each offer a rich set of preferences governing where rests sit when voices share a staff: whether to displace them at all, by how much, how to treat paired voices, how whole rests differ from the rest. A rest the author left floating is a rest whose placement those settings are meant to govern. Writing a display pitch overrides every one of them and pins the rest where one application would have put it, permanently, in every program that later opens the file.

This reasoning does not depend on Denigma having no layout engine, and the entry is filed here rather than above for that reason. Whether a floated rest's position could be derived is beside the point, because it should not be written even if it could.

An explicitly positioned rest is the opposite case. There the author overrode the application's placement, and the override is exactly what should survive.

Finale's own export writes a resolved position for every rest, floating or not, which discards the distinction entirely.

The separate question of whether an explicitly positioned rest's display pitch means Finale's nominal position or the SMuFL glyph origin is unsettled in MusicXML itself; see the whole-rest position item in the [roadmap](roadmap.md).

### Lyric verse numbers carry their Finale lyric type

`<lyric number>` is written as the first letter of the Finale lyric block's node name followed by that block's number, so Verse 1 becomes `v1`, Chorus 1 becomes `c1`, and Section 1 becomes `s1`. Finale keeps three independent lyric blocks, each numbered from 1, and a note can carry a syllable from more than one at once. A plain integer would collide Verse 1 with Chorus 1 and merge two distinct lyric lines into one.

MusicXML permits this. `<lyric number>` is an NMTOKEN, an identifier used to distinguish and align lyric lines, not an ordinal, and nothing in the specification requires it to be numeric.

Importers vary in how well they honor that. Dorico handles the scheme correctly. MuseScore assigns a new vertical line per distinct number value rather than inferring from usage how the lines are actually laid out, so a document that switches between numbers marches its lyrics down the page. That is MuseScore reading an identifier as an ordinal, and it is tracked as a MuseScore issue rather than a reason to change the scheme; note that MuseScore's MNX importer infers this correctly, so the behavior is not inherent to the problem.

This is held until real-world evidence argues otherwise. The cost is confined to one importer, while the benefit, not merging distinct lyric blocks, applies everywhere.

### The Finale title becomes work-title, not movement-title

`setFileInfoText` maps Finale's Title to `mx::api::ScoreData::workTitle`, so the document's title appears as `<work><work-title>`. Finale's own export writes `<movement-title>` instead.

The Finale field is called Title and holds the name of the work, and `<work-title>` is the element for the name of a work, so the mapping is the direct one. MusicXML's split between a containing work and a movement within it is the awkward part: it serves a multi-movement collection well and has no good answer for the ordinary single-piece file, where either element can be argued for and importers disagree about which they read. Given a defective choice, matching the field's own meaning is the defensible reading, and the title is in any case also carried visibly by its `<credit>`.

This is held provisionally, subject to real-world importer behavior rather than to further argument from the specification. If it turns out that the importers Denigma targets consistently do the wrong thing with `<work-title>`, delete this entry and change the mapping.

### The subtitle becomes a miscellaneous field, not a creator

Finale's Subtitle file-info field is written as `<identification><miscellaneous><miscellaneous-field name="subtitle">`. It is also emitted as a `<credit>` with credit-type `subtitle` wherever a page text block inserts it, but that is the page text path's doing and depends on the subtitle actually being placed on a page.

MusicXML has no subtitle element and no way to add one. `<work>` offers only `<work-title>` and `<work-number>`, and the `identification` complexType is a closed `xs:sequence` with no `xs:any`, so a `<subtitle>` child would make the document schema-invalid rather than merely unread.

What is available is a non-standard `type` value. `creator`, `rights`, and `relation` are all `typed-text`, whose `type` is an unconstrained `xs:token`, and the specification says other type values may be used. `<creator type="subtitle">` therefore validates. It is rejected anyway, because `<creator>` is Dublin Core creator and the attribute is open for creative roles: writing the subtitle there asserts that the subtitle is a person who made the score. An importer that handles unknown creator types generically, and several list them, would print the subtitle in the composer block. Wrong metadata is a worse outcome than absent metadata, since a reader cannot tell it is wrong.

`<miscellaneous-field>` makes no such claim. Its own documentation describes it as the place for metadata not yet supported in the MusicXML format, which is this case exactly, and `mx::api` routes `EncodingData::miscellaneousFields` into `<identification><miscellaneous>` despite the field hanging off the encoding model.

`<movement-title>` is genuinely free, since Finale's Title goes to `<work-title>` per the entry above, and it is a standard element importers do read. It is not used for the subtitle because MuseScore and Dorico both surface `movement-title` as the piece's main title, so the subtitle would compete with the real one.

### Unresolved ties become let-ring ties

A tie whose start has no reachable end, either because Finale recorded no tie end or because the target entry is hidden, exports as `<tied type="let-ring">` rather than an unterminated `<tie type="start">`.

`<tied type="let-ring">` is MusicXML's element for exactly this: a tie that is drawn and sounds but has no destination note. An unterminated `<tie type="start">` is malformed, and importers that honor it leave the note sounding indefinitely. Finale's export writes the malformed form; Denigma does not follow it there.

### Music-font characters become symbols, splitting the run around them

A character that resolves to a canonical SMuFL name is exported as `<symbol>` rather than as text in its source font, and a chunk mixing mappable and unmappable characters is split so the glyph converts and the rest stays words. `utils::SmuflSymbolPolicy` models the alternatives, and `SplitSmufl` is both the default and the only value a MusicXML export currently reaches.

Splitting and not splitting differ only for a mixed run, which is what a legacy metronome font produces when a note glyph and its number are typed together. Compare how each degrades on a system without that font. Splitting yields a portable glyph followed by digits and punctuation that any fallback font renders correctly. Not splitting keeps the whole run as text in a font the reader does not have, so the glyph renders as whatever character occupies that codepoint elsewhere. Denigma's own corpus shows the failure: `tempo_varied_staves.musx` carries "Tempo (♩=120)" in the legacy font Patmm, which Denigma used to export as a raw character that reads as "Tempo (∞=120)" anywhere Patmm is missing. It now exports `metNoteQuarterUp`.

The cost is real, and it is not recovered elsewhere. A reader who does have the font receives an unsplit run completely intact, kerning and all, because the font travels on the `<words>` element and the importer applies it. Some legacy metronome fonts kern a note against its number deliberately, and splitting discards that. So this policy knowingly degrades output for readers who would otherwise have had none.

It is chosen anyway because the two failures are not comparable. A reader with the font loses spacing: visible, minor, and obviously a layout matter. A reader without it sees the glyph replaced by whatever character occupies that codepoint, which reads as data corruption, gives no hint of the original, and cannot be repaired from the file. Preferring a small certain loss over an occasional unrecoverable one is the trade being made, and it is a judgement about which readers to favor rather than a case where one policy dominates.

What a `<symbol>` carries about its font follows from whether that font can draw the glyph. A SMuFL source keeps its whole font data, family included: the face really does contain the glyph under this name, so naming it gives a reader who has it the source's own design, while the family list degrades to its generic for a reader who does not, which is exactly what such a list is for. The generic is swapped for an engraving one, since the fallback appended for running prose would send a reader to a text font that cannot draw a glyph at all. A legacy source keeps neither its family nor its bold or italic, because the name means nothing in that face and there is nothing to point a reader at, and because a synthesized slant on a glyph that has none is not wanted.

Size is different, and is carried across converted. A SMuFL music font sets one em to four staff spaces, which is what makes a point size portable from one such font to another and keeps a tempo glyph deliberately smaller than staff size. Legacy fonts promise nothing of the kind, so `smufl_mapping` records per font how many staff spaces one em actually spans, and the exporter multiplies by the resulting ratio rather than assuming it. Every Finale music font measured so far agrees with SMuFL's four, so the conversion is currently an identity everywhere it applies, and Denigma's output matches Finale's own for the same document. A font whose em is not staff-relative at all still states no size: the metronome font Patmm measures anywhere from 1.09 to 7.05 staff spaces per em across its mapped glyphs, so nothing can be derived from its point size.

Dropping the size, which is what Denigma did before the measurements existed, is not the neutral choice it appears to be. It hands the reader a default that is usually full staff size, so a glyph the author deliberately set smaller than the staff comes back oversized. Stating a size that is slightly wrong is a smaller error than stating none, and for every font now in the registry the size is not wrong at all.

Note that a SMuFL *text* face is not four spaces to the em: Bravura Text and its peers use five, while MakeMusic's text faces stay on the music scale. That does not affect a SMuFL source here, because such a source keeps its own family and so is read back in the face the size was measured against.

Style and weight are always stated, since `mx::api::FontData` leaves them unspecified by default and an unspecified style inherits from whatever ran before; a legacy source gets an explicit normal rather than nothing, exactly as ordinary words do.

The scope of the trade is narrower than it looks. The policies diverge only for a run mixing mappable and unmappable characters in one font. A SMuFL font run is ordinarily a single glyph, which both policies convert identically, so the divergence is confined to legacy symbol fonts.

The default assumes the worst about the reader's fonts because Denigma has no way to know better. The user does, and the roadmap's font-availability assertion is the intended way to say so. Once the fonts are known to be present the faithful setting is `PreserveText`, which substitutes nothing at all: converting even a wholly-mappable run would replace the source glyph design with the reader's music font, which is the substitution such a user is declining.

This is also why MusicXML does not need to parse metronome markings out of expression text. Finale splits its own chunks at font changes, so "Adagio espressivo ♩ = 84" arrives as three chunks and exports as words, symbol, words: faithful text plus a portable glyph, with `<sound tempo>` carrying the playback.

### Tuplet ratios are reduced, and the printed spelling travels separately

`<time-modification>` is written from the entry's cumulative ratio, so a Finale tuplet of six sixteenths in the space of four is exported as `3:2`. Finale's own export writes the unreduced `6:4`. The printed numbers are unaffected: `<tuplet-actual>` and `<tuplet-normal>` carry Finale's display number and reference number with their durations, so the tuplet still reads as "6 in the space of 4" on the page.

The two elements answer different questions, and MusicXML intends the split. `<time-modification>` states how long the note actually sounds, while the `<tuplet>` notation states what the engraver drew. Denigma takes the sounding ratio from `EntryInfo::cumulativeRatio`, which is the product of every tuplet in force at that entry, and lowest terms simply falls out of the fraction being normalized. Reduction is therefore a consequence of computing the right quantity, not a separate tidying step.

Copying Finale's display numbers into `<time-modification>` instead would be wrong as soon as tuplets nest, because the display numbers describe one tuplet while the timing effect is cumulative. Finale's own export shows the hazard. In `zwei_gesange.musx` it writes `36:16` for a tuplet whose notation reads 6:4, with durations that leave the piano's first voice holding 240 of the measure's 288 divisions, so the measure does not add up. Denigma's `3:2` for the same tuplet sums exactly.

Nothing is lost by reducing, and that holds unconditionally. `createTupletStart` always populates the display and reference numbers, including for a hidden tuplet, where `TupletDef::hidden` suppresses the show flags but leaves the values intact.

A related difference is not Denigma's doing. Denigma sets `<normal-type>` only when the tuplet's reference duration differs from the note's own type, but `mx::impl::NoteWriter` ignores that field and infers the element by scanning sibling notes, so it appears far more often than Denigma requests. See the nested-tuplet entry in [mx-api-gaps.md](mx-api-gaps.md).
