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
#include "utils/textmetrics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "denigma.h"
#include "utils/stringutils.h"

#if defined(DENIGMA_USE_DIRECTWRITE)
#include <windows.h>
#include <dwrite.h>
#endif

#if defined(DENIGMA_USE_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#if defined(DENIGMA_USE_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

#if defined(DENIGMA_USE_CORETEXT)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#endif

namespace denigma {
namespace textmetrics {
namespace {

[[maybe_unused]] void warnMissingBackend(const DenigmaContext& denigmaContext)
{
    static std::once_flag warningOnce;
    std::call_once(warningOnce, [&denigmaContext]() {
        denigmaContext.logMessage(LogMsg() << "FreeType text metrics backend is not enabled in this build. Falling back to heuristic text metrics.",
                                  LogSeverity::Warning);
    });
}

std::string normalizeName(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (unsigned char ch : input) {
        if (std::isalnum(ch)) {
            result.push_back(static_cast<char>(std::tolower(ch)));
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            continue;
        }
    }
    return result;
}

bool styleLooksBold(std::string_view style)
{
    const std::string normalized = normalizeName(style);
    return normalized.find("bold") != std::string::npos
        || normalized.find("demi") != std::string::npos
        || normalized.find("black") != std::string::npos;
}

bool styleLooksItalic(std::string_view style)
{
    const std::string normalized = normalizeName(style);
    return normalized.find("italic") != std::string::npos
        || normalized.find("oblique") != std::string::npos;
}

#if defined(DENIGMA_USE_FREETYPE)

constexpr double EVPU_PER_POINT = musx::dom::EVPU_PER_POINT;

struct ResolvedFace
{
    std::string filePath;
    int faceIndex{};
};

struct FaceKey
{
    std::string filePath;
    int faceIndex{};

    bool operator==(const FaceKey& other) const
    {
        return faceIndex == other.faceIndex && filePath == other.filePath;
    }
};

struct FaceKeyHash
{
    std::size_t operator()(const FaceKey& value) const
    {
        return std::hash<std::string>()(value.filePath) ^ (std::hash<int>()(value.faceIndex) << 1);
    }
};

struct IndexedFace
{
    ResolvedFace resolved;
    std::string familyNormalized;
    bool bold{};
    bool italic{};
};

class FreeTypeTextMetricsBackend
{
public:
    FreeTypeTextMetricsBackend()
    {
        if (FT_Init_FreeType(&m_library) == 0) {
            m_initialized = true;
        }
#if defined(DENIGMA_USE_FONTCONFIG)
        m_fontconfigAvailable = FcInit() != FcFalse;
#endif
    }

    ~FreeTypeTextMetricsBackend()
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        for (auto& it : m_faces) {
            if (it.second) {
                FT_Done_Face(it.second);
            }
        }
        if (m_library) {
            FT_Done_FreeType(m_library);
        }
    }

    std::optional<TextMetricsEvpu> measureText(const musx::dom::FontInfo& fontInfo,
                                               std::u32string_view text,
                                               std::optional<double> pointSizeOverride,
                                               const DenigmaContext& denigmaContext)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        const double pointSize = pointSizeOverride.value_or(static_cast<double>(fontInfo.fontSize));
        auto face = resolveFaceLocked(fontInfo,
                                      pointSize,
                                      denigmaContext);
        if (!face) {
            return std::nullopt;
        }

        TextMetricsEvpu result;
        bool hasMeasuredGlyphBounds = false;
        bool loadedAnyGlyph = false;
        double penXEvpu = 0.0;
        double boundsMinXEvpu = 0.0;
        double boundsMaxXEvpu = 0.0;
        double boundsMinYEvpu = 0.0;
        double boundsMaxYEvpu = 0.0;
        constexpr FT_Int32 glyphLoadFlags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP;

        FT_UInt previousGlyph = 0;
        const bool hasKerning = FT_HAS_KERNING(*face);
        for (char32_t codePoint : text) {
            if (codePoint == U'\n' || codePoint == U'\r') {
                previousGlyph = 0;
                continue;
            }
            const FT_UInt glyphIndex = FT_Get_Char_Index(*face, static_cast<FT_ULong>(codePoint));
            if (hasKerning && previousGlyph && glyphIndex) {
                FT_Vector kerning{};
                if (FT_Get_Kerning(*face, previousGlyph, glyphIndex, FT_KERNING_UNFITTED, &kerning) == 0) {
                    penXEvpu += (static_cast<double>(kerning.x) / 64.0) * EVPU_PER_POINT;
                }
            }
            if (FT_Load_Glyph(*face, glyphIndex, glyphLoadFlags) == 0) {
                loadedAnyGlyph = true;
                const auto& glyphMetrics = (*face)->glyph->metrics;
                const double glyphMinX = penXEvpu + (static_cast<double>(glyphMetrics.horiBearingX) / 64.0) * EVPU_PER_POINT;
                const double glyphMaxX = glyphMinX + (static_cast<double>(glyphMetrics.width) / 64.0) * EVPU_PER_POINT;
                const double glyphMaxY = (static_cast<double>(glyphMetrics.horiBearingY) / 64.0) * EVPU_PER_POINT;
                const double glyphMinY = glyphMaxY - (static_cast<double>(glyphMetrics.height) / 64.0) * EVPU_PER_POINT;
                if (glyphMetrics.width > 0 || glyphMetrics.height > 0) {
                    if (!hasMeasuredGlyphBounds) {
                        boundsMinXEvpu = glyphMinX;
                        boundsMaxXEvpu = glyphMaxX;
                        boundsMinYEvpu = glyphMinY;
                        boundsMaxYEvpu = glyphMaxY;
                        hasMeasuredGlyphBounds = true;
                    } else {
                        boundsMinXEvpu = (std::min)(boundsMinXEvpu, glyphMinX);
                        boundsMaxXEvpu = (std::max)(boundsMaxXEvpu, glyphMaxX);
                        boundsMinYEvpu = (std::min)(boundsMinYEvpu, glyphMinY);
                        boundsMaxYEvpu = (std::max)(boundsMaxYEvpu, glyphMaxY);
                    }
                }
                penXEvpu += (static_cast<double>((*face)->glyph->linearHoriAdvance) / 65536.0) * EVPU_PER_POINT;
            }
            previousGlyph = glyphIndex;
        }

        if (hasMeasuredGlyphBounds) {
            result.advance = (std::max)(0.0, penXEvpu);
            result.ascent = (std::max)(0.0, boundsMaxYEvpu);
            result.descent = (std::max)(0.0, -boundsMinYEvpu);
        } else if (loadedAnyGlyph) {
            // No glyph ink box was produced (e.g. whitespace-only text); preserve logical advance.
            result.advance = (std::max)(0.0, penXEvpu);
        } else if (!text.empty()) {
            // Fallback only when glyph loading failed for the whole run.
            const auto vertical = calcFaceVerticalMetricsEvpu(*face, pointSize);
            result.ascent = vertical.ascent;
            result.descent = vertical.descent;
        }

        return result;
    }

    std::optional<double> measureGlyphWidth(const musx::dom::FontInfo& fontInfo,
                                            char32_t codePoint,
                                            std::optional<double> pointSizeOverride,
                                            const DenigmaContext& denigmaContext)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        auto face = resolveFaceLocked(fontInfo,
                                      pointSizeOverride.value_or(static_cast<double>(fontInfo.fontSize)),
                                      denigmaContext);
        if (!face) {
            return std::nullopt;
        }

        const FT_UInt glyphIndex = FT_Get_Char_Index(*face, static_cast<FT_ULong>(codePoint));
        if (!glyphIndex) {
            return std::nullopt;
        }
        if (FT_Load_Glyph(*face, glyphIndex, FT_LOAD_DEFAULT) != 0) {
            return std::nullopt;
        }
        return (std::max)(0.0, static_cast<double>((*face)->glyph->metrics.width) / 64.0 * EVPU_PER_POINT);
    }

    std::optional<double> measureHeight(const musx::dom::FontInfo& fontInfo,
                                        double pointSize,
                                        const DenigmaContext& denigmaContext)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        auto face = resolveFaceLocked(fontInfo, pointSize, denigmaContext);
        if (!face) {
            return std::nullopt;
        }
        const auto vertical = calcFaceVerticalMetricsEvpu(*face, pointSize);
        return vertical.ascent + vertical.descent;
    }

    std::optional<TextMetricsEvpu> measureAscentDescent(const musx::dom::FontInfo& fontInfo,
                                                        std::optional<double> pointSizeOverride,
                                                        const DenigmaContext& denigmaContext)
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        const double pointSize = pointSizeOverride.value_or(static_cast<double>(fontInfo.fontSize));
        auto face = resolveFaceLocked(fontInfo,
                                      pointSize,
                                      denigmaContext);
        if (!face) {
            return std::nullopt;
        }

        return calcFaceVerticalMetricsEvpu(*face, pointSize);
    }

private:
    static TextMetricsEvpu calcFaceVerticalMetricsEvpu(FT_Face face, double pointSize)
    {
        TextMetricsEvpu result;
        if (!face || !face->size) {
            return result;
        }

        // Use design-space metrics from font units to avoid pixel-grid rounding.
        if (FT_IS_SCALABLE(face) && face->units_per_EM > 0) {
            const double effectivePointSize = pointSize > 0.0 ? pointSize : 12.0;
            const double emUnits = static_cast<double>(face->units_per_EM);
            result.ascent = (std::max)(0.0, (static_cast<double>(face->ascender) / emUnits) * effectivePointSize * EVPU_PER_POINT);
            result.descent = (std::max)(0.0, (-static_cast<double>(face->descender) / emUnits) * effectivePointSize * EVPU_PER_POINT);
            if (result.ascent > 0.0 || result.descent > 0.0) {
                return result;
            }
        }

        // Fallback path for non-scalable fonts or unexpected empty design metrics.
        result.ascent = (std::max)(0.0, static_cast<double>(face->size->metrics.ascender) / 64.0 * EVPU_PER_POINT);
        result.descent = (std::max)(0.0, -static_cast<double>(face->size->metrics.descender) / 64.0 * EVPU_PER_POINT);
        return result;
    }

    template <typename T>
    static void safeRelease(T*& value)
    {
        if (value) {
            value->Release();
            value = nullptr;
        }
    }

    void warnBackendUnavailableLocked(const DenigmaContext& denigmaContext)
    {
        if (m_warnedBackendUnavailable) {
            return;
        }
        m_warnedBackendUnavailable = true;
        denigmaContext.logMessage(LogMsg() << "Unable to initialize FreeType text metrics backend. Falling back to heuristic text metrics.",
                                  LogSeverity::Warning);
    }

    void warnUnresolvedFamilyLocked(const DenigmaContext& denigmaContext, const std::string& familyName)
    {
        const std::string key = familyName.empty() ? std::string("<unknown font>") : familyName;
        if (!m_warnedUnresolvedFamilies.insert(key).second) {
            return;
        }
        denigmaContext.logMessage(LogMsg() << "Unable to resolve font \"" << key
                                           << "\" for FreeType metrics. Falling back to heuristic text metrics.",
                                  LogSeverity::Warning);
    }

    static std::vector<std::filesystem::path> candidateFontDirectories()
    {
        std::vector<std::filesystem::path> dirs;
        auto append = [&](const std::filesystem::path& path) {
            std::error_code ec;
            if (!path.empty() && std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec)) {
                dirs.push_back(path);
            }
        };

#if defined(MUSX_RUNNING_ON_MACOS)
        append("/System/Library/Fonts");
        append("/Library/Fonts");
        if (const auto home = utils::getEnvironmentValue("HOME")) {
            append(std::filesystem::path(*home) / "Library/Fonts");
        }
#elif defined(MUSX_RUNNING_ON_WINDOWS)
        if (const auto windir = utils::getEnvironmentValue("WINDIR")) {
            append(std::filesystem::path(*windir) / "Fonts");
        }
        if (const auto localAppData = utils::getEnvironmentValue("LOCALAPPDATA")) {
            append(std::filesystem::path(*localAppData) / "Microsoft/Windows/Fonts");
        }
#elif defined(MUSX_RUNNING_ON_LINUX_UNIX)
        append("/usr/share/fonts");
        append("/usr/local/share/fonts");
        if (const auto home = utils::getEnvironmentValue("HOME")) {
            append(std::filesystem::path(*home) / ".local/share/fonts");
            append(std::filesystem::path(*home) / ".fonts");
        }
#endif

        std::vector<std::filesystem::path> deduped;
        std::set<std::string> seen;
        for (const auto& path : dirs) {
            const std::string key = path.lexically_normal().u8string();
            if (seen.insert(key).second) {
                deduped.push_back(path);
            }
        }
        return deduped;
    }

    static bool hasSupportedExtension(const std::filesystem::path& filePath)
    {
        std::string ext = filePath.extension().u8string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".otc" || ext == ".pfa" || ext == ".pfb";
    }

    void indexFontFaceLocked(const std::filesystem::path& filePath, long faceIndex)
    {
        FT_Face face = nullptr;
        if (FT_New_Face(m_library, filePath.u8string().c_str(), faceIndex, &face) != 0 || !face) {
            return;
        }

        std::string family = face->family_name ? std::string(face->family_name) : filePath.stem().u8string();
        std::string style = face->style_name ? std::string(face->style_name) : std::string{};
        const bool bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0 || styleLooksBold(style);
        const bool italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0 || styleLooksItalic(style);

        m_faceIndex.push_back(IndexedFace{
            ResolvedFace{ filePath.u8string(), static_cast<int>(faceIndex) },
            normalizeName(family),
            bold,
            italic
        });

        FT_Done_Face(face);
    }

    void indexFontFileLocked(const std::filesystem::path& filePath)
    {
        if (!hasSupportedExtension(filePath)) {
            return;
        }

        FT_Face probeFace = nullptr;
        if (FT_New_Face(m_library, filePath.u8string().c_str(), 0, &probeFace) != 0 || !probeFace) {
            return;
        }
        const long numFaces = (std::max)(1L, static_cast<long>(probeFace->num_faces));
        FT_Done_Face(probeFace);

        for (long faceIndex = 0; faceIndex < numFaces; ++faceIndex) {
            indexFontFaceLocked(filePath, faceIndex);
        }
    }

    void ensureIndexBuiltLocked()
    {
        if (m_indexBuilt) {
            return;
        }
        m_indexBuilt = true;
        const auto dirs = candidateFontDirectories();
        for (const auto& dir : dirs) {
            std::error_code ec;
            std::filesystem::recursive_directory_iterator it(dir, ec);
            if (ec) {
                continue;
            }
            for (const auto& entry : it) {
                if (entry.is_regular_file(ec)) {
                    indexFontFileLocked(entry.path());
                }
            }
        }
    }

#if defined(DENIGMA_USE_FONTCONFIG)
    std::optional<ResolvedFace> resolveWithFontconfigLocked(const std::string& familyName,
                                                            bool bold,
                                                            bool italic) const
    {
        if (!m_fontconfigAvailable) {
            return std::nullopt;
        }
        FcPattern* pattern = FcPatternCreate();
        if (!pattern) {
            return std::nullopt;
        }
        FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(familyName.c_str()));
        FcPatternAddInteger(pattern, FC_WEIGHT, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
        FcPatternAddInteger(pattern, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
        FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);
        FcResult result = FcResultNoMatch;
        FcPattern* matched = FcFontMatch(nullptr, pattern, &result);
        FcPatternDestroy(pattern);
        if (!matched) {
            return std::nullopt;
        }
        FcChar8* filePath = nullptr;
        if (FcPatternGetString(matched, FC_FILE, 0, &filePath) != FcResultMatch || !filePath) {
            FcPatternDestroy(matched);
            return std::nullopt;
        }
        int faceIndex = 0;
        (void)FcPatternGetInteger(matched, FC_INDEX, 0, &faceIndex);
        ResolvedFace resolved{ reinterpret_cast<const char*>(filePath), faceIndex };
        FcPatternDestroy(matched);
        return resolved;
    }
#endif

#if defined(DENIGMA_USE_DIRECTWRITE)
    static std::wstring utf8ToWide(const std::string& value)
    {
        if (value.empty()) {
            return {};
        }
        const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
        if (chars <= 1) {
            return {};
        }
        std::wstring output(static_cast<size_t>(chars), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, output.data(), chars) <= 0) {
            return {};
        }
        output.resize(static_cast<size_t>(chars - 1));
        return output;
    }

    static std::string wideToUtf8(const std::wstring& value)
    {
        if (value.empty()) {
            return {};
        }
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 1) {
            return {};
        }
        std::string output(static_cast<size_t>(bytes), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, output.data(), bytes, nullptr, nullptr) <= 0) {
            return {};
        }
        output.resize(static_cast<size_t>(bytes - 1));
        return output;
    }

    std::optional<ResolvedFace> resolveWithDirectWriteLocked(const std::string& familyName,
                                                             bool bold,
                                                             bool italic) const
    {
        const std::wstring familyWide = utf8ToWide(familyName);
        if (familyWide.empty()) {
            return std::nullopt;
        }

        IDWriteFactory* factory = nullptr;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&factory))) || !factory) {
            return std::nullopt;
        }

        IDWriteFontCollection* collection = nullptr;
        if (FAILED(factory->GetSystemFontCollection(&collection, FALSE)) || !collection) {
            safeRelease(factory);
            return std::nullopt;
        }

        IDWriteFontFamily* family = nullptr;
        IDWriteFont* font = nullptr;
        IDWriteGdiInterop* gdiInterop = nullptr;
        if (SUCCEEDED(factory->GetGdiInterop(&gdiInterop)) && gdiInterop) {
            LOGFONTW logFont{};
            logFont.lfWeight = bold ? FW_BOLD : FW_NORMAL;
            logFont.lfItalic = italic ? TRUE : FALSE;
            logFont.lfCharSet = DEFAULT_CHARSET;
            wcsncpy_s(logFont.lfFaceName, familyWide.c_str(), _TRUNCATE);
            (void)gdiInterop->CreateFontFromLOGFONT(&logFont, &font);
        }
        safeRelease(gdiInterop);

        if (!font) {
            UINT32 familyIndex = 0;
            BOOL familyExists = FALSE;
            if (FAILED(collection->FindFamilyName(familyWide.c_str(), &familyIndex, &familyExists)) || !familyExists) {
                safeRelease(collection);
                safeRelease(factory);
                return std::nullopt;
            }

            if (FAILED(collection->GetFontFamily(familyIndex, &family)) || !family) {
                safeRelease(collection);
                safeRelease(factory);
                return std::nullopt;
            }

            if (FAILED(family->GetFirstMatchingFont(bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_REGULAR,
                                                    DWRITE_FONT_STRETCH_NORMAL,
                                                    italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                                    &font))
                || !font) {
                safeRelease(family);
                safeRelease(collection);
                safeRelease(factory);
                return std::nullopt;
            }
        }

        IDWriteFontFace* fontFace = nullptr;
        if (FAILED(font->CreateFontFace(&fontFace)) || !fontFace) {
            safeRelease(font);
            safeRelease(family);
            safeRelease(collection);
            safeRelease(factory);
            return std::nullopt;
        }

        UINT32 fileCount = 0;
        if (FAILED(fontFace->GetFiles(&fileCount, nullptr)) || fileCount == 0) {
            safeRelease(fontFace);
            safeRelease(font);
            safeRelease(family);
            safeRelease(collection);
            safeRelease(factory);
            return std::nullopt;
        }

        std::vector<IDWriteFontFile*> files(fileCount, nullptr);
        if (FAILED(fontFace->GetFiles(&fileCount, files.data())) || files.empty() || !files.front()) {
            for (auto* file : files) {
                safeRelease(file);
            }
            safeRelease(fontFace);
            safeRelease(font);
            safeRelease(family);
            safeRelease(collection);
            safeRelease(factory);
            return std::nullopt;
        }

        IDWriteFontFileLoader* fileLoader = nullptr;
        IDWriteLocalFontFileLoader* localLoader = nullptr;
        const void* referenceKey = nullptr;
        UINT32 referenceKeySize = 0;
        std::optional<ResolvedFace> resolved = std::nullopt;

        if (SUCCEEDED(files.front()->GetLoader(&fileLoader)) && fileLoader) {
            if (SUCCEEDED(fileLoader->QueryInterface(__uuidof(IDWriteLocalFontFileLoader), reinterpret_cast<void**>(&localLoader)))
                && localLoader) {
                files.front()->GetReferenceKey(&referenceKey, &referenceKeySize);
                if (referenceKey && referenceKeySize > 0) {
                    UINT32 pathLength = 0;
                    if (SUCCEEDED(localLoader->GetFilePathLengthFromKey(referenceKey, referenceKeySize, &pathLength))) {
                        std::wstring filePath(static_cast<size_t>(pathLength), L'\0');
                        if (SUCCEEDED(localLoader->GetFilePathFromKey(referenceKey, referenceKeySize, filePath.data(), pathLength + 1))) {
                            ResolvedFace value;
                            value.filePath = wideToUtf8(filePath);
                            value.faceIndex = static_cast<int>(fontFace->GetIndex());
                            if (!value.filePath.empty()) {
                                resolved = value;
                            }
                        }
                    }
                }
            }
        }

        safeRelease(localLoader);
        safeRelease(fileLoader);
        for (auto* file : files) {
            safeRelease(file);
        }
        safeRelease(fontFace);
        safeRelease(font);
        safeRelease(family);
        safeRelease(collection);
        safeRelease(factory);
        return resolved;
    }
#endif

#if defined(DENIGMA_USE_CORETEXT)
    static std::string cfStringToStdString(CFStringRef value)
    {
        if (!value) {
            return {};
        }
        const CFIndex len = CFStringGetLength(value);
        if (len <= 0) {
            return {};
        }
        const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
        std::vector<char> buffer(static_cast<size_t>(maxSize), '\0');
        if (CFStringGetCString(value, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
            return std::string(buffer.data());
        }
        return {};
    }

    std::optional<int> resolveFaceIndexInFileLocked(const std::string& filePath,
                                                    const std::string& postScriptName,
                                                    const std::string& familyName,
                                                    bool bold,
                                                    bool italic) const
    {
        if (!m_library || filePath.empty()) {
            return std::nullopt;
        }

        FT_Face probeFace = nullptr;
        if (FT_New_Face(m_library, filePath.c_str(), 0, &probeFace) != 0 || !probeFace) {
            return std::nullopt;
        }
        const long numFaces = (std::max)(1L, static_cast<long>(probeFace->num_faces));
        FT_Done_Face(probeFace);

        const std::string targetPostScript = normalizeName(postScriptName);
        const std::string targetFamily = normalizeName(familyName);
        int bestScore = (std::numeric_limits<int>::min)();
        std::optional<int> bestIndex = std::nullopt;

        for (long faceIndex = 0; faceIndex < numFaces; ++faceIndex) {
            FT_Face face = nullptr;
            if (FT_New_Face(m_library, filePath.c_str(), faceIndex, &face) != 0 || !face) {
                continue;
            }

            int score = 0;
            const std::string candidateFamily = normalizeName(face->family_name ? std::string(face->family_name) : std::string{});
            if (!targetFamily.empty()) {
                if (candidateFamily == targetFamily) {
                    score += 100;
                } else if (candidateFamily.find(targetFamily) != std::string::npos
                           || targetFamily.find(candidateFamily) != std::string::npos) {
                    score += 40;
                } else {
                    score -= 40;
                }
            }

            const char* postScript = FT_Get_Postscript_Name(face);
            if (!targetPostScript.empty()) {
                const std::string candidatePostScript = normalizeName(postScript ? std::string(postScript) : std::string{});
                if (candidatePostScript == targetPostScript) {
                    score += 1000;
                } else if (!candidatePostScript.empty()
                        && (candidatePostScript.find(targetPostScript) != std::string::npos
                            || targetPostScript.find(candidatePostScript) != std::string::npos)) {
                    score += 300;
                } else {
                    score -= 80;
                }
            }

            const std::string style = face->style_name ? std::string(face->style_name) : std::string{};
            const bool candidateBold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0 || styleLooksBold(style);
            const bool candidateItalic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0 || styleLooksItalic(style);
            score += (candidateBold == bold) ? 20 : -20;
            score += (candidateItalic == italic) ? 20 : -10;

            if (!bestIndex || score > bestScore) {
                bestScore = score;
                bestIndex = static_cast<int>(faceIndex);
            }

            FT_Done_Face(face);
        }

        return bestIndex;
    }

    std::optional<ResolvedFace> resolveWithCoreTextLocked(const std::string& familyName,
                                                           bool bold,
                                                           bool italic) const
    {
        if (familyName.empty()) {
            return std::nullopt;
        }

        CFStringRef cfFamily = CFStringCreateWithCString(kCFAllocatorDefault, familyName.c_str(), kCFStringEncodingUTF8);
        if (!cfFamily) {
            return std::nullopt;
        }
        CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 2,
                                                                 &kCFCopyStringDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks);
        if (!attrs) {
            CFRelease(cfFamily);
            return std::nullopt;
        }
        CFDictionarySetValue(attrs, kCTFontFamilyNameAttribute, cfFamily);

        CFMutableDictionaryRef traits = CFDictionaryCreateMutable(kCFAllocatorDefault, 2,
                                                                  &kCFCopyStringDictionaryKeyCallBacks,
                                                                  &kCFTypeDictionaryValueCallBacks);
        if (traits) {
            const double weight = bold ? 0.4 : 0.0;
            const double slant = italic ? 1.0 : 0.0;
            CFNumberRef weightNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &weight);
            CFNumberRef slantNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &slant);
            if (weightNumber) {
                CFDictionarySetValue(traits, kCTFontWeightTrait, weightNumber);
            }
            if (slantNumber) {
                CFDictionarySetValue(traits, kCTFontSlantTrait, slantNumber);
            }
            if (weightNumber) CFRelease(weightNumber);
            if (slantNumber) CFRelease(slantNumber);
            CFDictionarySetValue(attrs, kCTFontTraitsAttribute, traits);
            CFRelease(traits);
        }

        CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(attrs);
        CFRelease(attrs);
        CFRelease(cfFamily);
        if (!descriptor) {
            return std::nullopt;
        }

        CTFontDescriptorRef matched = CTFontDescriptorCreateMatchingFontDescriptor(descriptor, nullptr);
        CFRelease(descriptor);
        if (!matched) {
            return std::nullopt;
        }

        std::optional<ResolvedFace> resolved = std::nullopt;
        std::string postScriptName;
        CFTypeRef postScriptAttr = CTFontDescriptorCopyAttribute(matched, kCTFontNameAttribute);
        if (postScriptAttr && CFGetTypeID(postScriptAttr) == CFStringGetTypeID()) {
            postScriptName = cfStringToStdString(static_cast<CFStringRef>(postScriptAttr));
        }
        if (postScriptAttr) {
            CFRelease(postScriptAttr);
        }

        CFTypeRef urlAttr = CTFontDescriptorCopyAttribute(matched, kCTFontURLAttribute);
        if (urlAttr && CFGetTypeID(urlAttr) == CFURLGetTypeID()) {
            CFStringRef pathRef = CFURLCopyFileSystemPath(static_cast<CFURLRef>(urlAttr), kCFURLPOSIXPathStyle);
            if (pathRef) {
                ResolvedFace value;
                value.filePath = cfStringToStdString(pathRef);
                CFRelease(pathRef);
                if (!value.filePath.empty()) {
                    value.faceIndex = resolveFaceIndexInFileLocked(value.filePath, postScriptName, familyName, bold, italic).value_or(0);
                    resolved = value;
                }
            }
        }
        if (urlAttr) {
            CFRelease(urlAttr);
        }
        CFRelease(matched);
        return resolved;
    }
#endif

    std::optional<ResolvedFace> resolveWithNativeLocked(const std::string& familyName,
                                                        bool bold,
                                                        bool italic) const
    {
#if defined(MUSX_RUNNING_ON_WINDOWS) && defined(DENIGMA_USE_DIRECTWRITE)
        if (auto resolved = resolveWithDirectWriteLocked(familyName, bold, italic)) {
            return resolved;
        }
#endif
#if defined(MUSX_RUNNING_ON_LINUX_UNIX) && defined(DENIGMA_USE_FONTCONFIG)
        if (auto resolved = resolveWithFontconfigLocked(familyName, bold, italic)) {
            return resolved;
        }
#endif
#if defined(MUSX_RUNNING_ON_MACOS) && defined(DENIGMA_USE_CORETEXT)
        if (auto resolved = resolveWithCoreTextLocked(familyName, bold, italic)) {
            return resolved;
        }
#endif
        return std::nullopt;
    }

    std::optional<ResolvedFace> resolveWithIndexLocked(const std::string& familyName,
                                                       bool bold,
                                                       bool italic)
    {
        ensureIndexBuiltLocked();
        if (m_faceIndex.empty()) {
            return std::nullopt;
        }

        const std::string familyNorm = normalizeName(familyName);
        int bestScore = -1;
        std::optional<ResolvedFace> bestMatch = std::nullopt;
        for (const auto& candidate : m_faceIndex) {
            int score = 0;
            if (candidate.familyNormalized == familyNorm) {
                score += 100;
            } else if (!familyNorm.empty()
                    && (candidate.familyNormalized.find(familyNorm) != std::string::npos
                        || familyNorm.find(candidate.familyNormalized) != std::string::npos)) {
                score += 50;
            } else {
                continue;
            }

            score += (candidate.bold == bold) ? 10 : -10;
            score += (candidate.italic == italic) ? 10 : -5;

            if (score > bestScore) {
                bestScore = score;
                bestMatch = candidate.resolved;
            }
        }
        return bestMatch;
    }

    std::optional<FT_Face> resolveFaceLocked(const musx::dom::FontInfo& fontInfo,
                                             double pointSize,
                                             const DenigmaContext& denigmaContext)
    {
        if (!m_initialized || !m_library) {
            warnBackendUnavailableLocked(denigmaContext);
            return std::nullopt;
        }

        std::string familyName;
        try {
            familyName = fontInfo.getName();
        } catch (...) {
            familyName.clear();
        }
        if (familyName.empty()) {
            warnUnresolvedFamilyLocked(denigmaContext, familyName);
            return std::nullopt;
        }

        auto resolved = resolveWithNativeLocked(familyName, fontInfo.bold, fontInfo.italic);
        if (!resolved) {
            resolved = resolveWithIndexLocked(familyName, fontInfo.bold, fontInfo.italic);
        }
        if (!resolved) {
            warnUnresolvedFamilyLocked(denigmaContext, familyName);
            return std::nullopt;
        }

        const FaceKey key{ resolved->filePath, resolved->faceIndex };
        FT_Face face = nullptr;
        auto cacheIt = m_faces.find(key);
        if (cacheIt == m_faces.end()) {
            if (FT_New_Face(m_library, resolved->filePath.c_str(), resolved->faceIndex, &face) != 0 || !face) {
                warnUnresolvedFamilyLocked(denigmaContext, familyName);
                return std::nullopt;
            }
            cacheIt = m_faces.emplace(key, face).first;
        } else {
            face = cacheIt->second;
        }

        const double sizePoints = pointSize > 0.0 ? pointSize : 12.0;
        const auto size26d6 = static_cast<FT_F26Dot6>(std::llround(sizePoints * 64.0));
        if (FT_Set_Char_Size(face, 0, size26d6, 72, 72) != 0) {
            warnUnresolvedFamilyLocked(denigmaContext, familyName);
            return std::nullopt;
        }
        return face;
    }

    mutable std::mutex m_mutex;
    FT_Library m_library{};
    bool m_initialized{};
    bool m_warnedBackendUnavailable{};
    std::unordered_set<std::string> m_warnedUnresolvedFamilies;
    std::unordered_map<FaceKey, FT_Face, FaceKeyHash> m_faces;

    bool m_indexBuilt{};
    std::vector<IndexedFace> m_faceIndex;

#if defined(DENIGMA_USE_FONTCONFIG)
    bool m_fontconfigAvailable{};
#endif
};

FreeTypeTextMetricsBackend& backend()
{
    static FreeTypeTextMetricsBackend instance;
    return instance;
}

#endif // DENIGMA_USE_FREETYPE

} // namespace

std::optional<TextMetricsEvpu> measureTextEvpu(const musx::dom::FontInfo& fontInfo,
                                               std::u32string_view text,
                                               std::optional<double> pointSizeOverride,
                                               const DenigmaContext& denigmaContext)
{
#if defined(DENIGMA_USE_FREETYPE)
    return backend().measureText(fontInfo, text, pointSizeOverride, denigmaContext);
#else
    (void)fontInfo;
    (void)text;
    (void)pointSizeOverride;
    warnMissingBackend(denigmaContext);
    return std::nullopt;
#endif
}

std::optional<double> measureGlyphWidthEvpu(const musx::dom::FontInfo& fontInfo,
                                            char32_t codePoint,
                                            std::optional<double> pointSizeOverride,
                                            const DenigmaContext& denigmaContext)
{
#if defined(DENIGMA_USE_FREETYPE)
    return backend().measureGlyphWidth(fontInfo, codePoint, pointSizeOverride, denigmaContext);
#else
    (void)fontInfo;
    (void)codePoint;
    (void)pointSizeOverride;
    warnMissingBackend(denigmaContext);
    return std::nullopt;
#endif
}

std::optional<double> measureFontHeightEvpu(const musx::dom::FontInfo& fontInfo,
                                            double pointSize,
                                            const DenigmaContext& denigmaContext)
{
#if defined(DENIGMA_USE_FREETYPE)
    return backend().measureHeight(fontInfo, pointSize, denigmaContext);
#else
    (void)fontInfo;
    (void)pointSize;
    warnMissingBackend(denigmaContext);
    return std::nullopt;
#endif
}

std::optional<TextMetricsEvpu> measureFontAscentDescentEvpu(const musx::dom::FontInfo& fontInfo,
                                                            std::optional<double> pointSizeOverride,
                                                            const DenigmaContext& denigmaContext)
{
#if defined(DENIGMA_USE_FREETYPE)
    return backend().measureAscentDescent(fontInfo, pointSizeOverride, denigmaContext);
#else
    (void)fontInfo;
    (void)pointSizeOverride;
    warnMissingBackend(denigmaContext);
    return std::nullopt;
#endif
}

musx::util::SvgConvert::GlyphMetricsFn makeSvgGlyphMetricsCallback(const DenigmaContext& denigmaContext)
{
    const DenigmaContext* contextPtr = &denigmaContext;
    return [contextPtr](const musx::dom::FontInfo& font,
                        std::u32string_view text) -> std::optional<musx::util::SvgConvert::GlyphMetrics> {
        if (!contextPtr) {
            return std::nullopt;
        }
        auto measured = measureTextEvpu(font, text, std::nullopt, *contextPtr);
        if (!measured) {
            return std::nullopt;
        }
        auto verticalMetrics = measureFontAscentDescentEvpu(font, std::nullopt, *contextPtr);
        const bool useMeasuredVerticals = (measured->ascent != 0.0) || (measured->descent != 0.0);
        const double glyphAscent = useMeasuredVerticals
            ? measured->ascent
            : (verticalMetrics ? verticalMetrics->ascent : measured->ascent);
        const double glyphDescent = useMeasuredVerticals
            ? measured->descent
            : (verticalMetrics ? verticalMetrics->descent : measured->descent);
        std::string fontName;
        try {
            fontName = font.getName();
        } catch (...) {
            fontName.clear();
        }
        const uint32_t cp = text.empty() ? 0 : static_cast<uint32_t>(text.front());
        contextPtr->logMessage(LogMsg() << "SVG metrics callback [freetype]"
                                        << " font=\"" << fontName << "\""
                                        << " sizePt=" << font.fontSize
                                        << " cpDec=" << cp
                                        << " measuredAdvance=" << measured->advance
                                        << " measuredAscent=" << measured->ascent
                                        << " measuredDescent=" << measured->descent
                                        << " finalAscent=" << glyphAscent
                                        << " finalDescent=" << glyphDescent,
                             LogSeverity::Verbose);
        return musx::util::SvgConvert::GlyphMetrics{ measured->advance, glyphAscent, glyphDescent };
    };
}

} // namespace textmetrics
} // namespace denigma
