#pragma once

#include <string>
#include <vector>
#include <optional>
#include <vulkan/vulkan.h>

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;
};

class VulkanDevice
{
public:
    explicit VulkanDevice();
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    VkCommandPool GetGraphicsCommandPool() { return m_GraphicsCommandPool; }
    VkCommandPool GetComputeCommandPool() { return m_ComputeCommandPool; }
    VkDevice GetDevice() { return m_LogicalDevice; }
    VkQueue GetComputeQueue() { return m_ComputeQueue; }
    VkPhysicalDevice GetPhysicalDevice() { return m_PhysicalDevice; }
    VkDescriptorPool GetDescriptorPool() { return m_DescriptorPool; }

    std::optional<uint32_t> FindComputeQueueFamily(VkPhysicalDevice device);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkPhysicalDeviceProperties PhysicalDeviceProperties{};

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommand(VkCommandBuffer commandBuffer);

    void CreateBuffer(
            VkDeviceSize size,
            VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
            VkBuffer &buffer, VkDeviceMemory &bufferMemory);

private:
    void CreateInstance();
    void SetupDebugMessenger();
    void SelectPhysicalDevice();
    void CreateLogicalDevice();
    void CreateComputeCommandPool();

    bool IsDeviceSuitable(VkPhysicalDevice device);
    [[nodiscard]] std::vector<const char*> GetRequiredExtensions() const;
    bool CheckValidationLayerSupport();

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
    void HasGLFWRequiredInstanceExtensions();
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

    std::optional<uint32_t> m_ComputeFamily;
    VkDescriptorPool m_DescriptorPool;
    bool m_EnableValidationLayers = true;
    VkInstance m_Instance{};
    VkDebugUtilsMessengerEXT m_DebugMessenger{};
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkCommandPool m_GraphicsCommandPool{};
    VkCommandPool m_ComputeCommandPool{};

    VkDevice m_LogicalDevice{};
    VkSurfaceKHR m_Surface{};
    VkQueue m_GraphicsQueue{};
    VkQueue m_PresentQueue{};
    VkQueue m_ComputeQueue{};

    const std::vector<const char *> m_ValidationLayers = {
            "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char *> m_DeviceExtensions = {
    };
};