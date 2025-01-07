//
// Created by darby on 6/4/2024.
//

#pragma once

#include <glslang/Public/ShaderLang.h>
#include <vector>
#include "Context.hpp"
#include "RenderPass.hpp"

namespace Fairy::Graphics {

class ShaderModule final {

public:
    explicit ShaderModule(const Context* context, const std::string& filePath, VkShaderStageFlagBits stages,
                          const std::string& name);

    explicit ShaderModule(const Context* context, const std::string& filePath, const std::string& entryPoint,
                          VkShaderStageFlagBits stages, const std::string& name);

    std::vector<char> glslToSpirv(const std::vector<char>& data, EShLanguage shaderStage, const std::string& shaderDir,
                                  const char* entryPoint);

    void printShader(const std::vector<char>& data);

    EShLanguage shaderStageFromFileName(const char* fileName);

    void createShaderFromFile(const std::string& filePath, const std::string& entryPoint, const std::string& name);

    void createShader(const std::vector<char>& spirv, const std::string& entryPoint, const std::string& name);

    // std::shared_ptr<RenderPass> createRenderPasS(Vk);

    VkShaderStageFlagBits vkShaderStageFlags() const { return vkStageFlags_; }

    VkShaderModule vkShaderModule() const { return shaderModule_; }

    const std::string& entryPoint() const { return entryPoint_; }

private:

    const Context* context_;
    std::string entryPoint_;
    VkShaderStageFlagBits vkStageFlags_;
    VkShaderModule shaderModule_;

    std::string name_;

};

}