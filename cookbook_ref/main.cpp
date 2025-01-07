//
// Created by darby on 5/28/2024.
//
#include <iostream>
#include <filesystem>

#define VOLK_IMPLEMENTATION
#define VK_USE_PLATFORM_WIN32_KHR
#include "volk.h"

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "VulkanUtils.hpp"
#include "Context.hpp"
#include "Window.hpp"
#include "Framebuffer.hpp"
#include "Camera.hpp"
#include "RingBuffer.hpp"
#include "Model.hpp"
#include "GLTFLoader.hpp"
#include "Pipeline.hpp"
#include "ImguiManager.hpp"
#include "DynamicRendering.hpp"
#include "Input.hpp"
#include "Keys.hpp"

#include "glm/ext.hpp"
#include "glm/gtx/string_cast.hpp"

using namespace Fairy;

Application::Camera camera(glm::vec3(-.9f, 2.f, 2.f));

int main() {
    std::cout << "Hello World" << std::endl;

    Application::WindowConfiguration windowConfig {
        .width = 800,
        .height = 600,
    };

    Application::Window window {};
    window.init(windowConfig);

    std::cout << "Hello World 2" << std::endl;

    VkResult volkInitResult = volkInitialize();

    const std::vector<std::string> requestedLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    Graphics::Context::enableDefaultFeatures();
    // Graphics::Context::enabledBufferDeviceAddressFeature(); no debug data for RenderDoc with this feature
    Graphics::Context::enableDynamicRenderingFeature();

    Graphics::Context context(window.get_win32_window(),
        requestedLayers,
        {
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
        },
        {
            VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_MEMORY_BUDGET_EXTENSION_NAME
        },
        VK_QUEUE_GRAPHICS_BIT,
        true,
        "Main Context");
    std::cout << "Context Created" << std::endl;

    const VkExtent2D extent = context.physicalDevice().surfaceCapabilities().minImageExtent;

    const VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    context.createSwapchain(swapchainFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR, VK_PRESENT_MODE_FIFO_KHR, extent);
    static const uint32_t framesInFlight = (uint32_t)context.swapchain()->imageCount();
    std::cout << "Swapchain Created w/ " << framesInFlight << " frames in flight." << std::endl;


    // Create command manager
    auto commandMgr = context.createGraphicsCommandQueue(context.swapchain()->imageCount(), context.swapchain()->imageCount());
    std::cout << "Graphics Command Queue Created" << std::endl;

    // START TRACY
#if defined(VK_EXT_calibrated_timestamps)
    TracyVkCtx tracyCtx_ = TracyVkContextCalibrated(
        context.physicalDevice().getPhysicalDevice(), context.device(),
        context.graphicsQueue(), commandMgr.getCmdBuffer(),
        vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT);
#else
    TracyVkCtx tracyCtx_ =
      TracyVkContext(context.physicalDevice().getPhysicalDevice(), context.device(),
                     context.graphicsQueue(), commandMgr.getCmdBuffer());
#endif

    Application::UniformTransforms transform = {
        .model = glm::mat4(1.0f),
        .view = camera.viewMatrix(),
        .projection = camera.getProjectMatrix()
    };

    std::vector<std::shared_ptr<Graphics::Buffer>> buffers;
    std::vector<std::shared_ptr<Graphics::Texture>> textures;
    std::vector<std::shared_ptr<Graphics::Sampler>> samplers;

    Application::RingBuffer cameraRingBuffer(context.swapchain()->imageCount(), context, sizeof(Application::UniformTransforms));
    cameraRingBuffer.buffer(0)->copyDataToBuffer(&transform, sizeof(Application::UniformTransforms));
    cameraRingBuffer.buffer(1)->copyDataToBuffer(&transform, sizeof(Application::UniformTransforms));
    cameraRingBuffer.buffer(2)->copyDataToBuffer(&transform, sizeof(Application::UniformTransforms));

    uint32_t numMeshes = 0;
    std::shared_ptr<Graphics::Model> fox;

    const auto resourcesDir = std::filesystem::current_path() / "resources/";

    // Load Model
    {
        const auto commandBuffer = commandMgr->beginCmdBuffer();
        {
            samplers.emplace_back(context.createSampler(
                VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f, "default sampler"
            ));

            ZoneScopedN("Model Load");
            Application::GLTFLoader gltfLoader;
            // fox = gltfLoader.load((resourcesDir / "models/Fox/glTF/Fox.gltf").string());
            fox = gltfLoader.load((resourcesDir / "models/Box/glTF/Box.gltf").string());
            TracyVkZone(tracyCtx_, commandBuffer, "Model upload");
            Fairy::Application::convertModelToOneMeshPerBuffer_VerticesIndicesMaterialsAndTextures(context,
                                                                                                   *commandMgr.get(),
                                                                                                   commandBuffer,
                                                                                                   *fox.get(), buffers,
                                                                                                   textures, samplers);

            if (textures.size() == 0) {
                textures.push_back(context.createTexture(
                    VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VkExtent3D{
                        .width = static_cast<uint32_t>(1),
                        .height = static_cast<uint32_t>(1),
                        .depth = static_cast<uint32_t>(1.0)
                    },
                    1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
                    "Empty Texture?"
                ));
            }

            numMeshes = buffers.size() >> 1; // divides buffers.size() by 2, since we have one vert buffer and one index buffer
        }

        TracyVkCollect(tracyCtx_, commandBuffer);
        commandMgr->endCmdBuffer(commandBuffer);

        const VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
        commandMgr->submitCmdBuffer(&submitInfo);
        commandMgr->waitUntilSubmitIsComplete();
    }

    std::cout << "Model loaded" << std::endl;

    // Check for image format support, from most desired to least
    std::vector<VkFormat> formatsToAttempt = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT
    };

    VkFormat depthStencilFormatToUse = VK_FORMAT_UNDEFINED;
    for(int i = 0; VkFormat format : formatsToAttempt) {
        auto result = Fairy::Graphics::checkImageFormatProperties(context, format, VK_IMAGE_TYPE_2D,  VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0);
        if(result == VK_SUCCESS) {
            depthStencilFormatToUse = format;
            break;
        } else {
            std::cout << "Can not use formatsToAttempt at index " << i << std::endl;
        }
    }

    ASSERT(depthStencilFormatToUse != VK_FORMAT_UNDEFINED, "Error finding suitable VkFormat for depth stencil");

    // Depth Texture
    auto depthTexture = context.createTexture(VK_IMAGE_TYPE_2D, depthStencilFormatToUse, 0, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                              {
                                                  .width = context.swapchain()->extent().width,
                                                  .height = context.swapchain()->extent().height,
                                                  .depth = 1,
                                              },
                                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
                                              "depth buffer");

    std::cout << "Depth texture created" << std::endl;


    constexpr uint32_t CAMERA_SET = 0;
    constexpr uint32_t TEXTURES_AND_SAMPLER_SET = 1;
    constexpr uint32_t BINDING_0 = 0;
    constexpr uint32_t BINDING_1 = 1;

    std::cout << "About to load shaders" << std::endl;

    const auto vertexShader = context.createShaderModule((resourcesDir / "shaders/bindfull/bindfull.vert").string(),
                                                         VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
    const auto fragmentShader = context.createShaderModule((resourcesDir / "shaders/bindfull/bindfull.frag").string(),
                                                         VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

    std::cout << "Shaders loaded" << std::endl;

    const std::vector<Fairy::Graphics::Pipeline::SetDescriptor> setLayout = {
        {
            .set_ = CAMERA_SET,
            .bindings_ = {
                VkDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT),
            }
        },
        {
            .set_ = TEXTURES_AND_SAMPLER_SET,
            .bindings_ = {
                VkDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
                VkDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
            }
        }
    };

    const VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(Fairy::Graphics::Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    const std::vector<std::pair<VkFormat, size_t>> vertexAttributesFormatAndOffset = {
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Fairy::Graphics::Vertex, pos)},
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Fairy::Graphics::Vertex, normal)},
        {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Fairy::Graphics::Vertex, tangent)},
        {VK_FORMAT_R32G32_SFLOAT, offsetof(Fairy::Graphics::Vertex, texCoord)},
        {VK_FORMAT_R32G32_SFLOAT, offsetof(Fairy::Graphics::Vertex, texCoord1)},
        {VK_FORMAT_R32_SINT, offsetof(Fairy::Graphics::Vertex, material)},
    };

    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

    for(uint32_t i = 0; i < vertexAttributesFormatAndOffset.size(); i++) {
        auto [format, offset] = vertexAttributesFormatAndOffset[i];
        vertexInputAttributes.push_back(VkVertexInputAttributeDescription {
            .location = i,
            .binding = 0,
            .format = format,
            .offset = uint32_t(offset)
        });
    }

    uint32_t texturesNotPresentSpecializationData = 0;

    const std::vector<VkSpecializationMapEntry> fragSpecializationMapEntries = {
        {
            .constantID = 0,
            .offset = 0,
            .size = sizeof(texturesNotPresentSpecializationData)
        }
    };

    Graphics::Pipeline::GraphicsPipelineDescriptor graphicsPipelineDescriptor {
        .sets_ = setLayout,
        .vertexShader_ = vertexShader,
        .fragmentShader_ = fragmentShader,
        .dynamicStates_ = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
        },
        .useDynamicRendering_ = true,
        .colorTextureFormats = { swapchainFormat },
        .depthTextureFormat = { depthTexture->format() },
        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .viewport = context.swapchain()->extent(),
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOperation = VK_COMPARE_OP_LESS,
        .vertexInputStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1u,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size()),
            .pVertexAttributeDescriptions = vertexInputAttributes.data(),
        },
        .fragmentSpecConstants_ = fragSpecializationMapEntries,
        .fragmentSpecializationData = &texturesNotPresentSpecializationData
    };

    // Pipeline Initialization
    auto pipelineWithTexture = context.createGraphicsPipeline(
        graphicsPipelineDescriptor, VK_NULL_HANDLE, "Pipeline w/ BaseColorTexture");

    // Set this constant to FALSE
    texturesNotPresentSpecializationData = 1;

    auto pipelineWithoutTexture = context.createGraphicsPipeline(
        graphicsPipelineDescriptor, VK_NULL_HANDLE, "Pipeline w/out BaseColorTexture");

    std::cout << "Pipelines created" << std::endl;

    pipelineWithTexture->allocateDescriptors({
        {.set_ = CAMERA_SET, .count_ = 3},
        {.set_ = TEXTURES_AND_SAMPLER_SET, .count_ = uint32_t(textures.size() + 1)}
    });

    pipelineWithoutTexture->allocateDescriptors({
         {.set_ = CAMERA_SET, .count_ = 3},
         {.set_ = TEXTURES_AND_SAMPLER_SET, .count_ = 1}
     });

    // We use 3 counts since we are using triple buffering
    pipelineWithTexture->bindBufferResource(CAMERA_SET, BINDING_0, 0, cameraRingBuffer.buffer(0), 0, sizeof(Application::UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    pipelineWithTexture->bindBufferResource(CAMERA_SET, BINDING_0, 1, cameraRingBuffer.buffer(1), 0, sizeof(Application::UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    pipelineWithTexture->bindBufferResource(CAMERA_SET, BINDING_0, 2, cameraRingBuffer.buffer(2), 0, sizeof(Application::UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    pipelineWithoutTexture->bindBufferResource(CAMERA_SET, BINDING_0, 0, cameraRingBuffer.buffer(0), 0, sizeof(Application::UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    pipelineWithoutTexture->bindBufferResource(CAMERA_SET, BINDING_0, 1, cameraRingBuffer.buffer(1), 0, sizeof(Application::UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    pipelineWithoutTexture->bindBufferResource(CAMERA_SET, BINDING_0, 2, cameraRingBuffer.buffer(2), 0, sizeof(Application::UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Bind each texture to a separate index since we are bind-fully indexing
    for(uint32_t i = 0; i < textures.size(); i++) {
        pipelineWithTexture->bindTextureResource(TEXTURES_AND_SAMPLER_SET, BINDING_0, i, {
            textures.begin() + i, 1
        });

        pipelineWithTexture->bindSamplerResource(TEXTURES_AND_SAMPLER_SET, BINDING_1, i, {
            samplers.begin(), 1
        });
    }

    pipelineWithoutTexture->bindTextureResource(TEXTURES_AND_SAMPLER_SET, BINDING_0, 0, {
        textures.begin(), 1
    });

    pipelineWithoutTexture->bindSamplerResource(TEXTURES_AND_SAMPLER_SET, BINDING_1, 0, {
        samplers.begin(), 1
    });

    // DEBUG UI
    float r = 0.6f, g = 0.6f, b = 1.f;
    size_t frame = 0;
    size_t previousFrame = 0;
    const std::array<VkClearValue, 2> clearValues = {
        VkClearValue{.color = {r, g, b, 0.0f}},
        VkClearValue{.depthStencil = {1.0f}}
    };

    const glm::mat4 view = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.5f});
    auto time = glfwGetTime();

    std::unique_ptr<Application::ImguiManager> imguiMgr = nullptr;

    std::cout << "About to start running loop" << std::endl;

    TracyPlotConfig("Swapchain image index", tracy::PlotFormatType::Number, true, false, tracy::Color::Aqua);

    Application::Input input{};
    input.init(&window);

    while(!window.should_exit()) {
        // Start of frame actions
        const auto now = glfwGetTime();
        const auto delta = now - time;

        if(delta > 1) {
            const auto fps = static_cast<double>(frame - previousFrame) / delta;
            std::cerr << "FPS: " << fps << std::endl;
            previousFrame = frame;
            time = now;
        }

        input.start_new_frame();
        if(input.is_key_down((Fairy::Application::KEY_A))) {
            camera.move(glm::vec3(-1.0, 0.0, 0.0), 1);
        }

        if(input.is_key_down((Fairy::Application::KEY_D))) {
            camera.move(glm::vec3(1.0, 0.0, 0.0), 1);
        }

        if(input.is_key_down((Fairy::Application::KEY_S))) {
            camera.move(glm::vec3(0.0, 0.0, -1.0), 1);
        }

        if(input.is_key_down((Fairy::Application::KEY_W))) {
            camera.move(glm::vec3(0.0, 0.0, 1.0), 1);
        }

        const auto swapchainTexture = context.swapchain()->acquireImage();
        const auto swapchainImageIndex = context.swapchain()->currentImageIndex();
        TracyPlot("Swapchain image index", (int64_t)swapchainImageIndex);

        const Graphics::DynamicRendering::AttachmentDescription colorAttachmentDesc {
            .imageView = swapchainTexture->view(),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearValues[0]
        };

        const Graphics::DynamicRendering::AttachmentDescription depthAttachmentDesc {
            .imageView = depthTexture->view(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = clearValues[1]
        };

        auto commandBuffer = commandMgr->beginCmdBuffer();

        Graphics::DynamicRendering::beginRenderingCmd(commandBuffer, swapchainTexture->image(),
                                                      0,
                                                      {{0, 0},
                                                       {
                                                          swapchainTexture->extent().width,
                                                          swapchainTexture->extent().height
                                                      }},
                                                      1, 0, {colorAttachmentDesc},
                                                      &depthAttachmentDesc, nullptr);

        // Dynamic States
        const VkViewport viewport = {
            .x = 0.0f,
            .y = static_cast<float>(context.swapchain()->extent().height),
            .width = static_cast<float>(context.swapchain()->extent().width),
            .height = static_cast<float>(context.swapchain()->extent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        const VkRect2D scissor = {
            .offset = {0, 0},
            .extent = context.swapchain()->extent()
        };

        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);

        // Uniforms update
        if(camera.isDirty()) {
            transform.view = camera.viewMatrix();
            camera.setNotDirty();
        }

        cameraRingBuffer.buffer()->copyDataToBuffer(&transform, sizeof(Application::UniformTransforms));

        for(uint32_t meshIdx = 0; meshIdx < numMeshes; meshIdx++) {
            Graphics::Material mat;
            if(fox->meshes[meshIdx].material != -1) {
                mat = fox->materials[fox->meshes[meshIdx].material];
            }

            auto pipeline = pipelineWithTexture;

            if(mat.baseColorTextureId == -1) {
                pipeline = pipelineWithoutTexture;
            }

            auto vertexBufferIndex = meshIdx * 2;
            auto indexBufferIndex = meshIdx * 2 + 1;

            pipeline->bind(commandBuffer);

            pipeline->bindVertexBuffer(commandBuffer, buffers[vertexBufferIndex]->vkBuffer());
            pipeline->bindIndexBuffer(commandBuffer, buffers[indexBufferIndex]->vkBuffer());

            pipeline->bindDescriptorSets(
                commandBuffer,
                {
                    { .set = CAMERA_SET, .bindIdx = (uint32_t)swapchainImageIndex },
                    { .set = TEXTURES_AND_SAMPLER_SET, .bindIdx = uint32_t(mat.baseColorTextureId == -1 ? 0 : mat.baseColorTextureId) }
                });

            pipeline->updateDescriptorSets(); // is this the issue??

            const auto vertexCount = buffers[indexBufferIndex]->size() / sizeof(uint32_t);

            vkCmdDrawIndexed(commandBuffer, vertexCount, 1, 0, 0, 0);
        }

        Graphics::DynamicRendering::endRenderCmd(commandBuffer, swapchainTexture->image());

        TracyVkCollect(tracyCtx_, commandBuffer);

        commandMgr->endCmdBuffer(commandBuffer);

        const VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
        commandMgr->submitCmdBuffer(&submitInfo);
        commandMgr->moveToNextCmdBuffer();

        context.swapchain()->present();
        glfwPollEvents();

        frame++;

        cameraRingBuffer.moveToNextBuffer();

        FrameMarkNamed("main frame");
    }

    vkDeviceWaitIdle(context.device());

    return 0;
}