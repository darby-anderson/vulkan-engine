//
// Created by darby on 8/10/2024.
//

#pragma once

#if defined(_WIN32)

#include "glm/glm.hpp"
#include "Context.hpp"

namespace Fairy::Application {

class ImguiManager {

public:
    ImguiManager(void* GLFWWindow, const Graphics::Context& context, VkCommandBuffer commandBuffer,
                 VkRenderPass renderPass, VkSampleCountFlagBits msaaSamples);

    // used for dynamic rendering
    ImguiManager(void* GLFWWindow, const Graphics::Context& context, VkCommandBuffer commandBuffer,
                 const VkFormat swapchainFormat, VkSampleCountFlagBits msaaSamples);

    ~ImguiManager();

    void frameBegin();
    void createMenu();
    void createDummyText();

    void createCameraPosition(glm::vec3 pos);
    glm::vec3 cameraPosition();

    void createCameraDir(glm::vec3 dir);
    glm::vec3 cameraDir();

    void createCameraUpDir(glm::vec3 up);
    glm::vec3 cameraUpDir();

    void createLightPos(glm::vec3 pos);
    glm::vec3 lightPosValue();

    void createLightDir(glm::vec3 dir);
    glm::vec3 lightDirValue();

    void createLightUpDir(glm::vec3 dir);
    glm::vec3 lightUpDirValue();

    void createLightColor(glm::vec3 col);
    glm::vec3 lightColorValue();

    void createAmbientColor(glm::vec3 col);
    glm::vec3 ambientColorValue();

    void setDisplayShadowMapTexture(bool val);
    bool displayShadowMapTexture();

    void frameEnd();
    void recordCommands(VkCommandBuffer commandBuffer);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;

    VkDescriptorPool  descriptorPool_ = VK_NULL_HANDLE;

};

}

#endif
