//
// Created by darby on 8/10/2024.
//

#include "ImguiManager.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

namespace Fairy::Application {

static void checkVkResult(VkResult err) {
    if (err == 0) return;
    std::cerr << "[imgui - Vulkan] Error";
}

Fairy::Application::ImguiManager::ImguiManager(void* GLFWWindow, const Fairy::Graphics::Context& context,
                                               VkCommandBuffer commandBuffer, VkRenderPass renderPass,
                                               VkSampleCountFlagBits msaaSamples) : device_(context.device()),
                                                                                    renderPass_(renderPass) {

    ImGui_ImplVulkan_LoadFunctions([](const char* name, void*) {
        return vkGetInstanceProcAddr(volkGetLoadedInstance(), name);
    });

    const int numElements = 500;
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER,                numElements},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numElements},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          numElements},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   numElements},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         numElements},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, numElements},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       numElements},
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = numElements * (uint32_t) IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = (uint32_t) IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(GLFWWindow), true);

    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance = context.instance(),
        .PhysicalDevice = context.physicalDevice().getPhysicalDevice(),
        .Device = device_,
        .QueueFamily = context.physicalDevice().getGraphicsFamilyIndex().value(),
        .Queue = context.graphicsQueue(),
        .PipelineCache = VK_NULL_HANDLE,
        .DescriptorPool = descriptorPool_,
        .MinImageCount = context.swapchain()->imageCount(),
        .MSAASamples = msaaSamples,
        .Allocator = nullptr,
        .CheckVkResultFn = &checkVkResult
    };

    if (!ImGui_ImplVulkan_Init(&initInfo, renderPass_)) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        ASSERT(false, "Could not initialize imgui");
    }

    auto font = ImGui::GetIO().Fonts->AddFontDefault();

    auto result = ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    ASSERT(result, "Error creating ImGui fonts.");

}

Fairy::Application::ImguiManager::ImguiManager(void* GLFWWindow, const Graphics::Context& context,
                                               VkCommandBuffer commandBuffer, const VkFormat swapchainFormat,
                                               VkSampleCountFlagBits msaaSamples) : device_(context.device()) {
    ImGui_ImplVulkan_LoadFunctions([](const char* name, void*) {
        return vkGetInstanceProcAddr(volkGetLoadedInstance(), name);
    });

    const int numElements = 500;
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER,                numElements},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numElements},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          numElements},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   numElements},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         numElements},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, numElements},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, numElements},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       numElements},
    };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = numElements * (uint32_t) IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = (uint32_t) IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(GLFWWindow), true);

    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance = context.instance(),
        .PhysicalDevice = context.physicalDevice().getPhysicalDevice(),
        .Device = device_,
        .QueueFamily = context.physicalDevice().getGraphicsFamilyIndex().value(),
        .Queue = context.graphicsQueue(),
        .PipelineCache = VK_NULL_HANDLE,
        .DescriptorPool = descriptorPool_,
        .MinImageCount = context.swapchain()->imageCount(),
        .ImageCount = context.swapchain()->imageCount(),
        .MSAASamples = msaaSamples,
        .UseDynamicRendering = true,
        .ColorAttachmentFormat = swapchainFormat,
        .Allocator = nullptr,
        .CheckVkResultFn = &checkVkResult
    };

    if (!ImGui_ImplVulkan_Init(&initInfo, renderPass_)) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        ASSERT(false, "Could not initialize imgui");
    }

    auto font = ImGui::GetIO().Fonts->AddFontDefault();

    auto result = ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    ASSERT(result, "Error creating ImGui fonts.");

}

ImguiManager::~ImguiManager() {
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImguiManager::frameBegin() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImguiManager::createMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                // nothing
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void ImguiManager::createDummyText() {
    static float val = .2f;
    ImGui::Text("Hello World!");
    ImGui::SliderFloat("Float", &val, 0.0f, 1.0f);
}

static float cameraPos[3] = {0.0f, 0.0f, 0.0f};

void ImguiManager::createCameraPosition(glm::vec3 pos) {
    cameraPos[0] = pos.x;
    cameraPos[1] = pos.y;
    cameraPos[2] = pos.z;

    ImGui::SliderFloat3("Camera Pos", &cameraPos[0], -100.0f, 100.0f);
}

glm::vec3 ImguiManager::cameraPosition() {
    return glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]);
}

static float cameraDirection[3] = {0.0f, 0.0f, 0.0f};

void ImguiManager::createCameraDir(glm::vec3 dir) {
    cameraDirection[0] = dir.x;
    cameraDirection[1] = dir.y;
    cameraDirection[2] = dir.z;

    ImGui::SliderFloat3("Camera Euler angles", &cameraDirection[0], -360.0f, 360.0f);
}

glm::vec3 ImguiManager::cameraDir() {
    return glm::vec3(cameraDirection[0], cameraDirection[1], cameraDirection[2]);
}

static float cameraUpVar[3] = {0.0f, 1.0f, 0.0f};

void ImguiManager::createCameraUpDir(glm::vec3 dir) {
    cameraUpVar[0] = dir.x;
    cameraUpVar[1] = dir.y;
    cameraUpVar[2] = dir.z;

    ImGui::SliderFloat3("Camera Up", &cameraUpVar[0], -1.0f, 1.0f);
}

glm::vec3 ImguiManager::cameraUpDir() {
    return glm::vec3(cameraUpVar[0], cameraUpVar[1], cameraUpVar[2]);
}

static float lightPos[3] = {0.0f, 0.0f, 0.0f};

void ImguiManager::createLightPos(glm::vec3 pos) {
    lightPos[0] = pos.x;
    lightPos[1] = pos.y;
    lightPos[2] = pos.z;

    ImGui::SliderFloat3("Light Pos", &lightPos[0], -10.0f, 10.0f);
}

glm::vec3 ImguiManager::lightPosValue() {
    return glm::vec3(lightPos[0], lightPos[1], lightPos[2]);
}

static float lightDir[3] = {0.0f, 0.0f, 0.0f};

void ImguiManager::createLightDir(glm::vec3 dir) {
    lightDir[0] = dir.x;
    lightDir[1] = dir.y;
    lightDir[2] = dir.z;

    ImGui::SliderFloat3("Light Euler Angles", &lightDir[0], -360.0f, 360.0f);
}

glm::vec3 ImguiManager::lightDirValue() {
    return glm::vec3(lightDir[0], lightDir[1], lightDir[2]);
}

static float lightUpDir[3] = {0.0f, 1.0f, 0.0f};

void ImguiManager::createLightUpDir(glm::vec3 dir) {
    lightUpDir[0] = dir.x;
    lightUpDir[1] = dir.y;
    lightUpDir[2] = dir.z;

    ImGui::SliderFloat3("Light Up Dir", &lightUpDir[0], -1.0f, 1.0f);
}

glm::vec3 ImguiManager::lightUpDirValue() {
    return glm::vec3(lightUpDir[0], lightUpDir[1], lightUpDir[2]);
}

static float lightColor[3] = {0.0f, 0.0f, 0.0f};

void ImguiManager::createLightColor(glm::vec3 col) {
    lightColor[0] = col.x;
    lightColor[1] = col.y;
    lightColor[2] = col.z;

    ImGui::SliderFloat3("Light Color", &lightColor[0], 0.0f, 1.0f);
}

glm::vec3 ImguiManager::lightColorValue() {
    return {lightColor[0], lightColor[1], lightColor[2]};
}

static float ambientColor[3] = {0.0f, 0.0f, 0.0f};

void ImguiManager::createAmbientColor(glm::vec3 col) {
    ambientColor[0] = col.x;
    ambientColor[1] = col.y;
    ambientColor[2] = col.z;

    ImGui::SliderFloat3("Ambient Color", &ambientColor[0], 0.0f, 1.0f);
}

glm::vec3 ImguiManager::ambientColorValue() {
    return {ambientColor[0], ambientColor[1], ambientColor[2]};
}

static bool displayShadowMap = false;

void ImguiManager::setDisplayShadowMapTexture(bool val) {
    displayShadowMap = val;
    ImGui::Checkbox("Display shadowmap", &displayShadowMap);
}

bool ImguiManager::displayShadowMapTexture() {
    return displayShadowMap;
}

void ImguiManager::frameEnd() {
    ImGui::EndFrame();
    ImGui::Render();
}

void ImguiManager::recordCommands(VkCommandBuffer commandBuffer) {
    ImDrawData* drawData = ImGui::GetDrawData();

    if (drawData != nullptr) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
}

}
