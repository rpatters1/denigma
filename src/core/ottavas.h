/*
 * Copyright (C) 2026, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <functional>
#include <unordered_map>

#include "denigma/classify/smartshapes.h"
#include "musx/musx.h"

namespace denigma {

/// @struct OttavaInstance
/// @brief A semantic-carrier ottava: the assigned smart shape together with its classification.
///
/// Carriers are the shapes that determine octave displacement: visible built-in ottavas,
/// hidden built-in ottavas (whether or not a visible custom line renders them), and
/// unpaired visual ottava custom lines. Paired visual lines are appearance-only and are
/// never collected.
struct OttavaInstance
{
    musx::dom::MusxInstance<musx::dom::others::SmartShape> shape;  ///< The assigned smart shape.
    classify::smartshape::Ottava classification;                   ///< Its ottava classification.
};

using OttavaShapeMap = std::unordered_map<musx::dom::Cmper, OttavaInstance>;

bool isOttavaShapeType(musx::dom::others::SmartShape::ShapeType shapeType);

/// @brief Collects the semantic-carrier ottavas that touch the given measure and staff.
OttavaShapeMap collectOttavasForMeasureStaff(
    const musx::dom::DocumentPtr& document,
    musx::dom::Cmper partId,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& measure,
    musx::dom::StaffCmper staffId);

/// @brief Returns the octave displacement (sum of applicable carrier ottavas) for a note.
int calcOttavaOctaveAdjustment(
    const OttavaShapeMap& ottavas,
    const musx::dom::NoteInfoPtr& noteInfo,
    const std::function<void(const musx::dom::NoteInfoPtr&)>& onTiedFromOutsideOttava = {});

} // namespace denigma
