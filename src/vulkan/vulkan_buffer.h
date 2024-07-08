#pragma once

#include "vulkan_device.h"

class VulkanBuffer
{
public:
    VulkanBuffer(
        VulkanDevice& device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment = 1);

    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Unmap();

    void WriteToBuffer(const void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
    VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
    [[nodiscard]] VkDescriptorBufferInfo DescriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
    VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    void WriteToIndex(void* data, int index) const;
    [[nodiscard]] VkResult FlushIndex(int index) const;
    VkDescriptorBufferInfo DescriptorInfoForIndex(int index) const;
    VkResult InvalidateIndex(int index);

    [[nodiscard]] VkBuffer GetBuffer() const { return m_Buffer; }
    [[nodiscard]] void* GetMappedMemory() const { return m_Mapped; }
    [[nodiscard]] uint32_t GetInstanceCount() const { return m_InstanceCount; }
    [[nodiscard]] VkDeviceSize GetInstanceSize() const { return m_InstanceSize; }
    [[nodiscard]] VkDeviceSize GetAlignmentSize() const { return m_InstanceSize; }
    [[nodiscard]] VkBufferUsageFlags GetUsageFlags() const { return m_UsageFlags; }
    [[nodiscard]] VkMemoryPropertyFlags GetMemoryPropertyFlags() const { return m_MemoryPropertyFlags; }
    [[nodiscard]] VkDeviceSize GetBufferSize() const { return m_BufferSize; }

private:
    static VkDeviceSize GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    VulkanDevice& m_VulkanDevice;
    void* m_Mapped = nullptr;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;

    VkDeviceSize m_BufferSize;
    uint32_t m_InstanceCount;
    VkDeviceSize m_InstanceSize;
    VkDeviceSize m_AlignmentSize;
    VkBufferUsageFlags m_UsageFlags;
    VkMemoryPropertyFlags m_MemoryPropertyFlags;
};
