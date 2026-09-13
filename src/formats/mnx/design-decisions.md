# MNX Design Decisions

Deliberate choices the MNX exporter makes, and why. These are settled positions, not open work.

Deferred features and concrete `mnxdom` limitations belong with the code that runs into them. If a decision recorded here is ever reversed, delete the entry rather than leaving it to contradict the code.

## Tempo

### A tempo states what Finale plays, not what the page shows

An MNX tempo is a playback instruction. It carries a number and a note value and nothing else: there is no way to give it visible text, and a reader that draws it at all draws a generic metronome marking of its own devising. So the number Denigma writes is the tempo Finale would play, and the appearance of the Finale expression that produced it is not part of the question.

A Finale text expression stores a playback tempo, as a value and a beat unit, separately from whatever its text happens to say, and the two are free to disagree. Those stored settings are what Denigma exports, because they are what Finale sounds. An expression with no playback settings at all falls back to its displayed equation, which is the only tempo it has. A displayed number written with a decimal point survives that fallback, since MNX `bpm` is a real number as of schema version 34.

Finale's "Match Playback to Metronome Marking Text" setting is not consulted. It appears to keep the stored value in step with the text as the text is edited, which leaves the stored value correct and makes reading the text unnecessary; what it does not do is supply a beat unit, so an expression can display one note value and sound another. `metronome_marks.musx` holds such a case: its first mark displays an eighth and stores a playback beat of a quarter, and it exports as the quarter it sounds.

The displayed equation is not lost by this. MusicXML has room for both numbers and writes both: `<per-minute>` takes the printed equation and `<sound tempo>` takes the playback tempo. Only MNX, having one field, has to choose.

### A playback beat unit that is not a note value is restated in quarter notes

MNX states a tempo as a count of one note value per minute, so the beat unit has to be a note value: a base with some number of augmentation dots. Finale's beat unit is a raw EDU duration and need not be either. `calcDurationInfoFromEdu` answers with the closest base and dot count for any duration in range rather than refusing, so the exporter spells its answer back out and compares it against the original before trusting it.

A beat unit that does not survive that comparison is restated as a count of quarter notes. Nothing is lost by the restatement: MNX `bpm` is a real number as of schema version 34, and dividing by the EDUs in a quarter note divides by a power of two, which is exact in binary floating point. The same path takes a beat unit outside the range of a note value, which `calcDurationInfoFromEdu` would otherwise throw on.

## Sequences

### An empty staff measure is written as a full-measure rest

MNX requires a part-measure to carry a `sequences` array but says nothing about what an empty array means. A reader is free to draw nothing for it, and the spec's own full-measure-rest example spells an empty bar out as a sequence with `fullMeasure` set. So a staff that exports no music in a measure gets exactly that: one sequence with empty `content` and `fullMeasure: {}`, naming its staff when the part has more than one. This matches the MusicXML exporter, which writes a `<rest measure="yes"/>` for the same measures.

Whether the rest appears is decided from the staff as it stands at the start of the measure, the same way Finale decides it. "Display Rests in Empty Measures" turned off, whether on the staff or through a staff style, leaves the measure with no sequences. So does an alternate notation that replaces entries, since a measure repeat or slash region draws its own content and a blank-notation region draws nothing; the measure-repeat example in the spec likewise carries no full-measure rest. "Display Rests", which hides entered rests, is not consulted: MNX has no way to hide a rest, and the exporter already exports entered rests regardless of it. Consult it here and for entered rests if MNX ever gains a `hidden` option on rests.

Cue entries are skipped by the exporter and have no MNX representation yet, so a measure holding only cues counts as empty and gets the rest its staff settings call for. Revisit this when MNX gains a cue encoding.

A staff-attached fermata over an empty measure attaches to that staff's full-measure rest. A staff whose settings suppressed the rest gets one created for the fermata anyway, because MNX has nowhere else to put it.
