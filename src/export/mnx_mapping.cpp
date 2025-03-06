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
#include "mnx_mapping.h"

namespace denigma {
namespace mnxexp {

FontType convertFontToType(const std::shared_ptr<FontInfo>& fontInfo)
{
    if (fontInfo->calcIsSMuFL()) {
        return FontType::SMuFL;
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

std::optional<std::pair<mnx::ClefSign, int>> convertCharToClef(const char32_t sym, FontType fontType)
{
    // Standard Unicode musical symbols:
    switch (sym) {
        case 0x1D11E: return std::make_pair(mnx::ClefSign::GClef, 0); // MUSICAL SYMBOL G CLEF
        case 0x1D122: return std::make_pair(mnx::ClefSign::FClef, 0); // MUSICAL SYMBOL F CLEF
        case 0x1D121: return std::make_pair(mnx::ClefSign::CClef, 0); // MUSICAL SYMBOL C CLEF
    }
    if (fontType == FontType::Symbol) {
        switch (sym) {
            case '?': return std::make_pair(mnx::ClefSign::FClef, 0);
            case 't': return std::make_pair(mnx::ClefSign::FClef, -1);
            case 0xe6: return std::make_pair(mnx::ClefSign::FClef, 1);
            case 'B': return std::make_pair(mnx::ClefSign::CClef, 0);
            case '&': return std::make_pair(mnx::ClefSign::GClef, 0);
            case 'V': return std::make_pair(mnx::ClefSign::GClef, -1);
            case 0xa0:return std::make_pair(mnx::ClefSign::GClef, 1);
            default: return std::nullopt;
        }
    }
    // SMuFL branch: only include clefs with unison or octave multiples
    if (fontType == FontType::SMuFL) {
        switch (sym) {
                // G Clef:
            case 0xE050: // Standard G clef (unison)
            case 0xF472: // Alternate G clef (unison)
                return std::make_pair(mnx::ClefSign::GClef, 0);
            case 0xE052: // G clef 8vb (one octave down)
            case 0xE055: // G clef 8vb (one octave down)
            case 0xE056: // G clef 8vb (one octave down)
            case 0xE057: // G clef 8vb (one octave down)
            case 0xE05B: // G clef 8vb (one octave down)
                return std::make_pair(mnx::ClefSign::GClef, -1);
            case 0xE053: // G clef 8va (one octave up)
            case 0xE05A: // G clef 8va (one octave up)
                return std::make_pair(mnx::ClefSign::GClef, 1);
            case 0xE051: // G clef 15mb (two octaves down)
                return std::make_pair(mnx::ClefSign::GClef, -2);
            case 0xE054: // G clef 15ma (two octaves up)
                return std::make_pair(mnx::ClefSign::GClef, 2);

                // F Clef:
            case 0xE062: // Standard F clef (unison)
            case 0xF406: // Alternate F clef (unison)
            case 0xF407: // Alternate F clef (unison)
            case 0xF474: // Alternate F clef (unison)
                return std::make_pair(mnx::ClefSign::FClef, 0);
            case 0xE064: // F clef 8vb (one octave down)
            case 0xE068: // F clef 8vb (one octave down)
                return std::make_pair(mnx::ClefSign::FClef, -1);
            case 0xE065: // F clef 8va (one octave up)
            case 0xE067: // F clef 8va (one octave up)
                return std::make_pair(mnx::ClefSign::FClef, 1);
            case 0xE063: // F clef 15mb (two octaves down)
                return std::make_pair(mnx::ClefSign::FClef, -2);
            case 0xE066: // F clef 15ma (two octaves up)
                return std::make_pair(mnx::ClefSign::FClef, 2);

                // C Clef:
            case 0xE05C: // Standard C clef (unison)
            case 0xE060: // Alternate C clef (unison)
            case 0xF408: // Alternate C clef (unison)
            case 0xF473: // Alternate C clef (unison)
                return std::make_pair(mnx::ClefSign::CClef, 0);
            case 0xE05D: // C clef 8vb (one octave down)
            case 0xE05F: // C clef 8vb (one octave down)
                return std::make_pair(mnx::ClefSign::CClef, -1);
            case 0xE05E: // C clef 8va (one octave up)
                return std::make_pair(mnx::ClefSign::CClef, 1);
        }
    }
    return std::nullopt;
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

mnx::Fraction::Initializer mnxFractionFromFraction(musx::util::Fraction fraction)
{
    return mnx::Fraction::Initializer(fraction.numerator(), fraction.denominator());
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

} // namespace mnxexp
} // namespace denigma
