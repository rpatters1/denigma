# MusicXML Feature Backlog

This is the implementation backlog for larger user-facing MusicXML export features. It is not a version plan or a dependency order. Keep concrete `mx::api` limitations in [mx-api-gaps.md](mx-api-gaps.md).

## Compressed MusicXML (`.mxl`) output

Export standards-compliant compressed MusicXML in addition to uncompressed `.musicxml` files. Each archive should contain an uncompressed `mimetype` entry first, a `META-INF/container.xml` file that identifies the root MusicXML document, and the compressed score document. The existing ZIP utilities and MXL massage support should supply most of the required archive infrastructure.

Initially, write one `.mxl` archive for each score or part document that Denigma currently emits separately. Packaging the score and all linked parts into one MusicXML 4.0 archive through `<part-link>` can be considered as a later extension rather than a prerequisite for basic compressed output.

## Nontraditional and microtonal key signatures

Export Finale nontraditional key signatures through MusicXML's ordered `<key-step>`, `<key-alter>`, and optional `<key-accidental>` values instead of degrading them to zero fifths. Begin with custom 12-EDO signatures, for which MUSX DOM can provide the key map and `mx::api::KeyData::nonTraditional` already provides a writer path.

Extend the same mapping to microtonal key signatures by converting Finale's EDO divisions into MusicXML semitone alterations and suitable accidental values. Preserve the effective written or concert-pitch signature independently for each staff, as the exporter already does for traditional keys.

## Visible cue notes and rests

Export cue material that is visible in the target score or part as MusicXML `<cue/>` notes and rests. Skip cue material hidden in that target context. `mx::api::NoteData::isCue` already writes and reads cue, grace-cue, and cue-rest forms, so this is a Denigma policy and mapping task rather than an MX API feature.

The implementation needs to identify cue material per entry, resolve target-context hiding from entry visibility and effective staff-style alternate notation, set `isCue` in the note and rest creation paths, and retain visible cue-layer expressions. MNX keeps its current cue-discard behavior because it does not yet support cues. MusicXML cannot carry sound-level ties on cue or grace-cue notes; those ties will be omitted.

## MIDI channels

Export each part's MIDI playback assignment — channel, and with it program and bank where available — as MusicXML `<midi-instrument>` data. `mx::api::PartData::midiData` already models channel, program, bank, volume, and pan, so no MX API work is needed.

The source data requires musxdom effort first: Finale's channel assignments live in its playback system (instrument definitions and their staff/layer routing), which musxdom does not yet model. Once musxdom exposes that to some degree, this becomes a Denigma mapping task.

## Alternate notation: measure repeats and slash notation

Export effective staff alternate notation as MusicXML measure styles: one- and two-bar repeats, slash notation on beats, and rhythmic notation. This requires start/stop ranges per staff and, for slash notation, dots, stems, and where possible voice exclusions. It also needs a new public MX measure-style API and writer path.

Layer-only behavior, blank notation, and Finale's independent hiding of articulations, lyrics, expressions, and smart shapes are separate fidelity work. See [mx-api-gaps.md](mx-api-gaps.md) for the detailed limitation analysis.

## Fretboard diagrams

Export Finale fretboard diagrams through MusicXML `<frame>` data on their associated `<harmony>` elements. This does not refer to simultaneous note chords: those are already represented by MusicXML `<chord/>` note groups.

MUSX DOM exposes the fretboard groups, styles, and diagrams referenced by `details::ChordAssign`. `mx::api::ChordData::frameData` can represent the basic string/fret grid, first fret, barre, and fingering data.

First export the basic diagram and its note/fingering/barre details, then assess specialized frame appearance, diagram placement, and capo behavior.
