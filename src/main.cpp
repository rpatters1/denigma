#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <errno.h>

#include "zip_file.hpp"		// miniz submodule (for zip)
#include "ezgz.hpp"			// ezgz submodule (for gzip)



//Shout out to Deguerre https://github.com/Deguerre
struct ScoreFileCrypter {
	const uint32_t initialState = 0x28006D45;

	void CryptBuffer(std::vector<uint8_t>& buffer)
	{
		size_t length = buffer.size();
		uint32_t state = initialState;
		int i = 0;
		while (length-- > 0) {
			if ( i % 0x20000 == 0 ) {
				state = initialState;
			}
			state = state * 0x41c64e6d + 0x3039; // BSD rand()!
			uint16_t upper = state >> 16;
			uint8_t c = upper + upper / 255;
			buffer[i++] ^= c;
		}
	}
};



void create_directories(const std::string& filePath) {
    std::filesystem::path file(filePath);
    std::filesystem::path parentDir = file.parent_path();

    // Create the parent directory if it doesn't exist
    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        std::error_code ec; // To capture any error during creation
        std::filesystem::create_directories(parentDir, ec); // Creates parentDir and all necessary parent directories
        if (ec) {
            std::cerr << "Error creating directories: " << ec.message() << std::endl;
        }
    }
}




bool extract_zip(const std::string& zipFile, const std::string& outputDir) {
	try {
		miniz_cpp::zip_file zip(zipFile.c_str());
		for(auto &fileInfo : zip.infolist()) {
			/*
			std::cout << "Unzipping " << outputDir << "/" << fileInfo.filename << " ";
			std::cout << std::setfill('0')
					  << fileInfo.date_time.year << '-'
					  << std::setw(2) << fileInfo.date_time.month << '-'
					  << std::setw(2) << fileInfo.date_time.day << ' '
					  << std::setw(2) << fileInfo.date_time.hours << ':'
					  << std::setw(2) << fileInfo.date_time.minutes << ':'
					  << std::setw(2) << fileInfo.date_time.seconds << std::endl
					  << std::setfill(' ');
			std::cout << "-----------\n create_version " << fileInfo.create_version
					  << "\n extract_version " << fileInfo.extract_version
					  << "\n flag " << fileInfo.flag_bits
					  << "\n compressed_size " << fileInfo.compress_size
					  << "\n crc " << fileInfo.crc
					  << "\n compress_size " << fileInfo.compress_size
					  << "\n file_size " << fileInfo.file_size
					  << "\n extra " << fileInfo.extra
					  << "\n comment " << fileInfo.comment
					  << "\n header_offset " << fileInfo.header_offset;
			std::cout << std::showbase << std::hex
					  << "\n internal_attr " << fileInfo.internal_attr
					  << "\n external_attr " << fileInfo.external_attr << "\n\n"
					  << std::noshowbase << std::dec;
			*/
			std::string fullPath = outputDir + "/" + fileInfo.filename;
			create_directories(fullPath);

			if (!(fileInfo.external_attr & 0x10)) { // If it's not a directory
				std::string buffer = zip.read(fileInfo);
				std::ofstream outFile;
				outFile.exceptions(std::ios::failbit | std::ios::badbit);
				outFile.open(fullPath, std::ios::binary);
				outFile.write(buffer.data(), buffer.size());
			}
		}
	} catch (const std::ios_base::failure& ex) {
		std::cout << "unable to unzip file " << zipFile << std::endl
				  << "message: " << ex.what() << std::endl
				  << "details: " << std::strerror(ex.code().value()) << std::endl;
	} catch (const std::exception &ex) {
		std::cout << "Error: unable to unzip file " << zipFile << " (exception: " << ex.what() << ")" << std::endl;
		return false;
	}
	return true;
}




bool decode_score_dat(const std::string& openPath, const std::string& writePath) {
	std::cout << "open " << openPath << " and then write to " << writePath << "\n";

	try	{
		std::ifstream inFile;
		inFile.exceptions(std::ios::failbit | std::ios::badbit);
		inFile.open(openPath.c_str(), std::ios::binary);
		std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());

		ScoreFileCrypter theTool;
		std::cout << "file size " << buffer.size() << "\n";

		theTool.CryptBuffer(buffer);

		std::vector<char> xmlBuffer = EzGz::IGzFile<>({buffer.data(), buffer.size()}).readAll();

		size_t uncompressedSize = xmlBuffer.size();
		std::cout << "decompressed Size " << uncompressedSize << "\n";

		std::ofstream xmlFile;
		xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
		xmlFile.open(writePath, std::ios::binary);
		xmlFile.write(xmlBuffer.data(), xmlBuffer.size());
	} catch (const std::ios_base::failure& ex) {
		std::cout << "unable to decode " << openPath << std::endl
				  << "message: " << ex.what() << std::endl
				  << "details: " << std::strerror(ex.code().value()) << std::endl;
	} catch (const std::exception &ex) {
		std::cout << "unable to decode " << openPath << " (exception: " << ex.what() << ")" << std::endl;
		return false;
	}

	return true;
}


int main(int argc, char* argv[]) {
	
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <musx file>" << std::endl;
		return 1;
	}

	std::string zipFilePath = argv[1];
	std::string outputDir = zipFilePath.substr(0, zipFilePath.find_last_of('.'));

	if (!extract_zip(zipFilePath, outputDir)) {
		std::cerr << "Failed to extract zip file " << zipFilePath << " to " << outputDir << std::endl;
		return 1;
	}

	std::cout << "Decrypting and expanding score.dat\n";
	std::string datOpen = outputDir + "/score.dat";
	std::string datGud = outputDir + "/score.enigmaxml";
	if (!decode_score_dat(datOpen, datGud)) {
		std::cerr << "BONK! we failed";
		return 1;
	}



	std::cout << "Extraction complete!" << std::endl;
	return 0; 
}
