---
name: marking-categories
description: How Finale marking categories relate to TextExpressionDef/ShapeExpressionDef fields. Use when reading horzMeasExprAlign, vertMeasExprAlign, horzExprJustification, useCategoryPos, useCategoryFonts, or any other field that a MarkingCategory and an expression def both carry.
---

# Marking categories vs. expression defs

`MarkingCategory` was added in Finale 2009 on top of the already-existing text/shape expression
definition feature. It did not refactor expression storage: `TextExpressionDef` and
`ShapeExpressionDef` still carry their own complete set of positioning/font fields
(`horzMeasExprAlign`, `vertMeasExprAlign`, `horzExprJustification`, text font, etc.), and a
`MarkingCategory` carries a parallel copy of the same kinds of fields (`horzAlign`, `vertAlign`,
`justification`, fonts).

A category exists purely as a Finale UI convenience: editing the category's settings mass-edits
every expression def assigned to it, writing the new values into each def's own fields. `useCategoryPos`
and `useCategoryFonts` on an expression def just mean "this def's own fields were populated from
its category, and should keep tracking it through Finale's UI" — they are not a live join/pointer
resolved at read time.

**The expression def's own field is always the canonical value.** Never resolve a def's
positioning/font field by falling through to `MarkingCategory` when `useCategoryPos`/
`useCategoryFonts` is set. The def's own field already holds the effective value (Finale keeps
it in sync whenever the category changes); reading through to the category instead is redundant
at best, and wrong if the sync is ever stale or the category can't be resolved.

```cpp
// Correct: read the def's own field directly.
const auto align = textExpression->horzMeasExprAlign;

// Wrong: do not fall through to the category.
if (textExpression->useCategoryPos) {
    if (auto category = assignment->getMarkingCategory()) {
        align = category->horzAlign; // redundant / potentially stale
    }
}
```

`useCategoryPos`/`useCategoryFonts` are worth reading only for UI-facing purposes (e.g. Finale
plugin code that wants to know whether editing the category would also change this expression), not
for computing what an expression currently looks like.

Do not add a comment at every ordinary access site explaining that you're not falling through to
the category and why. This skill is the one place that reasoning belongs; repeating it at each
call site becomes redundant noise as more code reads these fields. Just read the def's own field
plainly, the way any other field is read.
