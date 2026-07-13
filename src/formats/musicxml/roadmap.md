# MusicXML Feature Roadmap

This is the implementation to-do list for user-facing MusicXML export features. The targets describe planned scope, not a dependency order. Keep concrete `mx::api` limitations in [mx-api-gaps.md](mx-api-gaps.md).

## V1 targets

### Visible cue notes and rests

Export cue material that is visible in the target score or part as MusicXML `<cue/>` notes and rests. Skip cue material hidden in that target context. `mx::api::NoteData::isCue` already writes and reads cue, grace-cue, and cue-rest forms, so this is a Denigma policy and mapping task rather than an MX API feature.

The implementation needs to identify cue material per entry, resolve target-context hiding from entry visibility and effective staff-style alternate notation, set `isCue` in the note and rest creation paths, and retain visible cue-layer expressions. MNX keeps its current cue-discard behavior because it does not yet support cues. MusicXML cannot carry sound-level ties on cue or grace-cue notes; those ties will be omitted.

## V2 targets

### MIDI channels

Export each part's MIDI playback assignment — channel, and with it program and bank where available — as MusicXML `<midi-instrument>` data. `mx::api::PartData::midiData` already models channel, program, bank, volume, and pan, so no MX API work is needed.

Deferred to v2 because the source data requires musxdom effort first: Finale's channel assignments live in its playback system (instrument definitions and their staff/layer routing), which musxdom does not yet model. Once musxdom exposes that to some degree, this becomes a Denigma mapping task.

### Alternate notation: measure repeats and slash notation

Export effective staff alternate notation as MusicXML measure styles: one- and two-bar repeats, slash notation on beats, and rhythmic notation. This requires start/stop ranges per staff and, for slash notation, dots, stems, and where possible voice exclusions. It also needs a new public MX measure-style API and writer path.

Layer-only behavior, blank notation, and Finale's independent hiding of articulations, lyrics, expressions, and smart shapes are separate fidelity work. See [mx-api-gaps.md](mx-api-gaps.md) for the detailed limitation analysis.

### Chord symbols and fretboard diagrams

Export Finale `ChordAssign` records as MusicXML `<harmony>`, including chord roots, suffixes, alternate basses, placement, and fretboard diagrams. This does not refer to simultaneous note chords: those are already represented by MusicXML `<chord/>` note groups.

MUSX DOM exposes the assignment's staff, measure, EDU position, scale-degree root and alteration, alternate bass, visibility, display and playback flags, capo, and individual offsets through `details::ChordAssign`. The suffix is not a normalized chord quality: it is a library of positioned, font-based `others::ChordSuffixElement` glyphs. Fretboard groups, styles, and diagrams are referenced separately from the same assignment.

`mx::api::ChordData`, attached to `DirectionData::chords`, already writes rich MusicXML harmony data: root or functional/numeral source, kind, display text, degrees, alternate bass, inversion, position, and a frame diagram. No MX API work is necessary for a first export.

MuseScore's Finale importer provides a proven strategy. It resolves Finale's scale-degree root and bass against the effective key at the assignment position; reconstructs a Unicode chord string from suffix prefixes and font glyphs; normalizes Finale/SMuFL accidentals and chord symbols; then passes that string to MuseScore's built-in harmony parser. That parser is the robust classifier: it recognizes the reconstructed symbol rather than requiring the Finale importer to maintain a separate suffix-to-quality table. The same importer covers Standard, European, German, Solfeggio, Roman, Nashville A, Nashville B, and Scandinavian chord styles, with dedicated fixtures for each.

Denigma cannot call MuseScore's parser, but can reuse the reconstruction logic. A custom suffix's glyph sequence may be represented safely in MusicXML as `kind="other"` plus text. Chord-aware MusicXML importers can then apply their own robust harmony classifiers to that reconstructed display string, making this a useful interoperable first stage even without Denigma-side semantic classification. MUSX `others::ChordSuffixPlayback` also supplies the suffix's semitone offsets from the root, which define Finale's playback harmony and provide a strong input for optional v2 classification into MusicXML `kind` and `degree` values. The offsets alone cannot preserve every intended spelling or display convention, so the glyph text remains necessary for disambiguation and display. Finale-specific suffix fonts, per-glyph offsets, and many fretboard-style controls do not have a direct MusicXML equivalent.

Suggested stages:

1. Port the Finale-to-Unicode reconstruction logic; export root, alteration, alternate bass, staff, timing, and placement, with a text-preserving `kind="other"` fallback.
2. Optionally classify standard suffixes from their playback intervals, disambiguated by reconstructed glyph text, allowing MusicXML `kind` and `degree` values when recognized.
3. Convert fretboard diagrams into `ChordData::frameData`, then assess specialized frame appearance and capo behavior.
