/*
 * Copyright (C) 2024, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <cmath>

#include "mnx.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace mnxexp {

/**
 * @brief Provides a default mapping from TextRepeatDef text to a jump type
 *
 * This function only maps strings from the Finale 27 Maestro Default file
 * plus a few other obvious symbols (standard Unicode and SMuFL symbols).
 * Anything else returns JumpType::None. It does a case-insensitive comparison.
 */
/// @param text The text to map. (Usuallu from TextRepeatText)
/// @param glyphName The glyphname, if any.
/// @return 
JumpType convertTextToJump(const std::string& text, const std::optional<std::string>& glyphName)
{
    if (glyphName) {
        if (glyphName == "segno" || glyphName == "segnoSerpent1" || glyphName == "segnoSerpent2" || glyphName == "segnoJapanese") {
            return JumpType::Segno;
        }
        if (glyphName == "dalSegno") {
            return JumpType::DalSegno;
        }
        if (glyphName == "daCapo") {
            return JumpType::DaCapo;
        }
        if (glyphName == "coda" || glyphName == "codaSquare" || glyphName == "codaJapanese") {
            return JumpType::Coda;
        }
    }

    std::string lowerText = utils::toLowerCase(text); // Convert input text to lowercase    

    if (lowerText == "d.c. al fine") {
        return JumpType::DCAlFine;
    }
    if (lowerText == "d.c. al coda") {
        return JumpType::DaCapo;
    }
    if (lowerText == "d.s. al fine") {
        return JumpType::DsAlFine;
    }
    if (lowerText == "d.s. al coda") {
        return JumpType::DalSegno;
    }
    if (lowerText == "to coda #" || lowerText == "coda" || lowerText == "to coda") {
        return JumpType::Coda;
    }
    if (lowerText == "fine") {
        return JumpType::Fine;
    }
    if (lowerText == "§" || lowerText == "𝄋") {
        return JumpType::Segno;
    }
    if (lowerText == "𝄌") {
        return JumpType::Coda;
    }

    return JumpType::None;
}

std::vector<EventMarkingType> calcMarkingType(const MusxInstance<others::ArticulationDef>& artic,
    std::optional<int>& numMarks,
    std::optional<mnx::BreathMarkSymbol>& breathMark)
{
    auto checkSymbol = [&](char32_t sym, const MusxInstance<FontInfo>& fontInfo) -> std::vector<EventMarkingType> {
        // First, check for standard Unicode chars
        switch (sym) {
            case 0x1D167:
            case 0x1D16A:
                numMarks = 1;
                return { EventMarkingType::Tremolo };
            case 0x1D168:
            case 0x1D16B:
                numMarks = 2;
                return { EventMarkingType::Tremolo };
            case 0x1D169:
            case 0x1D16C:
                numMarks = 3;
                return { EventMarkingType::Tremolo };
            case 0x1D17B: return { EventMarkingType::Accent };
            case 0x1D17C: return { EventMarkingType::Staccato };
            case 0x1D17D: return { EventMarkingType::Tenuto };
            case 0x1D17E: return { EventMarkingType::Staccatissimo };
            case 0x1D17F: return { EventMarkingType::StrongAccent };
            default: break;                
        }

        /// @todo Spiccato marking has no SMuFl glyph!
        if (auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
            if (glyphName == "articAccentAbove" || glyphName == "articAccentBelow" || glyphName == "articAccentAboveLegacy") {
                return { EventMarkingType::Accent };
            } else if (glyphName == "articAccentStaccatoAbove" || glyphName == "articAccentStaccatoBelow" ||
                    glyphName == "articAccentStaccatoAboveLegacy" || glyphName == "articAccentStaccatoBelowLegacy") {
                return { EventMarkingType::Accent, EventMarkingType::Staccato };
            } else if (glyphName == "articTenutoAccentAbove" || glyphName == "articTenutoAccentBelow" ||
                    glyphName == "articTenutoAccentAboveLegacy" || glyphName == "articTenutoAccentBelowLegacy") {
                return { EventMarkingType::Accent, EventMarkingType::Tenuto };
            } else if (glyphName == "breathMarkComma" || glyphName == "breathMarkCommaLegacy") {
                breathMark = mnx::BreathMarkSymbol::Comma;
                return { EventMarkingType::Breath };
            } else if (glyphName == "breathMarkTick") {
                breathMark = mnx::BreathMarkSymbol::Tick;
                return { EventMarkingType::Breath };
            } else if (glyphName == "breathMarkUpbow") {
                breathMark = mnx::BreathMarkSymbol::Upbow;
                return { EventMarkingType::Breath };
            } else if (glyphName == "breathMarkSalzedo") {
                breathMark = mnx::BreathMarkSymbol::Salzedo;
                return { EventMarkingType::Breath };
            } else if (glyphName == "articStaccatissimoAbove" || glyphName == "articStaccatissimoBelow") {
                return { EventMarkingType::Spiccato };
            } else if (glyphName == "articStaccatissimoStrokeAbove" || glyphName == "articStaccatissimoStrokeBelow" ||
                    glyphName == "articStaccatissimoWedgeAbove" || glyphName == "articStaccatissimoWedgeBelow") {
                return { EventMarkingType::Staccatissimo };
            } else if (glyphName == "articStaccatoAbove" || glyphName == "articStaccatoBelow") {
                return { EventMarkingType::Staccato };
            } else if (glyphName == "articMarcatoStaccatoAbove" || glyphName == "articMarcatoStaccatoBelow" ||
                    glyphName == "articMarcatoStaccatoAboveLegacy" || glyphName == "articMarcatoStaccatoBelowLegacy") {
                return { EventMarkingType::Staccato, EventMarkingType::StrongAccent };
            } else if (glyphName == "articTenutoStaccatoAbove" || glyphName == "articTenutoStaccatoBelow" ||
                    glyphName == "articTenutoStaccatoAboveLegacy" || glyphName == "articTenutoStaccatoBelowLegacy") {
                return { EventMarkingType::Staccato, EventMarkingType::Tenuto };
            } else if (glyphName == "articStressAbove" || glyphName == "articStressBelow") {
                return { EventMarkingType::Stress };
            } else if (glyphName == "articUnstressAbove" || glyphName == "articUnstressBelow") {
                return { EventMarkingType::Unstress };
            } else if (glyphName == "articMarcatoAbove" || glyphName == "articMarcatoBelow") {
                return { EventMarkingType::StrongAccent };
            } else if (glyphName == "articMarcatoTenutoAbove" || glyphName == "articMarcatoTenutoBelow") {
                return { EventMarkingType::StrongAccent, EventMarkingType::Tenuto };
            } else if (glyphName == "articTenutoAbove" || glyphName == "articTenutoBelow") {
                return { EventMarkingType::Tenuto };
            } else if (glyphName == "stemPendereckiTremolo" || glyphName == "buzzRoll" ||
                    glyphName == "pendereckiTremolo" || glyphName == "unmeasuredTremolo" ||
                    glyphName == "unmeasuredTremoloSimple" || glyphName == "stockhausenTremolo") {
                numMarks = 0;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo1" || glyphName == "tremoloFingered1" || glyphName == "tremolo1Alt") {
                numMarks = 1;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo2" || glyphName == "tremoloFingered2" || glyphName == "tremolo2Alt") {
                numMarks = 2;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo3" || glyphName == "tremoloFingered3" || glyphName == "tremolo3Alt") {
                numMarks = 3;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo4" || glyphName == "tremoloFingered4" || glyphName == "tremolo4Legacy") {
                numMarks = 4;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo5" || glyphName == "tremoloFingered5" || glyphName == "tremolo5Legacy") {
                numMarks = 5;
                return { EventMarkingType::Tremolo };
            }
        }
        return {};
    };
    
    auto checkShape = [&](Cmper shapeId) -> std::vector<EventMarkingType> {
        if (auto shape = artic->getDocument()->getOthers()->get<others::ShapeDef>(artic->getPartId(), shapeId)) {
            if (auto knownShape = shape->recognize()) {
                switch (knownShape.value()) {
                    case KnownShapeDefType::TenutoMark: return { EventMarkingType::Tenuto };
                    default: break;
                }
            }
        }
        return {};
    };

    auto main = artic->mainIsShape
              ? checkShape(artic->mainShape)
              : checkSymbol(artic->charMain, artic->fontMain);
    if (!main.empty()) {
        return main;
    }

    auto alt = artic->altIsShape
             ? checkShape(artic->altShape)
             : checkSymbol(artic->charAlt, artic->fontAlt);

    return alt;
}

mnx::NoteValue::Initializer mnxNoteValueFromEdu(Edu duration)
{
    auto [base, dots] = calcNoteInfoFromEdu(duration);
    return mnx::NoteValue::Initializer(enumConvert<mnx::NoteValueBase>(base), dots);
}

std::pair<int, mnx::NoteValue::Initializer> mnxNoteValueQuantityFromFraction(const MnxMusxMappingPtr& context, musx::util::Fraction duration)
{
    if (duration <= 0 || (duration.denominator() & (duration.denominator() - 1)) != 0) {
        auto newValue = musx::util::Fraction(duration.calcEduDuration(), Edu(musx::dom::NoteType::Whole));
        context->logMessage(LogMsg() << "Value " << duration << " cannot be exactly converted to a note value quantity. Using closest approximation. ("
            << newValue << ")", LogSeverity::Warning);
        duration = newValue;
    }

    return std::make_pair(duration.numerator(), mnxNoteValueFromEdu(musx::util::Fraction(1, duration.denominator()).calcEduDuration()));
}

mnx::Fraction::Initializer mnxFractionFromFraction(const musx::util::Fraction& fraction)
{
    return mnx::Fraction::Initializer(fraction.numerator(), fraction.denominator());
}

mnx::Fraction::Initializer mnxFractionFromEdu(Edu eduValue)
{
    return mnxFractionFromFraction(musx::util::Fraction::fromEdu(eduValue));
}

mnx::Fraction::Initializer mnxFractionFromSmartShapeEndPoint(const MusxInstance<smartshape::EndPoint>& endPoint)
{
    if (auto entryInfo = endPoint->calcAssociatedEntry(SCORE_PARTID)) {
        return mnxFractionFromFraction(entryInfo->elapsedDuration);
    }
    return mnxFractionFromFraction(endPoint->calcPosition()); 
}

int mnxStaffPosition(const MusxInstance<others::Staff>& staff, int musxStaffPosition)
{
    return musxStaffPosition - staff->calcMiddleStaffPosition();
}

mnx::LyricLineType mnxLineTypeFromLyric(const MusxInstance<LyricsSyllableInfo>& syl)
{
    if (syl->hasHyphenBefore && syl->hasHyphenAfter) {
        return mnx::LyricLineType::Middle;
    } else if (syl->hasHyphenBefore && !syl->hasHyphenAfter) {
        return mnx::LyricLineType::End;
    } else if (!syl->hasHyphenBefore && syl->hasHyphenAfter) {
        return mnx::LyricLineType::Start;
    }
    return mnx::LyricLineType::Whole;
}

std::optional<std::tuple<mnx::ClefSign, mnx::OttavaAmountOrZero, bool>> mnxClefInfoFromClefDef(
    const MusxInstance<options::ClefOptions::ClefDef>& clefDef,
    const MusxInstance<others::Staff>& staff, std::optional<std::string_view> glyphName)
{
    if (clefDef->isBlank()) {
        /// @todo handle blank clefs
        return std::nullopt;
    }
    auto [musxClefType, octave] = clefDef->calcInfo(staff);
    if (std::abs(octave) > 3) {
        return std::nullopt;
    }
    std::optional<mnx::ClefSign> clefSign;
    switch (musxClefType) {
    case music_theory::ClefType::G: clefSign = mnx::ClefSign::GClef; break;
        case music_theory::ClefType::C: clefSign = mnx::ClefSign::CClef; break;
        case music_theory::ClefType::F: clefSign = mnx::ClefSign::FClef; break;
        /// @todo handle Percussion and Tab cases when defined in mnx spec
        default: break;
    }
    if (!clefSign) {
        return std::nullopt;
    }
    bool hideOctave = false;
    if (octave != 0) {
        /// @todo rewrite these switches using glyph names instead of code points. Requires mappings for pre-SMuFL symbol fonts.
        switch (clefSign.value()) {
        case mnx::ClefSign::GClef:
        {
            if (clefDef->clefChar == 0x1D11E || glyphName == "gClef" || glyphName == "gClefSmall") { // Unicode G clef or known SMuFL G clef
                hideOctave = true;
            }
            break;
        }
        case mnx::ClefSign::CClef:
        {
            if (clefDef->clefChar == 0x1D121 || glyphName == "cClef" || glyphName == "cClefSquare" || glyphName == "cClefFrench" || glyphName == "cClefFrench20C") {
                hideOctave = true;
            }
            break;
        }
        case mnx::ClefSign::FClef:
        {
            if (clefDef->clefChar == 0x1D122 || glyphName == "fClef" || glyphName == "fClefFrench" || glyphName == "fClef19thCentury") {
                hideOctave = true;
            }
            break;
        }
        default:
            break;
        }
    }
    return std::make_tuple(clefSign.value(), mnx::OttavaAmountOrZero(octave), hideOctave);
}

} // namespace mnxexp
} // namespace denigma
