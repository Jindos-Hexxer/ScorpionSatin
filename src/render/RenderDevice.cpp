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
        .require_api_version(1, 2, 0)
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
    auto physResult = selector
        .set_minimum_version(1, 2)
        .require_present(false)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();
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
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;
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

void RenderDevice::Shutdown() {
    VkDevice dev = device_;
    VmaAllocator alloc = static_cast<VmaAllocator>(vmaAllocator_);
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
