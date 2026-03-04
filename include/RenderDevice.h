#pragma once

#include "MeshPrimitives.h"
#include "PBRMaterial.h"
#include "SceneUniforms.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Engine {

/** Opaque handle to a GPU mesh (VBO + IBO). ECS MeshRef.mesh_id uses this. */
using MeshHandle = uint32_t;
constexpr MeshHandle kInvalidMeshHandle = UINT32_MAX;

/** Max materials in the global PBR SSBO and bindless texture array. */
constexpr uint32_t kMaxPBRMaterials = 10000u;
constexpr uint32_t kMaxBindlessTextures = 10000u;

/** Push constant layout for PBR pipeline: mat4 model (64 bytes) + uint material_id (4 bytes). */
constexpr uint32_t kPBRPushConstantModelSize = 64u;
constexpr uint32_t kPBRPushConstantMaterialIdOffset = 64u;
constexpr uint32_t kPBRPushConstantSize = 68u;

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

    /** Create the scene uniform buffer (call after Init). Returns false on failure. */
    bool CreateSceneUBO();
    /** Update scene UBO with camera data. Call each frame before drawing. No-op if CreateSceneUBO was not called. */
    void UpdateSceneUBO(const GlobalUBO& ubo);
    /** Get the scene UBO buffer for descriptor set binding. VK_NULL_HANDLE if not created. */
    VkBuffer GetSceneUBOBuffer() const { return sceneUBOBuffer_; }
    /** Get the size of the scene UBO. */
    static constexpr size_t GetSceneUBOSize() { return sizeof(GlobalUBO); }

    // --- PBR bindless (call after Init; CreateSceneUBO recommended first) ---
    /** Create material SSBO, descriptor pool/layout/set, and bindless texture array. Requires Vulkan 1.3+ descriptor indexing. */
    bool CreatePBRResources();
    /** Upload material data into the SSBO. start + count must not exceed kMaxPBRMaterials. */
    void UpdateMaterialRange(uint32_t start, uint32_t count, const PBRMaterialData* data);
    /** Register a texture in the bindless array. Returns index (0..kMaxBindlessTextures-1) or UINT32_MAX on failure. */
    uint32_t RegisterBindlessTexture(VkImageView imageView);
    /** Get the single PBR descriptor set (bind once per frame for all PBR draws). */
    VkDescriptorSet GetPBRDescriptorSet() const { return pbrDescriptorSet_; }
    /** Get PBR descriptor set layout for pipeline creation. */
    VkDescriptorSetLayout GetPBRDescriptorSetLayout() const { return pbrDescriptorSetLayout_; }
    /** Get PBR pipeline layout (push constants: model mat4 + material_id uint). */
    VkPipelineLayout GetPBRPipelineLayout() const { return pbrPipelineLayout_; }
    /** Create PBR graphics pipeline. renderPass and vertex layout must match Engine::Vertex (pos, normal, uv, tangent). */
    VkPipeline CreatePBRPipeline(VkRenderPass renderPass, VkShaderModule vertModule, VkShaderModule fragModule);
    /** Record PBR draw: bind descriptor set (caller), push model + material_id, draw mesh. Pipeline and descriptor set must be bound by caller. */
    void CmdDrawMeshPBR(VkCommandBuffer cmd, MeshHandle meshId, uint32_t materialId, const float* modelMatrix4x4) const;

    // --- G-Buffer (for RTXGI: albedo, normal+roughness, depth). Call after CreatePBRResources. ---
    /** Create G-Buffer images (albedo, normal+roughness, depth) and render pass. Resize-safe: call with new width/height to recreate. */
    bool CreateGBufferResources(uint32_t width, uint32_t height);
    /** Get the G-Buffer render pass (2 color attachments + depth). VK_NULL_HANDLE if not created. */
    VkRenderPass GetGBufferRenderPass() const { return gbufferRenderPass_; }
    /** G-Buffer color attachment views for albedo (RGB) and normal+roughness (RGB+A). */
    VkImageView GetGBufferAlbedoView() const { return gbufferAlbedoView_; }
    VkImageView GetGBufferNormalRoughnessView() const { return gbufferNormalRoughnessView_; }
    /** G-Buffer depth attachment view. */
    VkImageView GetGBufferDepthView() const { return gbufferDepthView_; }
    /** Create PBR G-Buffer pipeline (same layout as PBR, 2 color outputs). Use with GetGBufferRenderPass(). */
    VkPipeline CreatePBRGBufferPipeline(VkRenderPass gbufferRenderPass, VkShaderModule vertModule, VkShaderModule gbufferFragModule);

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
    VkBuffer sceneUBOBuffer_ = VK_NULL_HANDLE;
    void* sceneUBOAlloc_ = nullptr; // VmaAllocation

    // PBR bindless
    VkBuffer pbrMaterialSSBO_ = VK_NULL_HANDLE;
    void* pbrMaterialSSBOAlloc_ = nullptr;
    VkDescriptorPool pbrDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout pbrDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet pbrDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout pbrPipelineLayout_ = VK_NULL_HANDLE;
    VkSampler pbrDefaultSampler_ = VK_NULL_HANDLE;
    uint32_t pbrNextTextureIndex_ = 0;

    // G-Buffer (RTXGI inputs)
    VkImage gbufferAlbedoImage_ = VK_NULL_HANDLE;
    VkImage gbufferNormalRoughnessImage_ = VK_NULL_HANDLE;
    VkImage gbufferDepthImage_ = VK_NULL_HANDLE;
    void* gbufferAlbedoAlloc_ = nullptr;
    void* gbufferNormalRoughnessAlloc_ = nullptr;
    void* gbufferDepthAlloc_ = nullptr;
    VkImageView gbufferAlbedoView_ = VK_NULL_HANDLE;
    VkImageView gbufferNormalRoughnessView_ = VK_NULL_HANDLE;
    VkImageView gbufferDepthView_ = VK_NULL_HANDLE;
    VkRenderPass gbufferRenderPass_ = VK_NULL_HANDLE;
    uint32_t gbufferWidth_ = 0;
    uint32_t gbufferHeight_ = 0;
};

} // namespace Engine
