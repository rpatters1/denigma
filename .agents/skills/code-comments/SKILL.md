---
name: code-comments
description: Denigma code comment conventions. Use when writing or revising a Doxygen comment in a public header, an implementation comment anywhere in src, or when reviewing the comments in a change.
---

# Code Comments

A Doxygen comment documents the contract. An implementation comment explains the code as it
now stands. Neither one records how the code came to be that way.

Rules:

- In `include/denigma` and anywhere else Denigma publishes a declaration, state what a caller
  must know: behavior, parameters, return values, and what is thrown. Do not explain why a
  declaration was added, what it replaced, or which problem prompted it. A consumer such as
  `denigma-online` arrives without that context, and the rationale ages badly once the
  surrounding code moves on.
- In implementation code, say why a nonobvious choice is correct, not how it was arrived at.
  Write what someone needs in order to edit the surrounding lines safely.
- Do not narrate. Leave out a comment that restates the statement below it, recounts what was
  tried first, or argues against an alternative the code does not take.
- Keep one source of truth. A deliberate policy choice belongs in the matching
  `design-decisions.md`, a third-party limitation in the matching gaps document, a finding about
  another application in the matching `implementation_notes.md`, and deferred work in the
  matching `roadmap.md`. Where one of those already carries the argument, name the behavior in a
  line or two and stop; do not restate the argument inline.
- Write a comment that survives ordinary edits. Do not enumerate every case a function handles,
  or the list goes stale the next time one is added.
- A comment is not correspondence. Do not address a reviewer, answer a question that was raised
  in review, or carry over the wording of a conversation.

The `/// @todo` rule in `AGENTS.md` applies to comments as well.
