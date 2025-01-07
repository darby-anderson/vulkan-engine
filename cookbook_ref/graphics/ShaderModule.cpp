//
// Created by darby on 6/4/2024.
//

#include "ShaderModule.hpp"
#include "CoreUtils.hpp"
#include "VulkanUtils.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_reflect.h>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <valarray>
#include <filesystem>

namespace {
static constexpr uint32_t MAX_DESC_BINDLESS = 1000;

class CustomIncluder final : public glslang::TShader::Includer {
public:
    explicit CustomIncluder(const std::string& shaderDir) : shaderDirectory(shaderDir) {}

    ~CustomIncluder() = default;

    IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t inclusionDepth) override {
        return nullptr;
    }

    IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth) override {
        std::string fullPath = shaderDirectory + "/" + headerName;
        std::ifstream fileStream(fullPath, std::ios::in);
        if (!fileStream.is_open()) {
            std::string errMsg = "Failed to open included file: ";
            errMsg.append(headerName);
            return nullptr;
        }

        std::stringstream fileContent;
        fileContent << fileStream.rdbuf();
        fileStream.close();

        char* content = new char[fileContent.str().length() + 1];
        strncpy(content, fileContent.str().c_str(), fileContent.str().length());
        content[fileContent.str().length()] = '\0';

        return new IncludeResult(headerName, content, fileContent.str().length(), nullptr);
    }

    void releaseInclude(IncludeResult* result) override {
        if(result) {
            delete result;
        }
    }

private:
    std::string shaderDirectory;
};
}

namespace Fairy::Graphics {

ShaderModule::ShaderModule(const Context* context, const std::string& filePath, VkShaderStageFlagBits stages,
                           const std::string& name) : ShaderModule(context, filePath, "main", stages, name) {}

ShaderModule::ShaderModule(const Context* context, const std::string& filePath, const std::string& entryPoint,
                           VkShaderStageFlagBits stages,
                           const std::string& name) : context_(context), entryPoint_(entryPoint),
                                                      vkStageFlags_(stages), name_(name) {
    createShaderFromFile(filePath, entryPoint, name);
}

void ShaderModule::createShaderFromFile(const std::string& filePath, const std::string& entryPoint,
                                        const std::string& name) {
    std::vector<char> spirv;
    const bool isBinary = Fairy::Core::endsWith(filePath.c_str(), ".spv");
    std::vector<char> fileData = Fairy::Core::readFile(filePath, isBinary);
    std::filesystem::path file(filePath);

    if (isBinary) {
        spirv = std::move(fileData);
    } else {
        spirv = glslToSpirv(fileData, shaderStageFromFileName(filePath.c_str()),
                            file.parent_path().string(), entryPoint.c_str());
    }

    createShader(spirv, entryPoint, name);
}

void
ShaderModule::createShader(const std::vector<char>& spirv, const std::string& entryPoint, const std::string& name) {
    const VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv.size(),
        .pCode = (const uint32_t*) (spirv.data())
    };

    VK_CHECK(vkCreateShaderModule(context_->device(), &smci, nullptr, &shaderModule_));
}

EShLanguage ShaderModule::shaderStageFromFileName(const char* fileName) {
    if (Fairy::Core::endsWith(fileName, ".vert")) {
        return EShLangVertex;
    } else if (Fairy::Core::endsWith(fileName, ".frag")) {
        return EShLangFragment;
    } else if (Fairy::Core::endsWith(fileName, ".comp")) {
        return EShLangCompute;
    } else if (Fairy::Core::endsWith(fileName, ".rgen")) {
        return EShLangRayGen;
    } else if (Fairy::Core::endsWith(fileName, ".rmiss")) {
        return EShLangMiss;
    } else if (Fairy::Core::endsWith(fileName, ".rchit")) {
        return EShLangClosestHit;
    } else if (Fairy::Core::endsWith(fileName, ".rahit")) {
        return EShLangAnyHit;
    } else {
        ASSERT(false, "Add if/else for GLSL stage");
    }

    return EShLangVertex;
}

void ShaderModule::printShader(const std::vector<char>& data) {
    uint32_t totalLines = std::count(data.begin(), data.end(), '\n');
    // We support up to 9,999 lines
    const uint32_t maxNumSpaces = static_cast<uint32_t>(std::log10(totalLines)) + 1;

    uint32_t lineNum = 1;
    std::cout << lineNum;
    const auto numSpaces = maxNumSpaces - static_cast<uint32_t>(std::log10(totalLines) - 1);
    for (int i = 0; i < numSpaces; ++i) {
        std::cout << ' ';
    }
    for (char c: data) {
        std::cout << c;
        if (c == '\n') {
            ++lineNum;
            std::cout << lineNum;

            const auto numSpaces =
                maxNumSpaces - static_cast<uint32_t>(std::log10(totalLines) - 1);
            for (int i = 0; i < numSpaces; ++i) {
                std::cout << ' ';
            }
        }
    }
}

std::string removeUnnecessaryLines(const std::string& str) {
    std::istringstream iss(str);
    std::ostringstream oss;
    std::string line;

    while (std::getline(iss, line)) {
        if (line != "#extension GL_GOOGLE_include_directive : require" &&
            line.substr(0, 5) != "#line") {
            oss << line << '\n';
        }
    }
    return oss.str();
}

std::vector<char>
ShaderModule::glslToSpirv(const std::vector<char>& data, EShLanguage shaderStage, const std::string& shaderDir,
                          const char* entryPoint) {

    static bool glslangInitialized = false;

    if (!glslangInitialized) {
        glslang::InitializeProcess();
        glslangInitialized = true;
    }

    glslang::TShader tshader(shaderStage);
    const char* glslCStr = data.data();
    tshader.setStrings(&glslCStr, 1);

    glslang::EshTargetClientVersion clientVersion = glslang::EShTargetVulkan_1_3;
    glslang::EShTargetLanguageVersion langVersion = glslang::EShTargetSpv_1_3;

    tshader.setEnvInput(glslang::EShSourceGlsl, shaderStage, glslang::EShClientVulkan, 460);
    tshader.setEnvClient(glslang::EShClientVulkan, clientVersion);
    tshader.setEnvTarget(glslang::EShTargetSpv, langVersion);

    tshader.setEntryPoint(entryPoint);
    tshader.setSourceEntryPoint(entryPoint);

    const TBuiltInResource* resources = GetDefaultResources();
    const EShMessages messages = static_cast<EShMessages>(
        EShMsgDefault | EShMsgSpvRules |
        EShMsgVulkanRules | EShMsgDebugInfo |
        EShMsgReadHlsl
    );
    CustomIncluder includer(shaderDir);

    std::string preprocessedGLSL;
    if (!tshader.preprocess(resources, 460, ENoProfile, false, false,
                            messages, &preprocessedGLSL, includer)) {
        std::cout << "Preprocessing failed for shader: " << name_ << std::endl;
        printShader(data);
        std::cout << std::endl;
        std::cout << tshader.getInfoLog() << std::endl;
        std::cout << tshader.getInfoDebugLog() << std::endl;
        ASSERT(false, "Error occured");
        return std::vector<char>();
    }

    preprocessedGLSL = removeUnnecessaryLines(preprocessedGLSL);

    const char* preprocessesGLSLStr = preprocessedGLSL.c_str();
    tshader.setStrings(&preprocessesGLSLStr, 1);

    if (!tshader.parse(resources, 460, false, messages)) {
        std::cout << "Parsing failed for shader: " << name_ << std::endl;
        std::cout << tshader.getInfoLog() << std::endl;
        std::cout << tshader.getInfoDebugLog() << std::endl;
        ASSERT(false, "parse failed");
    }

    glslang::SpvOptions options;
    // Debugging V
    tshader.setDebugInfo(true);
    options.generateDebugInfo = true;
    options.disableOptimizer = true;
    options.optimizeSize = false;
    options.stripDebugInfo = false;
    options.emitNonSemanticShaderDebugSource = true;
    // Debugging ^

    glslang::TProgram program;
    program.addShader(&tshader);
    if (!program.link(messages)) {
        std::cout << "Linking failed for shader: " << name_ << std::endl;
        std::cout << program.getInfoLog() << std::endl;
        std::cout << program.getInfoDebugLog() << std::endl;
        ASSERT(false, "link failed");
    }

    std::vector<uint32_t> spirvData;
    spv::SpvBuildLogger spvLogger;
    glslang::GlslangToSpv(*program.getIntermediate(shaderStage), spirvData, &spvLogger, &options);

    std::vector<char> byteCode;
    byteCode.resize(spirvData.size() * sizeof(uint32_t) / sizeof(char));
    std::memcpy(byteCode.data(), spirvData.data(), byteCode.size());

    return byteCode;
}

}
