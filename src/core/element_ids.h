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

#include <string>

#include "musx/musx.h"

namespace denigma {
namespace core {

/// @brief Computes a stable id for an event (an entry, or the full-measure rest standing in for it).
/// Shared by every exporter that needs to point at an event, so the same musx entry always yields
/// the same id regardless of target format.
std::string calcEventId(musx::dom::EntryNumber entryNum);

/// @brief Computes a stable id for one note within an entry.
std::string calcNoteId(const musx::dom::NoteInfoPtr& noteInfo);

/// @brief Computes a stable id for a measure, independent of any part.
std::string calcGlobalMeasureId(musx::dom::Cmper cmperValue);

/// @brief Computes a stable id for one part's instance of a measure.
///
/// A bare #calcGlobalMeasureId value is not unique across parts sharing one document-wide id
/// namespace (MNX's `Object::id` and MusicXML's `xs:ID`-typed id attributes both require
/// document-wide uniqueness, and both formats can hold every part's measures in one document), so
/// a part-measure id is the measure id prefixed with its owning part's id.
std::string calcPartMeasureId(const std::string& partId, musx::dom::Cmper cmperValue);

} // namespace core
} // namespace denigma
