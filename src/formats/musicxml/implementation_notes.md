# MusicXML Implementation Notes

Observations gathered while implementing the MusicXML exporter that do not belong in the sibling documents. Deferred work belongs in [roadmap.md](roadmap.md), concrete `mx::api` limitations in [mx-api-gaps.md](mx-api-gaps.md), and Denigma's own deliberate choices in [design-decisions.md](design-decisions.md).

## Importer behavior

What other applications do with output Denigma believes is correct, and why the output is not being changed to suit them. Each entry was measured against Finale's own export of the same fixture, so "we match the reference" means element for element unless stated otherwise.

These are recorded because the same reports keep arriving and the investigation is expensive to repeat. None of them is a defect on Denigma's side. Where one turns out to be, it belongs in the record too, so the last entry is a case that was.

### Dorico replaces elision glyphs

`for_health_and_strength.musx` contains nine elisions: eight carrying a no-break space (U+00A0) and one carrying an undertie (U+203F). Denigma's output is byte-identical to Finale's reference on all nine, attributes included. Dorico renders its own elision symbol for every one of them, discarding the distinction between the two.

MusicXML is prescriptive here. Its reference says the `<elision>` element's text content "specifies the symbol used to display the elision", names the no-break space, underscore, and undertie as common values, and reserves application-specific behavior for the case where neither the text content nor the `smufl` attribute is supplied. Denigma supplies text content in every case, so Dorico is overriding an explicit instruction rather than filling a gap.

Nothing is available to change: any deviation would move Denigma away from what Finale itself writes, and the elision still reads as an elision. Only the choice of glyph is lost, and the syllable grouping that matters for playback and re-export survives intact.

### Bare wavy lines are mishandled by every reader tested

`wavy_lines.musx` holds five beat-attached shapes: two trill extensions, one trill with a tr symbol, and two vibrato lines. Denigma's output matches Finale's reference on all ten `<wavy-line>` elements, differing only in the order of two ends that share one note and carry different numbers, which either order reads unambiguously.

A trill extension with no accompanying `<trill-mark>` is the case readers struggle with:

| Reader | Bare wavy line | Bare wavy line carrying `smufl` |
|--------|----------------|---------------------------------|
| Finale | dropped on import | imported |
| Dorico | dropped | dropped |
| MuseScore 4 | imported as a trill, wrong length | not measured |
| OSMD | drawn, over-extended to the barlines | not drawn |

Finale keeping the second and dropping the first is the informative pair, since the two are structurally identical apart from the `smufl` attribute. The glyph appears to give Finale something to build a line from, where a bare wavy line with no `tr` has no Finale representation. MuseScore inventing a `tr` that is not in the file is the same gap resolved in the opposite direction.

Denigma writes what the format provides, so there is nothing to adjust. Note that Finale does not round-trip its own export here either.

### MuseScore over-applies a lyric's vertical offset

One syllable in `for_health_and_strength.musx` carries a per-assignment offset of 68 Evpu, which is 28.33 tenths, or 2.83 staff spaces. The verse baselines in that document are 40 Evpu apart, so the offset is larger than a full lyric line.

Denigma writes it as `relative-y="28.33"`. Finale writes the same displacement as an absolute `default-y="-81"`, its other verse-2 lyrics sitting at `-109`. Both encode the same intent, and Denigma's value matches Finale's to within rounding.

MuseScore treats the two encodings differently. It discards `default-y` on lyrics in favor of its own layout, so importing Finale's export silently loses the offset and the syllable lands on the verse-2 line, looking correct while dropping what the author asked for. It honors `relative-y`, applies the full 2.83 spaces from its own verse-2 position, collides with the note, and evicts the syllable above the staff.

Denigma emits offsets rather than absolute positions by policy; see the engraving-geometry entry in [design-decisions.md](design-decisions.md). Matching Finale would mean deriving an absolute lyric baseline, and the obvious derivation does not reproduce Finale's numbers: baseline `-184` Evpu converts to `-76.7` tenths where Finale writes `-109`, so a reference-point or text-metric term is unaccounted for. That would need solving before absolute lyric positions could be written at all.

### An importer complaint that was Denigma's fault

Recorded so this document does not read as a catalogue of other people's bugs.

Denigma once identified a lyric line by `<lyric number>` alone, encoded as `v1`, `c1`, `s1` to keep Verse 1 distinct from Chorus 1. That is valid, since `number` is an `NMTOKEN` and nothing requires it to be numeric. Dorico stacked every verse on one line, and MuseScore marched them down the page. Both read `number` as the line position and could not parse the value.

The scheme was held on the grounds that the cost fell on one importer. When Dorico turned out to fail too, that premise was gone, and MusicXML had a dedicated attribute for the case all along. The fix is in the lyric-identity entry in [design-decisions.md](design-decisions.md).

The lesson is not that importers are always right. It is that "our output is spec-correct" settles the specification question and not the practical one, and the two should be checked separately.
