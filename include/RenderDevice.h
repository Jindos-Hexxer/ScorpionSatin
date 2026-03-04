#pragma once

#include "MeshPrimitives.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Engine {

/** Opaque handle to a GPU mesh (VBO + IBO). ECS MeshRef.mesh_id uses this. */
using MeshHandle = uint32_t;
constexpr MeshHandle kInvalidMeshHandle = UINT32_MAX;

/**
 * Vulkan render device: buffer creation (VMA), mesh upload, and draw recording.
 * Init is headless (no swapchain); swapchain/window can be added by the host.
 */
class RenderDevice {
public:
    RenderDevice() = default;
    ~RenderDevice() { Shutdown(); }

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    /** Initialize Vulkan instance, device, VMA allocator, and upload command pool (headless). */
    bool Init();
    void Shutdown();

    bool IsValid() const { return device_ != VK_NULL_HANDLE; }

    /**
     * Upload CPU mesh to GPU; creates VBO and IBO via staging, registers in mesh registry.
     * Returns mesh handle for use with CmdDrawMesh and ECS MeshRef.mesh_id.
     */
    MeshHandle UploadMesh(const PrimitiveMesh& mesh);

    /**
     * Record bind vertex/index buffers and vkCmdDrawIndexed for the given mesh.
     * Caller must have begun a render pass and bound a pipeline with matching vertex layout (Engine::Vertex).
     */
    void CmdDrawMesh(VkCommandBuffer cmd, MeshHandle id) const;

    /**
     * Debug/editor draw: push model matrix (16 floats, column-major) and draw mesh.
     * Caller must have bound a pipeline whose layout has a push constant of at least 64 bytes at the given offset.
     */
    void CmdDrawMeshWithTransform(VkCommandBuffer cmd, MeshHandle id,
        const float* modelMatrix4x4, VkPipelineLayout pipelineLayout, uint32_t pushConstantOffset) const;

    /** Return index count for the mesh (for indirect draw or validation). */
    uint32_t GetMeshIndexCount(MeshHandle id) const;

    /** Raw handles for pipeline/descriptor creation by render passes. */
    VkDevice GetDevice() const { return device_; }
    VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice_; }
    VkQueue GetGraphicsQueue() const { return graphicsQueue_; }

private:
    void* vkbContext_ = nullptr;  // Opaque: holds vkb::Instance + vkb::Device for teardown
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex_ = 0;
    VkCommandPool uploadCommandPool_ = VK_NULL_HANDLE;
    void* vmaAllocator_ = nullptr; // VmaAllocator
    void* meshRegistry_ = nullptr;  // Opaque: std::vector<GpuMesh>-like storage
};

} // namespace Engine
