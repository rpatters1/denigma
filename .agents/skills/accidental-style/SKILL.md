---
name: accidental-style
description: Denigma convention for rendering accidentals in part, instrument, and expression names. Use when calling any musxdom API that takes an EnigmaString::AccidentalStyle, or when naming an output file, writing a log message, or writing a name into an exported document.
---

# Accidental Style

Denigma renders accidentals two ways, chosen by where the text lands.

Rules:

- Text written into an exported document uses `EnigmaString::AccidentalStyle::Unicode`. Export files
  are UTF-8 and carry the proper glyphs, so a part named "Clarinet in B♭" keeps its flat sign. This
  covers instrument and group names, expression text, and score names in every exporter.
- Output filenames and log messages use `EnigmaString::AccidentalStyle::Ascii`, which is musxdom's
  default for `getName` and friends. Pass nothing rather than naming the style.
- Both settings are deliberate. When a name reaches both destinations, resolve it separately for each
  rather than picking one and reusing it.

## Naming a linked part

`denigma::calcLinkedPartDisplayName` (`src/core/denigma.h`) is the single routine for identifying a
linked part **in a filename or a log message**. It returns the part's name, or falls back to
`"Part <cmper>"` when Finale left the part unnamed, and `"Score"` for the score. A null instance
returns an empty string, so callers naming the score's own output file pass null and get an
unsuffixed filename.

Do not use it for content written into an export. Two reasons:

- Exports need Unicode accidentals, and this routine does not offer them.
- The `"Part <cmper>"` fallback is a Denigma presentation choice, not something the document states.
  That is also why the routine lives in Denigma rather than musxdom.

An exporter that needs a name for its own document content resolves it itself, with Unicode
accidentals and whatever fallback suits that format. `createScores` in `src/formats/mnx/mnx.cpp` is
the worked example.
