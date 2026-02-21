#include "Engine.hpp"

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

Engine::Engine() {
    createWindow();
    createInstance();
    createSurface();
    createDevice();
    createSwapchain();
    createMVP();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSets();
    createGfxPipelineLayout();
    createGfxPipeline();
    createCommandPool(gfxCmdPool, queueFamilyIndices.graphicsFamily.value());
    createCommandPool(presentCmdPool, queueFamilyIndices.presentFamily.value());
    createCommandPool(transferCmdPool, queueFamilyIndices.transferFamily.value());
    createVertexBuffer();
    createIndexBuffer();
}
Engine::~Engine() {
    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, MVPBuffers[i], nullptr);
        vkFreeMemory(device, MVPBufferMemory[i], nullptr);
    }
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyCommandPool(device, transferCmdPool, nullptr);
    vkDestroyCommandPool(device, presentCmdPool, nullptr);
    vkDestroyCommandPool(device, gfxCmdPool, nullptr);
    vkDestroyPipeline(device, gfxPipeline, nullptr);
    vkDestroyPipelineLayout(device, gfxPipelineLayout, nullptr);
    vkDestroyDescriptorPool(device, gfxDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, gfxDescriptorSetLayout, nullptr);
    cleanupSwapchain();
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Engine::run() {
    std::vector<VkCommandBuffer> gfxCmdBuffers(MAX_FRAMES_IN_FLIGHT);
    for (auto& cmdBuffer: gfxCmdBuffers) {
        cmdBuffer = allocateCommandBuffer(gfxCmdPool);
    }
    std::vector<VkSemaphore> imageAvailable(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderingDone(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkFence> cmdBufferReady(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        createSemaphore(imageAvailable[i]);
        createSemaphore(renderingDone[i]);
        createFence(cmdBufferReady[i], VK_FENCE_CREATE_SIGNALED_BIT);
    }
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        vkWaitForFences(device, 1, &cmdBufferReady[currFrame], VK_TRUE, ~0ull);
        
        uint32_t imageIndex;
        VkResult res = vkAcquireNextImageKHR(device, swapchain, ~0ull, imageAvailable[currFrame], VK_NULL_HANDLE, &imageIndex);
        if (res==VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            continue;
        } else if (res!=VK_SUBOPTIMAL_KHR && res!=VK_SUCCESS) {
            throw std::runtime_error("VK Error: cannot retrieve swapchain image");
        }
        
        vkResetFences(device, 1, &cmdBufferReady[currFrame]);

        vkResetCommandBuffer(gfxCmdBuffers[currFrame], 0);

        updateMVP(currFrame);
        recordCmdBuffer(gfxCmdBuffers[currFrame], imageIndex);

        VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = gfxCmdBuffers[currFrame],
            .deviceMask = 0,
        };
        VkSemaphoreSubmitInfo imageAvailableSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = imageAvailable[currFrame],
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };
        VkSemaphoreSubmitInfo renderingDoneSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = renderingDone[currFrame],
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };
        VkSubmitInfo2 submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &imageAvailableSubmitInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdBufferSubmitInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &renderingDoneSubmitInfo
        };
        VK_CHECK(vkQueueSubmit2(gfxQueue, 1, &submitInfo, cmdBufferReady[currFrame]));

        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderingDone[currFrame],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex,
            .pResults = nullptr,
        };
        res = vkQueuePresentKHR(gfxQueue, &presentInfo);
        if (res==VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
        } else if (res!=VK_SUCCESS && res!=VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("VK Error: cannot present");
        }
        
        currFrame=(currFrame+1)%MAX_FRAMES_IN_FLIGHT;
    }
    vkQueueWaitIdle(gfxQueue);
    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAvailable[i], nullptr);
        vkDestroySemaphore(device, renderingDone[i], nullptr);
        vkDestroyFence(device, cmdBufferReady[i], nullptr);
    }
}
void Engine::recordCmdBuffer(VkCommandBuffer& cmdBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo cmdBufferBegin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    vkBeginCommandBuffer(cmdBuffer, &cmdBufferBegin);
    {
        VkRenderingAttachmentInfo colorAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = swapchainImageViews[imageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{
                .color{
                    {0.0f, 0.0f, 0.0f}
                },
            }
        };
        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea{
                .offset{
                    .x = 0,
                    .y = 0,
                },
                .extent = swapchainExtent
            },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = nullptr,
            .pStencilAttachment = nullptr
        };
        vkCmdBeginRendering(cmdBuffer, &renderingInfo);
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
            vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipelineLayout, 0, 1, &gfxDescriptorSets[currFrame], 0, nullptr);

            VkViewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)swapchainExtent.width,
                .height = (float)swapchainExtent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            VkRect2D scissor{
                .offset{
                    .x = 0,
                    .y = 0
                },
                .extent = swapchainExtent
            };
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            vkCmdPushConstants(cmdBuffer, gfxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
            
            vkCmdDrawIndexed(cmdBuffer, (uint32_t)indices.size(), 1, 0, 0, 0);
        }
        vkCmdEndRendering(cmdBuffer);
    }
    vkEndCommandBuffer(cmdBuffer);
}

void Engine::createWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);
    glfwSetKeyCallback(window, key_callback);
}
void Engine::createInstance() {
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Vulkan application",
        .applicationVersion = VK_VERSION_1_0,
        .pEngineName = "Engine name",
        .engineVersion = VK_VERSION_1_0,
        .apiVersion = VK_API_VERSION_1_4
    };
    if (!checkInstanceExtensionsSupport() || !checkInstanceLayersSupport()) {
        throw std::runtime_error("VK Error: instance layers or extensions not supported");
    }
    VkInstanceCreateInfo instanceCI{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = (uint32_t)instanceLayers.size(),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = (uint32_t)instanceExtensions.size(),
        .ppEnabledExtensionNames = instanceExtensions.data()
    };
    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &instance));
}
void Engine::createSurface() {
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}
void Engine::createDevice() {
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    for (const auto& dev: devices) {
        if (isDeviceSuitable(dev)) {
            pDevice = dev;
            break;
        }
    }
    if (pDevice==VK_NULL_HANDLE) {
        throw std::runtime_error("VK Error: no suitable devices found");
    }
    queueFamilyIndices = getQueueFamilyIndices(pDevice);

    std::set<uint32_t> uniqueQueueFamilyIndices = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.transferFamily.value(),
        queueFamilyIndices.presentFamily.value() 
    };
    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    float priority = 1.0f;
    for (uint32_t queueFamilyIndex: uniqueQueueFamilyIndices) {
        VkDeviceQueueCreateInfo queueCI{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &priority
        };
        queueCIs.push_back(queueCI);
    }

    VkPhysicalDeviceFeatures features{};
    features.multiDrawIndirect = VK_TRUE;
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = &features12;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    VkDeviceCreateInfo deviceCI{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features13,
        .flags = 0,
        .queueCreateInfoCount = (uint32_t)queueCIs.size(),
        .pQueueCreateInfos = queueCIs.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = (uint32_t)deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &features
    };
    VK_CHECK(vkCreateDevice(pDevice, &deviceCI, nullptr, &device));
    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &gfxQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.transferFamily.value(), 0, &transferQueue);
}
void Engine::createSwapchain() {
    SurfaceDetails surfaceDetails = getSurfaceDetails(pDevice);
    swapchainExtent = chooseSurfaceExtent(surfaceDetails.capabilities);
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(surfaceDetails.formats);
    swapchainFormat = surfaceFormat.format;
    uint32_t imageCount = surfaceDetails.capabilities.minImageCount+1;
    if (imageCount > surfaceDetails.capabilities.maxImageCount && surfaceDetails.capabilities.maxImageCount > 0) {
        imageCount = surfaceDetails.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCI{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = surfaceDetails.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = choosePresentMode(surfaceDetails.modes),
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr
    };
    std::set<uint32_t> uniqueQueueFamilyIndices = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.transferFamily.value(),
        queueFamilyIndices.presentFamily.value() 
    };
    if (uniqueQueueFamilyIndices.size() > 1) {
        std::vector<uint32_t> queueFamilies(uniqueQueueFamilyIndices.begin(), uniqueQueueFamilyIndices.end());
        swapchainCI.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCI.queueFamilyIndexCount = (uint32_t)uniqueQueueFamilyIndices.size();
        swapchainCI.pQueueFamilyIndices = queueFamilies.data();
    }
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    swapchainImageViews.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
    for (uint32_t i=0; i<imageCount; i++) {
        createImageView(swapchainImages[i], swapchainImageViews[i], VK_IMAGE_ASPECT_COLOR_BIT, swapchainFormat);
    }
}
void Engine::recreateSwapchain() {
    vkDeviceWaitIdle(device);
    cleanupSwapchain();
    createSwapchain();
}
void Engine::cleanupSwapchain() {
    for (auto& imageView: swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}
void Engine::createImageView(VkImage& image, VkImageView& imageView, VkImageAspectFlags aspectMask, VkFormat format) {
    VkImageViewCreateInfo imageViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components{
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange{
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
    };
    VK_CHECK(vkCreateImageView(device, &imageViewCI, nullptr, &imageView));
}
void Engine::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &descriptorSetLayoutBinding
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &gfxDescriptorSetLayout));
}
void Engine::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };
    VkDescriptorPoolCreateInfo descriptorPoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &gfxDescriptorPool));
}
void Engine::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, gfxDescriptorSetLayout);
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = gfxDescriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };
    gfxDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, gfxDescriptorSets.data()));

    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo descriptorBufferInfo{
            .buffer = MVPBuffers[i],
            .offset = 0,
            .range = sizeof(MVP)
        };
        VkWriteDescriptorSet writeDescriptroSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = gfxDescriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorBufferInfo,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(device, 1, &writeDescriptroSet, 0, nullptr);
    }
}
void Engine::createGfxPipelineLayout() {
    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &gfxDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &gfxPipelineLayout));
}
void Engine::createGfxPipeline() {
    std::vector<char> vertCode = readFile("../render.vert.spv");
    std::vector<char> fragCode = readFile("../render.frag.spv");
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    createShaderModule(vertCode, vertShaderModule);
    createShaderModule(fragCode, fragShaderModule);
    VkPipelineShaderStageCreateInfo vertShaderStageCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    VkPipelineShaderStageCreateInfo fragShaderStageCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCIs = {vertShaderStageCI, fragShaderStageCI};

    VkPipelineVertexInputStateCreateInfo vertexInputCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = (float)swapchainExtent.width,
        .height = (float)swapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissor{
        .offset{
            .x = 0,
            .y = 0
        },
        .extent = swapchainExtent
    };
    VkPipelineViewportStateCreateInfo viewportCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo msaaCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depthCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front{
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0,
            .writeMask = 0,
            .reference = 0
        },
        .back{
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0,
            .writeMask = 0,
            .reference = 0
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo colorBlendCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = 0.0f
    };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = (uint32_t)dynamicStates.size(),
        .pDynamicStates = dynamicStates.data()
    };

    VkPipelineRenderingCreateInfo renderingCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainFormat,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    VkGraphicsPipelineCreateInfo gfxPipelineCI{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .flags = 0,
        .stageCount = (uint32_t)shaderStageCIs.size(),
        .pStages = shaderStageCIs.data(),
        .pVertexInputState = &vertexInputCI,
        .pInputAssemblyState = &inputAssemblyCI,
        .pTessellationState = nullptr,
        .pViewportState = &viewportCI,
        .pRasterizationState = &rasterCI,
        .pMultisampleState = &msaaCI,
        .pDepthStencilState = &depthCI,
        .pColorBlendState = &colorBlendCI,
        .pDynamicState = &dynamicCI,
        .layout = gfxPipelineLayout,
        .renderPass = nullptr,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gfxPipelineCI, nullptr, &gfxPipeline));

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}
void Engine::createShaderModule(std::vector<char> code, VkShaderModule& shaderModule) {
    VkShaderModuleCreateInfo shaderModuleCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = (uint32_t)code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };
    VK_CHECK(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));
}
void Engine::createCommandPool(VkCommandPool& cmdPool, uint32_t queueFamilyIndex) {
    VkCommandPoolCreateInfo cmdPoolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT means the command buffer will be short-lived
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT means command buffers are allowed to be reset individually
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(device, &cmdPoolCI, nullptr, &cmdPool));
}
void Engine::createSemaphore(VkSemaphore& sem) {
    VkSemaphoreCreateInfo semCI{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VK_CHECK(vkCreateSemaphore(device, &semCI, nullptr, &sem));
}
void Engine::createFence(VkFence& fence, VkFenceCreateFlags flags) {
    VkFenceCreateInfo fenceCI{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags
    };
    VK_CHECK(vkCreateFence(device, &fenceCI, nullptr, &fence));
}
void Engine::createBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memProperties) {
    VkBufferCreateInfo bufferCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    VK_CHECK(vkCreateBuffer(device, &bufferCI, nullptr, &buffer));
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocateFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
        .deviceMask = 0
    };
    VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocateFlagsInfo,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = getMemoryTypeIndex(memRequirements.memoryTypeBits, memProperties)
    };
    VK_CHECK(vkAllocateMemory(device, &allocateInfo, nullptr, &bufferMemory));  

    VK_CHECK(vkBindBufferMemory(device, buffer, bufferMemory, 0));
}
void Engine::createVertexBuffer() {
    vertexBufferSize = sizeof(vertices[0])*vertices.size();
    createBuffer(vertexBuffer, vertexBufferMemory, vertexBufferSize, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(stagingBuffer, stagingBufferMemory, vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, vertexBufferSize);
    
    VkBufferDeviceAddressInfo bdaInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = vertexBuffer
    };
    vertexBufferAddress = vkGetBufferDeviceAddress(device, &bdaInfo);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    pushConstants.vertexBufferAddress = vertexBufferAddress;
}
void Engine::createIndexBuffer() {
    indexBufferSize = sizeof(indices[0])*indices.size();
    createBuffer(indexBuffer, indexBufferMemory, indexBufferSize, 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(stagingBuffer, stagingBufferMemory, indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, indexBufferSize);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
void Engine::createMVP() {
    MVPBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    MVPBufferMemory.resize(MAX_FRAMES_IN_FLIGHT);
    MVPBufferMemoryMapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(MVPBuffers[i], MVPBufferMemory[i], sizeof(MVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        vkMapMemory(device, MVPBufferMemory[i], 0, sizeof(MVP), 0, &MVPBufferMemoryMapped[i]);
    }
}

bool Engine::checkInstanceLayersSupport() {
    uint32_t count;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> layers(count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, layers.data()));
    std::set<std::string> requested(instanceLayers.begin(), instanceLayers.end());
    for (const auto& layer: layers) {
        requested.erase(layer.layerName);
    }
    return requested.empty();
}
bool Engine::checkInstanceExtensionsSupport() {
    uint32_t count;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> extensions(count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()));
    std::set<std::string> requested(instanceExtensions.begin(), instanceExtensions.end());
    for (const auto& ext: extensions) {
        requested.erase(ext.extensionName);
    }
    return requested.empty();
}
bool Engine::checkDeviceExtensionsSupport(VkPhysicalDevice dev) {
    uint32_t count;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> extensions(count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, extensions.data()));
    std::set<std::string> requested(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext: extensions) {
        requested.erase(ext.extensionName);
    }
    return requested.empty();
}
bool Engine::isDeviceSuitable(VkPhysicalDevice dev) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(dev, &features);

    QueueFamilyIndices indices = getQueueFamilyIndices(dev);
    SurfaceDetails details = getSurfaceDetails(dev);

    return features.multiDrawIndirect && 
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && 
        indices.isComplete() &&
        checkDeviceExtensionsSupport(dev) &&
        !details.formats.empty() && !details.modes.empty();
}
QueueFamilyIndices Engine::getQueueFamilyIndices(VkPhysicalDevice dev) {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, queues.data());
    QueueFamilyIndices indices;

    int i = 0;
    for (const auto queue: queues) {
        if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        if (!(queue.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queue.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            indices.transferFamily = i;
        }
        VkBool32 presentSupported;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupported);
        if (presentSupported) {
            indices.presentFamily = i;
        }
        i++;
    }
    return indices;
}
SurfaceDetails Engine::getSurfaceDetails(VkPhysicalDevice dev) {
    SurfaceDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, nullptr);
    details.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, details.formats.data());
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, nullptr);
    details.modes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, details.modes.data());
    return details;
}
VkExtent2D Engine::chooseSurfaceExtent(VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
        };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}
VkPresentModeKHR Engine::choosePresentMode(std::vector<VkPresentModeKHR> modes) {
    for (const auto& mode: modes) {
        if (mode==VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return mode;
        }
    }
    return modes[0];
}
VkSurfaceFormatKHR Engine::chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> formats) {
    for (const auto& format: formats) {
        if (format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}
VkCommandBuffer Engine::allocateCommandBuffer(VkCommandPool& cmdPool) {
    VkCommandBufferAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = cmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &cmdBuffer));
    return cmdBuffer;
}
uint32_t Engine::getMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags memProperties) {
    VkPhysicalDeviceMemoryProperties pDeviceMemProps{};
    vkGetPhysicalDeviceMemoryProperties(pDevice, &pDeviceMemProps);
    for (uint32_t i=0; i<pDeviceMemProps.memoryTypeCount; i++) {
        if ((typeFilter & (1<<i)) && (pDeviceMemProps.memoryTypes[i].propertyFlags & memProperties)==memProperties) {
            return i;
        }
    }
    throw std::runtime_error("VK Error: no suitable memory type for buffer");
}
void Engine::copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size) {
    VkCommandBuffer cmdBuffer = allocateCommandBuffer(transferCmdPool);
    
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    VkBufferCopy region{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &region);
    vkEndCommandBuffer(cmdBuffer);

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmdBuffer,
        .deviceMask = 0
    };
    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufferSubmitInfo,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos = nullptr
    };

    VkFence fence;
    createFence(fence, 0);
    VK_CHECK(vkQueueSubmit2(transferQueue, 1, &submitInfo, fence));
    vkWaitForFences(device, 1, &fence, VK_TRUE, ~0ull);
    vkDestroyFence(device, fence, nullptr);
}
void Engine::updateMVP(uint32_t index) {
    MVP mvp;
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    mvp.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    mvp.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    mvp.proj = glm::perspective(glm::radians(45.0f), (float)swapchainExtent.width/swapchainExtent.height, 0.1f, 10.0f);
    mvp.proj[1][1]*=-1;
    memcpy(MVPBufferMemoryMapped[index], &mvp, sizeof(MVP));
}