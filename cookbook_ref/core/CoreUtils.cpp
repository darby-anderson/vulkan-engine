//
// Created by darby on 5/30/2024.
//

#include <cstring>
#include <vector>
#include <string>
#include <fstream>

namespace Fairy::Core {

    int endsWith(const char* s, const char* part) {
        return (strstr(s, part) - s) == (strlen(s) - strlen(part));
    }

    std::vector<char> readFile(const std::string& filePath, bool isBinary) {

        std::ios_base::openmode mode = std::ios::ate;
        if(isBinary) {
            mode |= std::ios::binary;
        }

        std::ifstream  file(filePath, mode);
        if(!file.is_open()) {
            throw std::runtime_error("failed to open file at: " + filePath);
        }

        size_t fileSize = (size_t)file.tellg();
        if(!isBinary) {
            fileSize += 1; // for null char terminator
        }

        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();
        if(!isBinary) {
            buffer[buffer.size() - 1] = '\0';
        }

        return buffer;
    }


}

