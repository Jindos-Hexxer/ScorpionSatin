#include "RenderDevice.h"
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <VkBootstrap.h>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Engine {

namespace {

constexpr VkDeviceSize kVertexStride = sizeof(Vertex); // position[3] + normal[3] + uv[2] = 8 floats

struct VkbContext {
    vkb::Instance instance;
    vkb::Device device;
};

struct GpuMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAlloc = VK_NULL_HANDLE;
    VmaAllocation indexAlloc = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
};

struct MeshRegistry {
    std::vector<GpuMesh> meshes;
};

} // namespace

bool RenderDevice::Init() {
    auto* ctx = new VkbContext();
    vkb::InstanceBuilder instBuilder;
    auto instResult = instBuilder
        .set_app_name("ScorpionSatin")
        .set_engine_name("ScorpionSatin")
        .require_api_version(1, 4, 0)
        .set_headless(true)
        .request_validation_layers(true)
        .use_default_debug_messenger()
        .build();
    if (!instResult) {
        delete ctx;
        return false;
    }
    ctx->instance = instResult.value();
    vkb::Instance& vkbInst = ctx->instance;
    instance_ = vkbInst.instance;

    vkb::PhysicalDeviceSelector selector(vkbInst);
    selector
        .set_minimum_version(1, 4)
        .require_present(false)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);
    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    selector.set_required_features_12(features12);
    auto physResult = selector.select();
    if (!physResult) {
        vkb::destroy_instance(vkbInst);
        delete ctx;
        instance_ = nullptr;
        return false;
    }
    vkb::PhysicalDevice vkbPhys = physResult.value();
    physicalDevice_ = vkbPhys.physical_device;

    vkb::DeviceBuilder deviceBuilder(vkbPhys);
    auto devResult = deviceBuilder.build();
    if (!devResult) {
        vkb::destroy_instance(vkbInst);
        delete ctx;
        instance_ = nullptr;
        physicalDevice_ = nullptr;
        return false;
    }
    ctx->device = devResult.value();
    vkb::Device& vkbDev = ctx->device;
    device_ = vkbDev.device;

    auto queueResult = vkbDev.get_queue(vkb::QueueType::graphics);
    if (!queueResult) {
        vkb::destroy_device(vkbDev);
        vkb::destroy_instance(vkbInst);
        delete ctx;
        device_ = nullptr;
        physicalDevice_ = nullptr;
        instance_ = nullptr;
        return false;
    }
    graphicsQueue_ = queueResult.value();
    auto queueIdxResult = vkbDev.get_queue_index(vkb::QueueType::graphics);
    graphicsQueueFamilyIndex_ = queueIdxResult.value();
    vkbContext_ = ctx;

    VmaAllocatorCreateInfo allocInfo = {};
    allocInfo.physicalDevice = physicalDevice_;
    allocInfo.device = device_;
    allocInfo.instance = instance_;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;
    VmaAllocator allocator = VK_NULL_HANDLE;
    if (vmaCreateAllocator(&allocInfo, &allocator) != VK_SUCCESS) {
        vkb::destroy_device(ctx->device);
        vkb::destroy_instance(ctx->instance);
        delete ctx;
        vkbContext_ = nullptr;
        device_ = nullptr;
        physicalDevice_ = nullptr;
        instance_ = nullptr;
        return false;
    }
    vmaAllocator_ = allocator;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex_;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkResult res = vkCreateCommandPool(static_cast<VkDevice>(device_), &poolInfo, nullptr, &pool);
    if (res != VK_SUCCESS) {
        vmaDestroyAllocator(static_cast<VmaAllocator>(vmaAllocator_));
        vkb::destroy_device(ctx->device);
        vkb::destroy_instance(ctx->instance);
        delete ctx;
        vkbContext_ = nullptr;
        vmaAllocator_ = nullptr;
        device_ = nullptr;
        physicalDevice_ = nullptr;
        instance_ = nullptr;
        return false;
    }
    uploadCommandPool_ = pool;

    meshRegistry_ = new MeshRegistry;
    return true;
}

bool RenderDevice::CreateSceneUBO() {
    if (device_ == VK_NULL_HANDLE || vmaAllocator_ == nullptr)
        return false;
    if (sceneUBOBuffer_ != VK_NULL_HANDLE)
        return true;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = sizeof(GlobalUBO);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer buf = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    if (vmaCreateBuffer(static_cast<VmaAllocator>(vmaAllocator_), &bufInfo, &allocInfo,
            &buf, &alloc, nullptr) != VK_SUCCESS)
        return false;

    sceneUBOBuffer_ = buf;
    sceneUBOAlloc_ = alloc;
    if (pbrDescriptorSet_ != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo uboInfo = {};
        uboInfo.buffer = sceneUBOBuffer_;
        uboInfo.offset = 0;
        uboInfo.range = sizeof(GlobalUBO);
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = pbrDescriptorSet_;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &uboInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }
    return true;
}

void RenderDevice::UpdateSceneUBO(const GlobalUBO& ubo) {
    if (sceneUBOBuffer_ == VK_NULL_HANDLE || sceneUBOAlloc_ == nullptr)
        return;
    void* ptr = nullptr;
    if (vmaMapMemory(static_cast<VmaAllocator>(vmaAllocator_), static_cast<VmaAllocation>(sceneUBOAlloc_), &ptr) == VK_SUCCESS) {
        std::memcpy(ptr, &ubo, sizeof(GlobalUBO));
        vmaUnmapMemory(static_cast<VmaAllocator>(vmaAllocator_), static_cast<VmaAllocation>(sceneUBOAlloc_));
    }
}

bool RenderDevice::CreatePBRResources() {
    if (device_ == VK_NULL_HANDLE || vmaAllocator_ == nullptr)
        return false;
    if (pbrDescriptorSetLayout_ != VK_NULL_HANDLE)
        return true; // already created

    VkDevice dev = device_;
    VmaAllocator alloc = static_cast<VmaAllocator>(vmaAllocator_);

    // Material SSBO
    const VkDeviceSize materialSSBOSize = kMaxPBRMaterials * sizeof(PBRMaterialData);
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = materialSSBOSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocation materialAlloc = VK_NULL_HANDLE;
    if (vmaCreateBuffer(alloc, &bufInfo, &allocInfo, &pbrMaterialSSBO_, &materialAlloc, nullptr) != VK_SUCCESS)
        return false;
    pbrMaterialSSBOAlloc_ = materialAlloc;

    // Descriptor set layout: binding 0 UBO, 1 SSBO, 3 combined image sampler array (variable count, partially bound)
    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].binding = 3; // skip 2 for optional transform SSBO later
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = kMaxBindlessTextures;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    VkDescriptorBindingFlags bindingFlagsArr[3] = { 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = 3;
    bindingFlagsInfo.pBindingFlags = bindingFlagsArr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;
    layoutInfo.pNext = &bindingFlagsInfo;

    if (vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &pbrDescriptorSetLayout_) != VK_SUCCESS) {
        vmaDestroyBuffer(alloc, pbrMaterialSSBO_, static_cast<VmaAllocation>(pbrMaterialSSBOAlloc_));
        pbrMaterialSSBO_ = VK_NULL_HANDLE;
        pbrMaterialSSBOAlloc_ = nullptr;
        return false;
    }

    // Pipeline layout: one descriptor set + push constant (vertex + fragment, 68 bytes)
    VkPushConstantRange pushRanges[1] = {};
    pushRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRanges[0].offset = 0;
    pushRanges[0].size = kPBRPushConstantSize;
    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = 1;
    plLayoutInfo.pSetLayouts = &pbrDescriptorSetLayout_;
    plLayoutInfo.pushConstantRangeCount = 1;
    plLayoutInfo.pPushConstantRanges = pushRanges;
    if (vkCreatePipelineLayout(dev, &plLayoutInfo, nullptr, &pbrPipelineLayout_) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(dev, pbrDescriptorSetLayout_, nullptr);
        vmaDestroyBuffer(alloc, pbrMaterialSSBO_, static_cast<VmaAllocation>(pbrMaterialSSBOAlloc_));
        pbrDescriptorSetLayout_ = VK_NULL_HANDLE;
        pbrMaterialSSBO_ = VK_NULL_HANDLE;
        pbrMaterialSSBOAlloc_ = nullptr;
        return false;
    }

    // Descriptor pool: 1 set, with UBO + SSBO + many image samplers
    VkDescriptorPoolSize poolSizes[3] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = kMaxBindlessTextures;
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(dev, &poolInfo, nullptr, &pbrDescriptorPool_) != VK_SUCCESS) {
        vkDestroyPipelineLayout(dev, pbrPipelineLayout_, nullptr);
        vkDestroyDescriptorSetLayout(dev, pbrDescriptorSetLayout_, nullptr);
        vmaDestroyBuffer(alloc, pbrMaterialSSBO_, static_cast<VmaAllocation>(pbrMaterialSSBOAlloc_));
        pbrPipelineLayout_ = VK_NULL_HANDLE;
        pbrDescriptorSetLayout_ = VK_NULL_HANDLE;
        pbrMaterialSSBO_ = VK_NULL_HANDLE;
        pbrMaterialSSBOAlloc_ = nullptr;
        return false;
    }

    // Allocate the single descriptor set
    VkDescriptorSetAllocateInfo allocSetInfo = {};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = pbrDescriptorPool_;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &pbrDescriptorSetLayout_;
    if (vkAllocateDescriptorSets(dev, &allocSetInfo, &pbrDescriptorSet_) != VK_SUCCESS) {
        vkDestroyDescriptorPool(dev, pbrDescriptorPool_, nullptr);
        vkDestroyPipelineLayout(dev, pbrPipelineLayout_, nullptr);
        vkDestroyDescriptorSetLayout(dev, pbrDescriptorSetLayout_, nullptr);
        vmaDestroyBuffer(alloc, pbrMaterialSSBO_, static_cast<VmaAllocation>(pbrMaterialSSBOAlloc_));
        pbrDescriptorPool_ = VK_NULL_HANDLE;
        pbrPipelineLayout_ = VK_NULL_HANDLE;
        pbrDescriptorSetLayout_ = VK_NULL_HANDLE;
        pbrMaterialSSBO_ = VK_NULL_HANDLE;
        pbrMaterialSSBOAlloc_ = nullptr;
        return false;
    }

    // Default sampler for bindless textures
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxAnisotropy = 1.0f;
    if (vkCreateSampler(dev, &samplerInfo, nullptr, &pbrDefaultSampler_) != VK_SUCCESS) {
        pbrDefaultSampler_ = VK_NULL_HANDLE;
        // non-fatal; we can still write descriptors, just need to provide sampler per write
    }

    // Write descriptor set: UBO (binding 0), SSBO (binding 1). Binding 3 updated by RegisterBindlessTexture.
    if (sceneUBOBuffer_ == VK_NULL_HANDLE) {
        // Caller should CreateSceneUBO first for UBO; we can still create and leave binding 0 unbound until they do
    }
    VkDescriptorBufferInfo uboInfo = {};
    uboInfo.buffer = sceneUBOBuffer_;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(GlobalUBO);
    VkDescriptorBufferInfo ssboInfo = {};
    ssboInfo.buffer = pbrMaterialSSBO_;
    ssboInfo.offset = 0;
    ssboInfo.range = materialSSBOSize;

    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = pbrDescriptorSet_;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &uboInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = pbrDescriptorSet_;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &ssboInfo;
    if (sceneUBOBuffer_ != VK_NULL_HANDLE)
        vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
    else
        vkUpdateDescriptorSets(dev, 1, writes + 1, 0, nullptr);

    return true;
}

void RenderDevice::UpdateMaterialRange(uint32_t start, uint32_t count, const PBRMaterialData* data) {
    if (pbrMaterialSSBO_ == VK_NULL_HANDLE || pbrMaterialSSBOAlloc_ == nullptr || !data || count == 0)
        return;
    if (start + count > kMaxPBRMaterials)
        return;
    void* ptr = nullptr;
    if (vmaMapMemory(static_cast<VmaAllocator>(vmaAllocator_), static_cast<VmaAllocation>(pbrMaterialSSBOAlloc_), &ptr) != VK_SUCCESS)
        return;
    std::memcpy(static_cast<char*>(ptr) + start * sizeof(PBRMaterialData), data, count * sizeof(PBRMaterialData));
    vmaUnmapMemory(static_cast<VmaAllocator>(vmaAllocator_), static_cast<VmaAllocation>(pbrMaterialSSBOAlloc_));
}

uint32_t RenderDevice::RegisterBindlessTexture(VkImageView imageView) {
    if (pbrDescriptorSet_ == VK_NULL_HANDLE || pbrDescriptorPool_ == VK_NULL_HANDLE || imageView == VK_NULL_HANDLE)
        return UINT32_MAX;
    if (pbrNextTextureIndex_ >= kMaxBindlessTextures)
        return UINT32_MAX;
    uint32_t idx = pbrNextTextureIndex_++;
    VkDescriptorImageInfo imgInfo = {};
    imgInfo.sampler = pbrDefaultSampler_ != VK_NULL_HANDLE ? pbrDefaultSampler_ : VK_NULL_HANDLE;
    imgInfo.imageView = imageView;
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = pbrDescriptorSet_;
    write.dstBinding = 3;
    write.dstArrayElement = idx;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgInfo;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return idx;
}

VkPipeline RenderDevice::CreatePBRPipeline(VkRenderPass renderPass, VkShaderModule vertModule, VkShaderModule fragModule) {
    if (device_ == VK_NULL_HANDLE || pbrPipelineLayout_ == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE ||
        vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;
    VkDevice dev = device_;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = static_cast<uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[4] = {};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = 12;
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = 24;
    attrs[3].location = 3;
    attrs[3].binding = 0;
    attrs[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[3].offset = 32;
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // reverse Z
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pbrPipelineLayout_;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return pipeline;
}

void RenderDevice::CmdDrawMeshPBR(VkCommandBuffer cmd, MeshHandle meshId, uint32_t materialId, const float* modelMatrix4x4) const {
    if (!cmd || !modelMatrix4x4 || pbrPipelineLayout_ == VK_NULL_HANDLE)
        return;
    alignas(4) char pushBuf[kPBRPushConstantSize];
    std::memcpy(pushBuf, modelMatrix4x4, kPBRPushConstantModelSize);
    std::memcpy(pushBuf + kPBRPushConstantMaterialIdOffset, &materialId, sizeof(materialId));
    vkCmdPushConstants(cmd, pbrPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, kPBRPushConstantSize, pushBuf);
    CmdDrawMesh(cmd, meshId);
}

bool RenderDevice::CreateGBufferResources(uint32_t width, uint32_t height) {
    if (device_ == VK_NULL_HANDLE || vmaAllocator_ == nullptr || physicalDevice_ == VK_NULL_HANDLE)
        return false;
    if (width == 0 || height == 0)
        return false;
    if (gbufferRenderPass_ != VK_NULL_HANDLE && gbufferWidth_ == width && gbufferHeight_ == height)
        return true;

    VkDevice dev = device_;
    VmaAllocator alloc = static_cast<VmaAllocator>(vmaAllocator_);

    // Destroy existing G-Buffer if size changed
    if (gbufferRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, gbufferRenderPass_, nullptr);
        gbufferRenderPass_ = VK_NULL_HANDLE;
    }
    if (gbufferAlbedoView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, gbufferAlbedoView_, nullptr);
        gbufferAlbedoView_ = VK_NULL_HANDLE;
    }
    if (gbufferNormalRoughnessView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, gbufferNormalRoughnessView_, nullptr);
        gbufferNormalRoughnessView_ = VK_NULL_HANDLE;
    }
    if (gbufferDepthView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, gbufferDepthView_, nullptr);
        gbufferDepthView_ = VK_NULL_HANDLE;
    }
    if (gbufferAlbedoImage_ != VK_NULL_HANDLE && gbufferAlbedoAlloc_ != nullptr) {
        vmaDestroyImage(alloc, gbufferAlbedoImage_, static_cast<VmaAllocation>(gbufferAlbedoAlloc_));
        gbufferAlbedoImage_ = VK_NULL_HANDLE;
        gbufferAlbedoAlloc_ = nullptr;
    }
    if (gbufferNormalRoughnessImage_ != VK_NULL_HANDLE && gbufferNormalRoughnessAlloc_ != nullptr) {
        vmaDestroyImage(alloc, gbufferNormalRoughnessImage_, static_cast<VmaAllocation>(gbufferNormalRoughnessAlloc_));
        gbufferNormalRoughnessImage_ = VK_NULL_HANDLE;
        gbufferNormalRoughnessAlloc_ = nullptr;
    }
    if (gbufferDepthImage_ != VK_NULL_HANDLE && gbufferDepthAlloc_ != nullptr) {
        vmaDestroyImage(alloc, gbufferDepthImage_, static_cast<VmaAllocation>(gbufferDepthAlloc_));
        gbufferDepthImage_ = VK_NULL_HANDLE;
        gbufferDepthAlloc_ = nullptr;
    }

    const VkFormat albedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkFormat normalRoughnessFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = { width, height, 1 };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    imgInfo.format = albedoFormat;
    VmaAllocation albedoAlloc = VK_NULL_HANDLE;
    if (vmaCreateImage(alloc, &imgInfo, &allocInfo, &gbufferAlbedoImage_, &albedoAlloc, nullptr) != VK_SUCCESS) return false;
    gbufferAlbedoAlloc_ = albedoAlloc;

    imgInfo.format = normalRoughnessFormat;
    VmaAllocation normalRoughAlloc = VK_NULL_HANDLE;
    if (vmaCreateImage(alloc, &imgInfo, &allocInfo, &gbufferNormalRoughnessImage_, &normalRoughAlloc, nullptr) != VK_SUCCESS) {
        vmaDestroyImage(alloc, gbufferAlbedoImage_, albedoAlloc);
        gbufferAlbedoImage_ = VK_NULL_HANDLE;
        gbufferAlbedoAlloc_ = nullptr;
        return false;
    }
    gbufferNormalRoughnessAlloc_ = normalRoughAlloc;

    VkImageCreateInfo depthInfo = {};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.format = depthFormat;
    depthInfo.extent = { width, height, 1 };
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo depthAllocInfo = {};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VmaAllocation depthAlloc = VK_NULL_HANDLE;
    if (vmaCreateImage(alloc, &depthInfo, &depthAllocInfo, &gbufferDepthImage_, &depthAlloc, nullptr) != VK_SUCCESS) {
        vmaDestroyImage(alloc, gbufferNormalRoughnessImage_, static_cast<VmaAllocation>(gbufferNormalRoughnessAlloc_));
        vmaDestroyImage(alloc, gbufferAlbedoImage_, static_cast<VmaAllocation>(gbufferAlbedoAlloc_));
        gbufferNormalRoughnessImage_ = VK_NULL_HANDLE;
        gbufferNormalRoughnessAlloc_ = nullptr;
        gbufferAlbedoImage_ = VK_NULL_HANDLE;
        gbufferAlbedoAlloc_ = nullptr;
        return false;
    }
    gbufferDepthAlloc_ = depthAlloc;

    auto createImageView = [&](VkImage image, VkFormat format, VkImageAspectFlags aspect) -> VkImageView {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(dev, &viewInfo, nullptr, &view) != VK_SUCCESS)
            return VK_NULL_HANDLE;
        return view;
    };

    gbufferAlbedoView_ = createImageView(gbufferAlbedoImage_, albedoFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    gbufferNormalRoughnessView_ = createImageView(gbufferNormalRoughnessImage_, normalRoughnessFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    gbufferDepthView_ = createImageView(gbufferDepthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    if (gbufferAlbedoView_ == VK_NULL_HANDLE || gbufferNormalRoughnessView_ == VK_NULL_HANDLE || gbufferDepthView_ == VK_NULL_HANDLE) {
        if (gbufferDepthView_ != VK_NULL_HANDLE) vkDestroyImageView(dev, gbufferDepthView_, nullptr);
        if (gbufferNormalRoughnessView_ != VK_NULL_HANDLE) vkDestroyImageView(dev, gbufferNormalRoughnessView_, nullptr);
        if (gbufferAlbedoView_ != VK_NULL_HANDLE) vkDestroyImageView(dev, gbufferAlbedoView_, nullptr);
        vmaDestroyImage(alloc, gbufferDepthImage_, static_cast<VmaAllocation>(gbufferDepthAlloc_));
        vmaDestroyImage(alloc, gbufferNormalRoughnessImage_, static_cast<VmaAllocation>(gbufferNormalRoughnessAlloc_));
        vmaDestroyImage(alloc, gbufferAlbedoImage_, static_cast<VmaAllocation>(gbufferAlbedoAlloc_));
        gbufferDepthView_ = gbufferNormalRoughnessView_ = gbufferAlbedoView_ = VK_NULL_HANDLE;
        gbufferDepthImage_ = gbufferNormalRoughnessImage_ = gbufferAlbedoImage_ = VK_NULL_HANDLE;
        gbufferDepthAlloc_ = gbufferNormalRoughnessAlloc_ = gbufferAlbedoAlloc_ = nullptr;
        return false;
    }

    VkAttachmentDescription attachments[3] = {};
    attachments[0].format = albedoFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = normalRoughnessFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].format = depthFormat;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRefs[2] = { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }, { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } };
    VkAttachmentReference depthRef = { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 2;
    subpass.pColorAttachments = colorRefs;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 3;
    rpInfo.pAttachments = attachments;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    if (vkCreateRenderPass(dev, &rpInfo, nullptr, &gbufferRenderPass_) != VK_SUCCESS) {
        vkDestroyImageView(dev, gbufferDepthView_, nullptr);
        vkDestroyImageView(dev, gbufferNormalRoughnessView_, nullptr);
        vkDestroyImageView(dev, gbufferAlbedoView_, nullptr);
        vmaDestroyImage(alloc, gbufferDepthImage_, static_cast<VmaAllocation>(gbufferDepthAlloc_));
        vmaDestroyImage(alloc, gbufferNormalRoughnessImage_, static_cast<VmaAllocation>(gbufferNormalRoughnessAlloc_));
        vmaDestroyImage(alloc, gbufferAlbedoImage_, static_cast<VmaAllocation>(gbufferAlbedoAlloc_));
        gbufferAlbedoView_ = gbufferNormalRoughnessView_ = gbufferDepthView_ = VK_NULL_HANDLE;
        gbufferAlbedoImage_ = gbufferNormalRoughnessImage_ = gbufferDepthImage_ = VK_NULL_HANDLE;
        gbufferAlbedoAlloc_ = gbufferNormalRoughnessAlloc_ = gbufferDepthAlloc_ = nullptr;
        return false;
    }

    gbufferWidth_ = width;
    gbufferHeight_ = height;
    return true;
}

VkPipeline RenderDevice::CreatePBRGBufferPipeline(VkRenderPass gbufferRenderPass, VkShaderModule vertModule, VkShaderModule gbufferFragModule) {
    if (device_ == VK_NULL_HANDLE || pbrPipelineLayout_ == VK_NULL_HANDLE || gbufferRenderPass == VK_NULL_HANDLE ||
        vertModule == VK_NULL_HANDLE || gbufferFragModule == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;
    VkDevice dev = device_;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = gbufferFragModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = static_cast<uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[4] = {};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = 12;
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = 24;
    attrs[3].location = 3;
    attrs[3].binding = 0;
    attrs[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[3].offset = 32;
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAttachments[2] = {};
    blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachments[0].blendEnable = VK_FALSE;
    blendAttachments[1] = blendAttachments[0];
    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 2;
    colorBlend.pAttachments = blendAttachments;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pbrPipelineLayout_;
    pipelineInfo.renderPass = gbufferRenderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return pipeline;
}

void RenderDevice::Shutdown() {
    VkDevice dev = device_;
    VmaAllocator alloc = static_cast<VmaAllocator>(vmaAllocator_);

    if (pbrDefaultSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(dev, pbrDefaultSampler_, nullptr);
        pbrDefaultSampler_ = VK_NULL_HANDLE;
    }
    if (pbrDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, pbrDescriptorPool_, nullptr);
        pbrDescriptorPool_ = VK_NULL_HANDLE;
        pbrDescriptorSet_ = VK_NULL_HANDLE;
    }
    if (pbrPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, pbrPipelineLayout_, nullptr);
        pbrPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (pbrDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, pbrDescriptorSetLayout_, nullptr);
        pbrDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (pbrMaterialSSBO_ != VK_NULL_HANDLE && pbrMaterialSSBOAlloc_ != nullptr) {
        vmaDestroyBuffer(alloc, pbrMaterialSSBO_, static_cast<VmaAllocation>(pbrMaterialSSBOAlloc_));
        pbrMaterialSSBO_ = VK_NULL_HANDLE;
        pbrMaterialSSBOAlloc_ = nullptr;
    }

    if (gbufferRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, gbufferRenderPass_, nullptr);
        gbufferRenderPass_ = VK_NULL_HANDLE;
    }
    if (gbufferAlbedoView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, gbufferAlbedoView_, nullptr);
        gbufferAlbedoView_ = VK_NULL_HANDLE;
    }
    if (gbufferNormalRoughnessView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, gbufferNormalRoughnessView_, nullptr);
        gbufferNormalRoughnessView_ = VK_NULL_HANDLE;
    }
    if (gbufferDepthView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, gbufferDepthView_, nullptr);
        gbufferDepthView_ = VK_NULL_HANDLE;
    }
    if (gbufferAlbedoImage_ != VK_NULL_HANDLE && gbufferAlbedoAlloc_ != nullptr) {
        vmaDestroyImage(alloc, gbufferAlbedoImage_, static_cast<VmaAllocation>(gbufferAlbedoAlloc_));
        gbufferAlbedoImage_ = VK_NULL_HANDLE;
        gbufferAlbedoAlloc_ = nullptr;
    }
    if (gbufferNormalRoughnessImage_ != VK_NULL_HANDLE && gbufferNormalRoughnessAlloc_ != nullptr) {
        vmaDestroyImage(alloc, gbufferNormalRoughnessImage_, static_cast<VmaAllocation>(gbufferNormalRoughnessAlloc_));
        gbufferNormalRoughnessImage_ = VK_NULL_HANDLE;
        gbufferNormalRoughnessAlloc_ = nullptr;
    }
    if (gbufferDepthImage_ != VK_NULL_HANDLE && gbufferDepthAlloc_ != nullptr) {
        vmaDestroyImage(alloc, gbufferDepthImage_, static_cast<VmaAllocation>(gbufferDepthAlloc_));
        gbufferDepthImage_ = VK_NULL_HANDLE;
        gbufferDepthAlloc_ = nullptr;
    }

    if (sceneUBOBuffer_ != VK_NULL_HANDLE && sceneUBOAlloc_ != nullptr) {
        vmaDestroyBuffer(alloc, sceneUBOBuffer_, static_cast<VmaAllocation>(sceneUBOAlloc_));
        sceneUBOBuffer_ = VK_NULL_HANDLE;
        sceneUBOAlloc_ = nullptr;
    }
    auto* reg = static_cast<MeshRegistry*>(meshRegistry_);
    if (reg) {
        for (GpuMesh& m : reg->meshes) {
            if (m.vertexBuffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(alloc, m.vertexBuffer, m.vertexAlloc);
                m.vertexBuffer = VK_NULL_HANDLE;
                m.vertexAlloc = VK_NULL_HANDLE;
            }
            if (m.indexBuffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(alloc, m.indexBuffer, m.indexAlloc);
                m.indexBuffer = VK_NULL_HANDLE;
                m.indexAlloc = VK_NULL_HANDLE;
            }
        }
        delete reg;
        meshRegistry_ = nullptr;
    }
    if (uploadCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(dev, uploadCommandPool_, nullptr);
        uploadCommandPool_ = nullptr;
    }
    if (alloc != VK_NULL_HANDLE) {
        vmaDestroyAllocator(alloc);
        vmaAllocator_ = nullptr;
    }
    if (dev != VK_NULL_HANDLE) {
        vkDestroyDevice(dev, nullptr);
        device_ = nullptr;
        graphicsQueue_ = nullptr;
    }
    physicalDevice_ = nullptr;
    if (vkbContext_ != nullptr) {
        auto* ctx = static_cast<VkbContext*>(vkbContext_);
        vkb::destroy_device(ctx->device);
        vkb::destroy_instance(ctx->instance);
        delete ctx;
        vkbContext_ = nullptr;
        instance_ = nullptr;
    }
}

MeshHandle RenderDevice::UploadMesh(const PrimitiveMesh& mesh) {
    if (!IsValid() || mesh.vertices.empty() || mesh.indices.empty()) {
        return kInvalidMeshHandle;
    }
    VkDevice dev = device_;
    VmaAllocator alloc = static_cast<VmaAllocator>(vmaAllocator_);
    VkQueue queue = graphicsQueue_;

    const VkDeviceSize vertexBytes = mesh.vertices.size() * kVertexStride;
    const VkDeviceSize indexBytes = mesh.indices.size() * sizeof(uint32_t);

    // GPU-only buffers for final storage
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.size = vertexBytes;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreate = {};
    allocCreate.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkBuffer vbo = VK_NULL_HANDLE;
    VmaAllocation vboAlloc = VK_NULL_HANDLE;
    if (vmaCreateBuffer(alloc, &bufInfo, &allocCreate, &vbo, &vboAlloc, nullptr) != VK_SUCCESS) {
        return kInvalidMeshHandle;
    }

    bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.size = indexBytes;
    VkBuffer ibo = VK_NULL_HANDLE;
    VmaAllocation iboAlloc = VK_NULL_HANDLE;
    if (vmaCreateBuffer(alloc, &bufInfo, &allocCreate, &ibo, &iboAlloc, nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(alloc, vbo, vboAlloc);
        return kInvalidMeshHandle;
    }

    // Staging buffers for one-time upload
    allocCreate.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocCreate.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.size = vertexBytes;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingVbo = VK_NULL_HANDLE;
    VmaAllocation stagingVboAlloc = VK_NULL_HANDLE;
    VmaAllocationInfo stagingVboInfo = {};
    if (vmaCreateBuffer(alloc, &stagingInfo, &allocCreate, &stagingVbo, &stagingVboAlloc, &stagingVboInfo) != VK_SUCCESS) {
        vmaDestroyBuffer(alloc, ibo, iboAlloc);
        vmaDestroyBuffer(alloc, vbo, vboAlloc);
        return kInvalidMeshHandle;
    }
    std::memcpy(stagingVboInfo.pMappedData, mesh.vertices.data(), vertexBytes);

    stagingInfo.size = indexBytes;
    VkBuffer stagingIbo = VK_NULL_HANDLE;
    VmaAllocation stagingIboAlloc = VK_NULL_HANDLE;
    VmaAllocationInfo stagingIboInfo = {};
    if (vmaCreateBuffer(alloc, &stagingInfo, &allocCreate, &stagingIbo, &stagingIboAlloc, &stagingIboInfo) != VK_SUCCESS) {
        vmaDestroyBuffer(alloc, stagingVbo, stagingVboAlloc);
        vmaDestroyBuffer(alloc, ibo, iboAlloc);
        vmaDestroyBuffer(alloc, vbo, vboAlloc);
        return kInvalidMeshHandle;
    }
    std::memcpy(stagingIboInfo.pMappedData, mesh.indices.data(), indexBytes);

    // One-time command buffer: copy staging -> GPU
    VkCommandPool pool = uploadCommandPool_;
    VkCommandBufferAllocateInfo allocCmdInfo = {};
    allocCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocCmdInfo.commandPool = pool;
    allocCmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocCmdInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(dev, &allocCmdInfo, &cmd) != VK_SUCCESS) {
        vmaDestroyBuffer(alloc, stagingIbo, stagingIboAlloc);
        vmaDestroyBuffer(alloc, stagingVbo, stagingVboAlloc);
        vmaDestroyBuffer(alloc, ibo, iboAlloc);
        vmaDestroyBuffer(alloc, vbo, vboAlloc);
        return kInvalidMeshHandle;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion = {};
    copyRegion.size = vertexBytes;
    vkCmdCopyBuffer(cmd, stagingVbo, vbo, 1, &copyRegion);
    copyRegion.size = indexBytes;
    vkCmdCopyBuffer(cmd, stagingIbo, ibo, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(dev, pool, 1, &cmd);
    vmaDestroyBuffer(alloc, stagingIbo, stagingIboAlloc);
    vmaDestroyBuffer(alloc, stagingVbo, stagingVboAlloc);

    GpuMesh gpuMesh;
    gpuMesh.vertexBuffer = vbo;
    gpuMesh.indexBuffer = ibo;
    gpuMesh.vertexAlloc = vboAlloc;
    gpuMesh.indexAlloc = iboAlloc;
    gpuMesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
    static_cast<MeshRegistry*>(meshRegistry_)->meshes.push_back(gpuMesh);
    return static_cast<MeshHandle>(static_cast<MeshRegistry*>(meshRegistry_)->meshes.size() - 1);
}

void RenderDevice::CmdDrawMesh(VkCommandBuffer cmd, MeshHandle id) const {
    auto* reg = static_cast<MeshRegistry*>(meshRegistry_);
    if (!cmd || !reg || id >= reg->meshes.size()) {
        return;
    }
    const GpuMesh& m = reg->meshes[id];
    if (m.vertexBuffer == VK_NULL_HANDLE || m.indexBuffer == VK_NULL_HANDLE) {
        return;
    }
    VkDeviceSize vbOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m.vertexBuffer, &vbOffset);
    vkCmdBindIndexBuffer(cmd, m.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m.indexCount, 1, 0, 0, 0);
}

void RenderDevice::CmdDrawMeshWithTransform(VkCommandBuffer cmd, MeshHandle id,
    const float* modelMatrix4x4, VkPipelineLayout pipelineLayout, uint32_t pushConstantOffset) const {
    if (!cmd || !modelMatrix4x4 || pipelineLayout == VK_NULL_HANDLE) {
        return;
    }
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, pushConstantOffset, 64, modelMatrix4x4);
    CmdDrawMesh(cmd, id);
}

uint32_t RenderDevice::GetMeshIndexCount(MeshHandle id) const {
    auto* reg = static_cast<MeshRegistry*>(meshRegistry_);
    if (!reg || id >= reg->meshes.size()) {
        return 0;
    }
    return reg->meshes[id].indexCount;
}

} // namespace Engine
