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
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>

#include "utils/ziputils.h"

#include "pugixml.hpp"
#include "unzip.h"
#include "zip.h"

#ifdef _WIN32
#include "iowin32.h"
#endif

namespace utils {

using namespace denigma;

namespace {

struct RandomAccessZipStream
{
    const IRandomAccessReader* reader{};
    std::uint64_t position{};
    bool error{};
};

struct ZipEntryInfo {
    std::string filename;
    unz_file_info64 info{};
    bool isDirectory{};
    bool isFile{};
    bool isSymLink{};
};

enum HostOS : unsigned {
    HostFAT = 0,
    HostUnix = 3,
    HostHPFS = 6,
    HostNTFS = 11,
    HostVFAT = 14,
    HostOSX = 19
};

namespace WindowsFileAttributes {
enum : unsigned {
    Dir = 0x10,
    TypeMask = 0x90
};
}

namespace UnixFileAttributes {
enum : unsigned {
    Dir = 0040000,
    SymLink = 0120000,
    TypeMask = 0170000
};
}

static unsigned zipEntryHostOS(const unz_file_info64& info)
{
    return (info.version >> 8) & 0xffU;
}

static void determineEntryType(ZipEntryInfo& entry)
{
    const unsigned hostOS = zipEntryHostOS(entry.info);
    unsigned mode = static_cast<unsigned>(entry.info.external_fa);
    switch (hostOS) {
    case HostUnix:
    case HostOSX:
        mode = (mode >> 16) & 0xffffU;
        entry.isDirectory = (mode & UnixFileAttributes::TypeMask) == UnixFileAttributes::Dir;
        entry.isSymLink = (mode & UnixFileAttributes::TypeMask) == UnixFileAttributes::SymLink;
        break;
    case HostFAT:
    case HostNTFS:
    case HostHPFS:
    case HostVFAT:
        entry.isDirectory = (mode & WindowsFileAttributes::TypeMask) == WindowsFileAttributes::Dir;
        entry.isSymLink = false;
        break;
    default:
        entry.isDirectory = !entry.filename.empty() && (entry.filename.back() == '/' || entry.filename.back() == '\\');
        entry.isSymLink = false;
        break;
    }
    entry.isFile = !entry.isDirectory && !entry.isSymLink;
}

static unzFile openZipForRead(const std::filesystem::path& zipFilePath, const DenigmaContext& denigmaContext)
{
#ifdef _WIN32
    zlib_filefunc64_def fileFuncs{};
    fill_win32_filefunc64W(&fileFuncs);
    const std::wstring widePath = zipFilePath.wstring();
    unzFile zip = unzOpen2_64(widePath.c_str(), &fileFuncs);
#else
    const auto utf8Path = zipFilePath.u8string();
    unzFile zip = unzOpen64(utf8Path.c_str());
#endif

    if (!zip) {
        denigmaContext.logMessage(LogMsg() << "unable to extract data from file " << utils::asUtf8Bytes(zipFilePath), LogSeverity::Error);
        throw std::runtime_error("unable to open zip archive");
    }
    return zip;
}

static voidpf ZCALLBACK randomAccessOpen64(voidpf opaque, const void*, int mode)
{
    if ((mode & ZLIB_FILEFUNC_MODE_READ) == 0) {
        return nullptr;
    }
    auto* reader = static_cast<const IRandomAccessReader*>(opaque);
    if (!reader) {
        return nullptr;
    }
    return new RandomAccessZipStream{ reader, 0, false };
}

static uLong ZCALLBACK randomAccessRead(voidpf, voidpf stream, void* buffer, uLong size)
{
    auto* zipStream = static_cast<RandomAccessZipStream*>(stream);
    if (!zipStream || !buffer) {
        return 0;
    }

    try {
        const auto bytesRead = zipStream->reader->readAt(
            zipStream->position,
            std::span<std::byte>(reinterpret_cast<std::byte*>(buffer), static_cast<std::size_t>(size)));
        zipStream->position += bytesRead;
        return static_cast<uLong>(bytesRead);
    } catch (...) {
        zipStream->error = true;
        return 0;
    }
}

static uLong ZCALLBACK randomAccessWrite(voidpf, voidpf, const void*, uLong)
{
    return 0;
}

static ZPOS64_T ZCALLBACK randomAccessTell64(voidpf, voidpf stream)
{
    auto* zipStream = static_cast<RandomAccessZipStream*>(stream);
    return zipStream ? static_cast<ZPOS64_T>(zipStream->position) : static_cast<ZPOS64_T>(0);
}

static long ZCALLBACK randomAccessSeek64(voidpf, voidpf stream, ZPOS64_T offset, int origin)
{
    auto* zipStream = static_cast<RandomAccessZipStream*>(stream);
    if (!zipStream) {
        return -1;
    }

    std::uint64_t base{};
    switch (origin) {
    case ZLIB_FILEFUNC_SEEK_SET:
        base = 0;
        break;
    case ZLIB_FILEFUNC_SEEK_CUR:
        base = zipStream->position;
        break;
    case ZLIB_FILEFUNC_SEEK_END:
        base = zipStream->reader->size();
        break;
    default:
        return -1;
    }

    const auto nextPosition = base + static_cast<std::uint64_t>(offset);
    if (nextPosition > zipStream->reader->size()) {
        return -1;
    }
    zipStream->position = nextPosition;
    return 0;
}

static int ZCALLBACK randomAccessClose(voidpf, voidpf stream)
{
    delete static_cast<RandomAccessZipStream*>(stream);
    return 0;
}

static int ZCALLBACK randomAccessError(voidpf, voidpf stream)
{
    auto* zipStream = static_cast<RandomAccessZipStream*>(stream);
    return zipStream && zipStream->error ? 1 : 0;
}

static unzFile openZipForRead(const IRandomAccessReader& reader, const DenigmaContext& denigmaContext)
{
    zlib_filefunc64_def fileFuncs{};
    fileFuncs.zopen64_file = randomAccessOpen64;
    fileFuncs.zread_file = randomAccessRead;
    fileFuncs.zwrite_file = randomAccessWrite;
    fileFuncs.ztell64_file = randomAccessTell64;
    fileFuncs.zseek64_file = randomAccessSeek64;
    fileFuncs.zclose_file = randomAccessClose;
    fileFuncs.zerror_file = randomAccessError;
    fileFuncs.opaque = const_cast<IRandomAccessReader*>(&reader);

    unzFile zip = unzOpen2_64(nullptr, &fileFuncs);
    if (!zip) {
        denigmaContext.logMessage(LogMsg() << "unable to extract data from random-access zip reader", LogSeverity::Error);
        throw std::runtime_error("unable to open random-access zip archive");
    }
    return zip;
}

static zipFile openZipForWrite(const std::filesystem::path& outputPath)
{
#ifdef _WIN32
    zlib_filefunc64_def fileFuncs{};
    fill_win32_filefunc64W(&fileFuncs);
    const std::wstring widePath = outputPath.wstring();
    return zipOpen2_64(widePath.c_str(), APPEND_STATUS_CREATE, nullptr, &fileFuncs);
#else
    const auto utf8Path = outputPath.u8string();
    return zipOpen64(utf8Path.c_str(), APPEND_STATUS_CREATE);
#endif
}

static ZipEntryInfo getCurrentEntryInfo(unzFile zip)
{
    ZipEntryInfo entry{};
    int rc = unzGetCurrentFileInfo64(zip, &entry.info, nullptr, 0, nullptr, 0, nullptr, 0);
    if (rc != UNZ_OK) {
        throw std::runtime_error("unable to read zip entry metadata");
    }

    std::string fileName(entry.info.size_filename, '\0');
    rc = unzGetCurrentFileInfo64(
        zip,
        &entry.info,
        fileName.empty() ? nullptr : fileName.data(),
        static_cast<uLong>(fileName.size()),
        nullptr,
        0,
        nullptr,
        0
    );
    if (rc != UNZ_OK) {
        throw std::runtime_error("unable to read zip entry filename");
    }

    entry.filename = std::move(fileName);
    determineEntryType(entry);
    return entry;
}

static std::string readCurrentFile(unzFile zip)
{
    int rc = unzOpenCurrentFile(zip);
    if (rc != UNZ_OK) {
        throw std::runtime_error("unable to open entry in zip archive");
    }

    std::string output;
    std::array<char, 16384> chunk{};

    while (true) {
        int readRc = unzReadCurrentFile(zip, chunk.data(), static_cast<unsigned>(chunk.size()));
        if (readRc < 0) {
            unzCloseCurrentFile(zip);
            throw std::runtime_error("unable to read entry from zip archive");
        }
        if (readRc == 0) {
            break;
        }
        output.append(chunk.data(), static_cast<std::size_t>(readRc));
    }

    rc = unzCloseCurrentFile(zip);
    if (rc != UNZ_OK) {
        throw std::runtime_error("unable to close entry in zip archive");
    }

    return output;
}

static bool iterateFiles(unzFile zip, const std::optional<std::string>& searchForFile,
    const std::function<bool(const ZipEntryInfo&)>& iterator)
{
    int rc = unzGoToFirstFile(zip);
    if (rc == UNZ_END_OF_LIST_OF_FILE) {
        return false;
    }
    if (rc != UNZ_OK) {
        throw std::runtime_error("unable to enumerate zip archive entries");
    }

    bool calledIterator = false;
    while (true) {
        ZipEntryInfo fileInfo = getCurrentEntryInfo(zip);
        if (!searchForFile.has_value() || searchForFile.value() == fileInfo.filename) {
            calledIterator = true;
            if (!iterator(fileInfo)) {
                break;
            }
        }

        rc = unzGoToNextFile(zip);
        if (rc == UNZ_END_OF_LIST_OF_FILE) {
            break;
        }
        if (rc != UNZ_OK) {
            throw std::runtime_error("unable to advance zip archive cursor");
        }
    }

    return calledIterator;
}

static void writeEntryToZip(zipFile outputZip, const ZipEntryInfo& fileInfo, const std::string& fileContents)
{
    zip_fileinfo zipInfo{};
    zipInfo.tmz_date.tm_sec = fileInfo.info.tmu_date.tm_sec;
    zipInfo.tmz_date.tm_min = fileInfo.info.tmu_date.tm_min;
    zipInfo.tmz_date.tm_hour = fileInfo.info.tmu_date.tm_hour;
    zipInfo.tmz_date.tm_mday = fileInfo.info.tmu_date.tm_mday;
    zipInfo.tmz_date.tm_mon = fileInfo.info.tmu_date.tm_mon;
    zipInfo.tmz_date.tm_year = fileInfo.info.tmu_date.tm_year;
    zipInfo.internal_fa = fileInfo.info.internal_fa;
    zipInfo.external_fa = fileInfo.info.external_fa;

    const int method = (fileInfo.info.compression_method == 0) ? 0 : Z_DEFLATED;
    const int useZip64 = fileContents.size() >= 0xffffffffULL ? 1 : 0;
    int rc = zipOpenNewFileInZip64(
        outputZip,
        fileInfo.filename.c_str(),
        &zipInfo,
        nullptr,
        0,
        nullptr,
        0,
        nullptr,
        method,
        Z_DEFAULT_COMPRESSION,
        useZip64
    );
    if (rc != ZIP_OK) {
        throw std::runtime_error("unable to create entry in output zip archive");
    }

    std::size_t offset = 0;
    while (offset < fileContents.size()) {
        const std::size_t chunkSize = std::min<std::size_t>(fileContents.size() - offset, std::numeric_limits<unsigned>::max());
        rc = zipWriteInFileInZip(outputZip, fileContents.data() + offset, static_cast<unsigned>(chunkSize));
        if (rc < 0) {
            zipCloseFileInZip(outputZip);
            throw std::runtime_error("unable to write entry data to output zip archive");
        }
        offset += chunkSize;
    }

    rc = zipCloseFileInZip(outputZip);
    if (rc != ZIP_OK) {
        throw std::runtime_error("unable to finalize entry in output zip archive");
    }
}

static std::string getMusicXmlScoreName(const std::filesystem::path& zipFilePath, unzFile zip, const denigma::DenigmaContext& denigmaContext)
{
    std::filesystem::path defaultName = zipFilePath.filename();
    defaultName.replace_extension(MUSICXML_EXTENSION);
    auto fileName = defaultName.filename().u8string();
    try {
        iterateFiles(zip, std::nullopt, [&](const ZipEntryInfo& entry) {
            if (entry.filename == "META-INF/container.xml") {
                pugi::xml_document containerXml;
                const std::string xmlBuffer = readCurrentFile(zip);
                auto parseResult = containerXml.load_buffer(xmlBuffer.data(), xmlBuffer.size());
                if (!parseResult) {
                    throw std::runtime_error("Error parsing container.xml: " + std::string(parseResult.description()));
                }
                if (auto path = containerXml.child("container").child("rootfiles").child("rootfile").attribute("full-path")) {
                    fileName = utils::stringToUtf8(path.value());
                }
                return false;
            }
            return true;
        });
        return utils::utf8ToString(fileName);
    } catch (const std::exception& ex) {
        denigmaContext.logMessage(LogMsg() << "unable to extract META-INF/container.xml from file " << utils::asUtf8Bytes(zipFilePath), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

} // namespace

std::string readFile(const std::filesystem::path& zipFilePath, const std::string& fileName, const DenigmaContext& denigmaContext)
{
    FileRandomAccessReader reader(zipFilePath);
    return readFile(reader, fileName, denigmaContext);
}

std::string readFile(const IRandomAccessReader& reader, const std::string& fileName, const DenigmaContext& denigmaContext)
{
    unzFile zip = openZipForRead(reader, denigmaContext);
    try {
        const int rc = unzLocateFile(zip, fileName.c_str(), 1);
        if (rc != UNZ_OK) {
            throw std::runtime_error("unable to locate file in zip archive: " + fileName);
        }
        std::string output = readCurrentFile(zip);
        unzClose(zip);
        return output;
    } catch (...) {
        unzClose(zip);
        throw;
    }
}

MusxArchiveFiles readMusxArchiveFiles(const std::filesystem::path& zipFilePath, const DenigmaContext& denigmaContext)
{
    FileRandomAccessReader reader(zipFilePath);
    return readMusxArchiveFiles(reader, denigmaContext);
}

MusxArchiveFiles readMusxArchiveFiles(const IRandomAccessReader& reader, const DenigmaContext& denigmaContext)
{
    constexpr char kScoreDatName[] = "score.dat";
    constexpr char kNotationMetadataName[] = "NotationMetadata.xml";
    constexpr char8_t kGraphicsDirName[] = u8"graphics";

    unzFile zip = openZipForRead(reader, denigmaContext);
    try {
        MusxArchiveFiles result;
        bool foundScoreDat = false;
        iterateFiles(zip, std::nullopt, [&](const ZipEntryInfo& fileInfo) {
            if (fileInfo.filename == kScoreDatName) {
                result.scoreDat = readCurrentFile(zip);
                foundScoreDat = true;
                return true;
            }

            if (fileInfo.filename == kNotationMetadataName) {
                result.notationMetadata = readCurrentFile(zip);
                return true;
            }

            const std::filesystem::path entryPath = utils::utf8ToPath(fileInfo.filename);
            if (!fileInfo.isFile
                || entryPath.parent_path().u8string() != kGraphicsDirName
                || !entryPath.has_filename()) {
                return true;
            }

            denigma::CommandInputData::EmbeddedGraphicFile graphicFile;
            graphicFile.filename = utils::utf8ToString(entryPath.filename().u8string());
            graphicFile.blob = readCurrentFile(zip);
            result.embeddedGraphics.emplace_back(std::move(graphicFile));
            return true;
        });

        if (!foundScoreDat) {
            throw std::runtime_error("unable to locate file in zip archive: " + std::string(kScoreDatName));
        }

        unzClose(zip);
        return result;
    } catch (...) {
        unzClose(zip);
        throw;
    }
}

std::string getMusicXmlScoreFile(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext)
{
    unzFile zip = openZipForRead(zipFilePath, denigmaContext);
    try {
        std::string scoreName = getMusicXmlScoreName(zipFilePath, zip, denigmaContext);
        const int rc = unzLocateFile(zip, scoreName.c_str(), 1);
        if (rc != UNZ_OK) {
            throw std::runtime_error("unable to locate score in zip archive: " + scoreName);
        }
        std::string output = readCurrentFile(zip);
        unzClose(zip);
        return output;
    } catch (...) {
        unzClose(zip);
        throw;
    }
}

bool iterateMusicXmlPartFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext, const std::optional<std::string>& fileName, IteratorFunc iterator)
{
    unzFile zip = openZipForRead(zipFilePath, denigmaContext);
    try {
        const std::string scoreName = getMusicXmlScoreName(zipFilePath, zip, denigmaContext);
        bool retval = iterateFiles(zip, std::nullopt, [&](const ZipEntryInfo& fileInfo) {
            if (!fileInfo.isFile) {
                return true;
            }
            if (scoreName == fileInfo.filename) {
                return true; // skip score
            }
            if (fileName.has_value() && fileName.value() != fileInfo.filename) {
                return true; // skip parts that aren't the one we are looking for
            }
            std::filesystem::path nextPath = utils::utf8ToPath(fileInfo.filename);
            if (utils::pathExtensionEquals(nextPath, MUSICXML_EXTENSION)) {
                return iterator(nextPath, readCurrentFile(zip));
            }
            return true;
        });
        unzClose(zip);
        return retval;
    } catch (...) {
        unzClose(zip);
        throw;
    }
}

bool iterateModifyFilesInPlace(const std::filesystem::path& zipFilePath, const std::filesystem::path& outputPath, const denigma::DenigmaContext& denigmaContext, ModifyIteratorFunc iterator)
{
    unzFile inputZip = openZipForRead(zipFilePath, denigmaContext);
    zipFile outputZip = openZipForWrite(outputPath);
    if (!outputZip) {
        unzClose(inputZip);
        denigmaContext.logMessage(LogMsg() << "unable to save data to file " << utils::asUtf8Bytes(outputPath), LogSeverity::Error);
        throw std::runtime_error("unable to create output zip archive");
    }

    try {
        const std::string scoreName = getMusicXmlScoreName(zipFilePath, inputZip, denigmaContext);
        bool retval = iterateFiles(inputZip, std::nullopt, [&](const ZipEntryInfo& fileInfo) {
            if (!fileInfo.isFile) {
                return true;
            }
            std::filesystem::path nextPath = utils::utf8ToPath(fileInfo.filename);
            std::string buffer = readCurrentFile(inputZip);
            if (iterator(nextPath, buffer, scoreName == fileInfo.filename)) {
                writeEntryToZip(outputZip, fileInfo, buffer);
            }
            return true;
        });

        zipClose(outputZip, nullptr);
        unzClose(inputZip);
        return retval;
    } catch (const std::exception& ex) {
        denigmaContext.logMessage(LogMsg() << "unable to save data to file " << utils::asUtf8Bytes(outputPath), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        zipClose(outputZip, nullptr);
        unzClose(inputZip);
        throw;
    }
}

} // namespace utils
