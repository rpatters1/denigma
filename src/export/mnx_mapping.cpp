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

namespace denigma {
namespace mnxexp {

FontType convertFontToType(const std::shared_ptr<const FontInfo>& fontInfo)
{
    if (fontInfo->calcIsSMuFL()) {
        return FontType::SMuFL;
    } else if (fontInfo->getName() == "GraceNotes") {
        return FontType::GraceNotes;
    } else if (fontInfo->calcIsDefaultMusic() || fontInfo->calcIsSymbolFont()) {
        return FontType::Symbol;
    }
    return FontType::Unicode;
}

/**
 * @brief Provides a default mapping from TextRepeatDef text to a jump type
 *
 * This function only maps strings from the Finale 27 Maestro Default file
 * plus a few other obvious symbols (standard Unicode and SMuFL symbols).
 * Anything else returns JumpType::None. It does a case-insensitive comparison.
 */
/// @param text The text to map. (Usuallu from TextRepeatText)
/// @param fontType The font type (Unicode/Ascii, SMuFL, or Symbol)
/// @return 
JumpType convertTextToJump(const std::string& text, FontType fontType)
{
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

    if (fontType == FontType::Unicode) {
        if (lowerText == "§" || lowerText == "𝄋") {
            return JumpType::Segno;
        }
        if (lowerText == "𝄌") {
            return JumpType::Coda;
        }
    }

    if (fontType == FontType::Symbol) {
        if (lowerText == "%" || lowerText == u8"\u009F") {
            return JumpType::Segno;
        }
        if (lowerText == u8"\u00DE") {
            return JumpType::Coda;
        }
    }

    if (fontType == FontType::SMuFL) {
        if (lowerText == u8"\uE047" || lowerText == u8"\uE04A" || lowerText == u8"\uE04B" || lowerText == u8"\uF404") {
            return JumpType::Segno;
        }
        if (lowerText == u8"\uE045") {
            return JumpType::DalSegno;
        }
        if (lowerText == u8"\uE046") {
            return JumpType::DaCapo;
        }
        if (lowerText == u8"\uE048" || lowerText == u8"\uE049" || lowerText == u8"\uF405") {
            return JumpType::Coda;
        }
    }

    return JumpType::None;
}

std::vector<EventMarkingType> calcMarkingType(const std::shared_ptr<const others::ArticulationDef>& artic,
    std::optional<int>& numMarks,
    std::optional<mnx::BreathMarkSymbol>& breathMark)
{
    auto checkSymbol = [&](char32_t sym, FontType fontType) -> std::vector<EventMarkingType> {
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
        }
        switch (fontType) {
            case FontType::Unicode: // this also includes ASCII
                switch (sym) {
                    case ',':
                        breathMark = mnx::BreathMarkSymbol::Comma;
                        return { EventMarkingType::Breath };
                    case '>': return { EventMarkingType::Accent };
                    case '.': return { EventMarkingType::Staccato };
                    case '-':
                    case 0x2013: // en-dash
                    case 0x2014: // em-dash
                        return { EventMarkingType::Tenuto };
                }
                break;
            case FontType::GraceNotes:
                // GraceNotes currently only supported for tremolo symbols
                switch (sym) {
                    case 124:
                        numMarks = 1;
                        return { EventMarkingType::Tremolo };
                    case 199:
                        numMarks = 2;
                        return { EventMarkingType::Tremolo };
                    case 200:
                        numMarks = 3;
                        return { EventMarkingType::Tremolo };
                    case 230:
                        numMarks = 3;
                        return { EventMarkingType::Tremolo };
                }
                break;
            case FontType::Symbol:
                switch (sym) {
                    case 33:
                        numMarks = 1;
                        return { EventMarkingType::Tremolo };
                    case 39: return { EventMarkingType::Spiccato };
                    case 44:
                        breathMark = mnx::BreathMarkSymbol::Comma;
                        return { EventMarkingType::Breath };
                    case 45: return { EventMarkingType::Tenuto };
                    case 46: return { EventMarkingType::Staccato };
                    case 60: return { EventMarkingType::Staccato, EventMarkingType::Tenuto };
                    case 62: return { EventMarkingType::Accent };
                    case 64:
                        numMarks = 2;
                        return { EventMarkingType::Tremolo };
                    case 94: return { EventMarkingType::StrongAccent };
                    case 107: return { EventMarkingType::Staccato };
                    case 118: return { EventMarkingType::StrongAccent };
                    case 133:
                        breathMark = mnx::BreathMarkSymbol::Tick;
                        return { EventMarkingType::Breath }; // v-slash breath
                    case 137:
                    case 138:
                        return { EventMarkingType::Accent, EventMarkingType::Tenuto };
                    case 171: return { EventMarkingType::Staccatissimo };
                    case 172: return { EventMarkingType::StrongAccent, EventMarkingType::Staccato };
                    case 174: return { EventMarkingType::Spiccato };
                    case 190:
                        numMarks = 3;
                        return { EventMarkingType::Tremolo };
                    case 216: return { EventMarkingType::Staccatissimo };
                    case 223: return { EventMarkingType::Accent, EventMarkingType::Staccato };
                    case 232: return { EventMarkingType::StrongAccent, EventMarkingType::Staccato };
                    case 248: return { EventMarkingType::Staccato, EventMarkingType::Tenuto };
                    case 249: return { EventMarkingType::Accent, EventMarkingType::Staccato };
                }
                break;
            case FontType::SMuFL:
                switch (sym) {
                    // accents
                    case 0xE4A0:
                    case 0xE4A1:
                    case 0xF632:
                        return { EventMarkingType::Accent };
                    case 0xE4B0:
                    case 0xE4B1:
                    case 0xF62B:
                    case 0xF62C:
                        return { EventMarkingType::Accent, EventMarkingType::Staccato };
                    case 0xE4B4:
                    case 0xE4B5:
                    case 0xF630:
                    case 0xF631:
                        return { EventMarkingType::Accent, EventMarkingType::Tenuto };
                    // breaths
                    case 0xE4CE:
                    case 0xF635:
                        breathMark = mnx::BreathMarkSymbol::Comma;
                        return { EventMarkingType::Breath };
                    case 0xE4CF:
                        breathMark = mnx::BreathMarkSymbol::Tick;
                        return { EventMarkingType::Breath };
                    case 0xE4D0:
                        breathMark = mnx::BreathMarkSymbol::Upbow;
                        return { EventMarkingType::Breath };
                    case 0xE4D5:
                        breathMark = mnx::BreathMarkSymbol::Salzedo;
                        return { EventMarkingType::Breath };
                    // soft accents
                    // spiccato, staccatissimo, staccato
                    case 0xE486:
                    case 0xE487:
                    case 0xE4AA:
                    case 0xE4AB:
                        return { EventMarkingType::Spiccato };
                    case 0xE4A8:
                    case 0xE4A9:
                        return { EventMarkingType::Staccatissimo };
                    case 0xE4A2:
                    case 0xE4A3:
                        return { EventMarkingType::Staccato };
                    case 0xE4AE:
                    case 0xE4AF:
                    case 0xF62D:
                    case 0xF62E:
                        return { EventMarkingType::Staccato, EventMarkingType::StrongAccent };
                    case 0xE4B2:
                    case 0xE4B3:
                    case 0xF62F:
                    case 0xF636:
                        return { EventMarkingType::Staccato, EventMarkingType::Tenuto };
                    // stress/unstress
                    case 0xE4B6:
                    case 0xE4B7:
                        return { EventMarkingType::Stress };
                    case 0xE4B8:
                    case 0xE4B9:
                        return { EventMarkingType::Unstress };
                    // strong accents (marcato)
                    case 0xE4AC:
                    case 0xE4AD:
                        return { EventMarkingType::StrongAccent };
                    case 0xE4BC:
                    case 0xE4BD:
                        return { EventMarkingType::StrongAccent, EventMarkingType::Tenuto };
                    // tenuto
                    case 0xE4A4:
                    case 0xE4A5:
                        return { EventMarkingType::Tenuto };
                    // tremolo
                    case 0xE213:
                    case 0xE22A:
                    case 0xE22B:
                    case 0xE22C:
                    case 0xE22D:
                    case 0xE232:
                        numMarks = 0;
                        return { EventMarkingType::Tremolo };
                    case 0xE220:
                    case 0xE225:
                    case 0xF680:
                        numMarks = 1;
                        return { EventMarkingType::Tremolo };
                    case 0xE221:
                    case 0xE226:
                    case 0xF681:
                        numMarks = 2;
                        return { EventMarkingType::Tremolo };
                    case 0xE222:
                    case 0xE227:
                    case 0xF682:
                        numMarks = 3;
                        return { EventMarkingType::Tremolo };
                    case 0xE223:
                    case 0xE228:
                    case 0xF683:
                        numMarks = 4;
                        return { EventMarkingType::Tremolo };
                    case 0xE224:
                    case 0xE229:
                    case 0xF684:
                        numMarks = 5;
                        return { EventMarkingType::Tremolo };
                }
                break;
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
              : checkSymbol(artic->charMain, convertFontToType(artic->fontMain));
    if (!main.empty()) {
        return main;
    }

    auto alt = artic->altIsShape
             ? checkShape(artic->altShape)
             : checkSymbol(artic->charAlt, convertFontToType(artic->fontAlt));

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

mnx::Fraction::Initializer mnxFractionFromSmartShapeEndPoint(const std::shared_ptr<const others::SmartShape::EndPoint>& endPoint)
{
    if (auto entryInfo = endPoint->calcAssociatedEntry(SCORE_PARTID)) {
        return mnxFractionFromFraction(entryInfo->elapsedDuration);
    }
    return mnxFractionFromFraction(endPoint->calcPosition()); 
}

int mnxStaffPosition(const std::shared_ptr<const others::Staff>& staff, int musxStaffPosition)
{
    return musxStaffPosition - staff->calcMiddleStaffPosition();
}

mnx::LyricLineType mnxLineTypeFromLyric(const std::shared_ptr<const LyricsSyllableInfo>& syl)
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
    const std::shared_ptr<const options::ClefOptions::ClefDef>& clefDef,
    const std::shared_ptr<const others::Staff>& staff, FontType fontType)
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
            switch (clefDef->clefChar) {
            case 0x1D11E: // Unicode G clef
                hideOctave = true;
                break;
            case '&': // Symbol G Clef
                hideOctave = fontType == FontType::Symbol || fontType == FontType::GraceNotes;
                break;
            case 0xE050: // SMuFL Standard G clef (unison)
            case 0xF472: // SMuFL Alternate G clef (unison)
                hideOctave = fontType == FontType::SMuFL;
                break;
            }
            break;
        }
        case mnx::ClefSign::CClef:
        {
            switch (clefDef->clefChar) {
            case 0x1D121: // Unicode C clef
                hideOctave = true;
                break;
            case 'B': // Symbol C Clef
                hideOctave = fontType == FontType::Symbol || fontType == FontType::GraceNotes;
                break;
            case 0xE05C: // SMuFL Standard C clef (unison)
            case 0xE060: // SMuFL Alternate C clef (unison)
            case 0xF408: // SMuFL Alternate C clef (unison)
            case 0xF473: // SMuFL Alternate C clef (unison)
                hideOctave = fontType == FontType::SMuFL;
                break;
            }
            break;
        }
        case mnx::ClefSign::FClef:
        {
            switch (clefDef->clefChar) {
            case 0x1D122: // Unicode F clef
                hideOctave = true;
                break;
            case '?': // Symbol F Clef
                hideOctave = fontType == FontType::Symbol || fontType == FontType::GraceNotes;
                break;
            case 0xE062: // SMuFL Standard F clef (unison)
            case 0xF406: // SMuFL Alternate F clef (unison)
            case 0xF407: // SMuFL Alternate F clef (unison)
            case 0xF474: // SMuFL Alternate F clef (unison)
                hideOctave = fontType == FontType::SMuFL;
                break;
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
