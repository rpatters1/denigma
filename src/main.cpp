#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <errno.h>

#include "zip_file.hpp"		// miniz submodule (for zip)
#include "ezgz.hpp"			// ezgz submodule (for gzip)

constexpr char MUSX_EXTENSION[]		    = ".musx";
constexpr char ENIGMAXML_EXTENSION[]    = ".enigmaxml";
constexpr char SCORE_DAT_NAME[] 		= "score.dat";

// Shout out to Deguerre https://github.com/Deguerre
struct ScoreFileCrypter
{
    constexpr static uint32_t INITIAL_STATE = 0x28006D45;

    static void cryptBuffer(std::string& buffer)
    {
        uint32_t state = INITIAL_STATE;
        int i = 0;
        for (size_t i = 0; i < buffer.size(); i++) {
            if (i % 0x20000 == 0) {
                state = INITIAL_STATE;
            }
            state = state * 0x41c64e6d + 0x3039; // BSD rand()!
            uint16_t upper = state >> 16;
            uint8_t c = upper + upper / 255;
            buffer[i] ^= c;
        }
    }
};

static std::vector<char> extractEnigmaXml(const std::string& zipFile)
{
    try {
        miniz_cpp::zip_file zip(zipFile.c_str());
        std::string buffer = zip.read(SCORE_DAT_NAME);
        ScoreFileCrypter::cryptBuffer(buffer);
        return EzGz::IGzFile<>({reinterpret_cast<uint8_t *>(buffer.data()), buffer.size()}).readAll();
    } catch (const std::exception &ex) {
        std::cout << "Error: unable to extract enigmaxml from file " << zipFile << " (exception: " << ex.what() << ")" << std::endl;
    }
    return {};
}

static bool writeEnigmaXml(const std::string& outputPath, const std::vector<char>& xmlBuffer)
{
    std::cout << "write to " << outputPath << "\n";

    try	{
        std::ifstream inFile;

        size_t uncompressedSize = xmlBuffer.size();
        std::cout << "decompressed Size " << uncompressedSize << "\n";

        std::ofstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(outputPath, std::ios::binary);
        xmlFile.write(xmlBuffer.data(), xmlBuffer.size());
    } catch (const std::ios_base::failure& ex) {
        std::cout << "unable to write " << outputPath << std::endl
                  << "message: " << ex.what() << std::endl
                  << "details: " << std::strerror(ex.code().value()) << std::endl;
    } catch (const std::exception &ex) {
        std::cout << "unable to write " << outputPath << " (exception: " << ex.what() << ")" << std::endl;
        return false;
    }

    return true;
}

static int showHelp(const std::string& programName)
{
    std::cerr << "Usage: " << programName << " <musx file>" << std::endl;
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc <= 0) {
        std::cerr << "Error: argv[0] is unavailable" << std::endl;
        return 1;
    }

    std::string programName = std::filesystem::path(argv[0]).stem().string();
    std::cout << programName << " " << MUSX2MNX_VERSION << std::endl;

    if (argc < 2) {
        return showHelp(programName);
    }

    const std::filesystem::path zipFilePath = argv[1];
    if (!std::filesystem::is_regular_file(zipFilePath) || zipFilePath.extension() != MUSX_EXTENSION) {
        return showHelp(programName);
    }
    const std::filesystem::path enigmaXmlPath = [zipFilePath]() -> std::filesystem::path
    {
        std::filesystem::path retval = zipFilePath;
        return retval.replace_extension(ENIGMAXML_EXTENSION);
    }();
    const std::vector<char> xmlBuffer = extractEnigmaXml(zipFilePath.string());
    if (xmlBuffer.size() <= 0) {
        std::cerr << "Failed to extract enigmaxml " << zipFilePath << std::endl;
        return 1;
    }

    if (!writeEnigmaXml(enigmaXmlPath.string(), xmlBuffer)) {
        std::cerr << "Failed to extract zip file " << zipFilePath << " to " << enigmaXmlPath << std::endl;
        return 1;
    }

    std::cout << "Extraction complete!" << std::endl;
    return 0; 
}
