#pragma once

#include <string>
#include "vulkan/vulkan_device.h"
#include "nv/NVSharpen.h"

class VkNVSharpen
{
public:
    VkNVSharpen();
    ~VkNVSharpen();
    void ProcessImage(const std::string& inputImagePath, const std::string& outputDirectoryPath);
    void SetSharpness(float sharpness) { m_CurrentSharpness = sharpness;}

private:
    void Initialize();
    void LoadInputImage();
    void CreateTextures();
    void CreateCommandBufferAndFence();
    void UpdateNVSharpen();
    void DispatchComputeShader();
    void SubmitAndWait();
    void SaveOutputImage();
    void Cleanup();


    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags buffUsage, VkMemoryPropertyFlags memProps, VkBuffer* outBuffer, VkDeviceMemory* outBuffMem);
    void CreateTexture2D(int w, int h, VkFormat format, VkImage* outImage, VkDeviceMemory* outDeviceMemory);
    void CreateTexture2D(int w, int h, VkFormat format, const void* data, uint32_t rowPitch, uint32_t imageSize, VkImage* outImage, VkDeviceMemory* outDeviceMemory);
    void CreateSRV(VkImage inputImage, VkFormat format, VkImageView* outSrv);
    void TransitionImageLayout(
            VkCommandBuffer commandBuffer,
            VkImage image,
            VkImageLayout oldLayout, VkImageLayout newLayout);
private:
    std::string m_CurrentFilePath;
    std::string m_CurrentInputImageName;
    std::string m_OutputDirectory;
    VulkanDevice* m_Device{};
    NVSharpen* m_NVSharpen{};
    std::vector<uint8_t> m_CurrentImageData;
    uint32_t m_CurrentImageWidth{}, m_CurrentImageHeight{};
    uint32_t m_CurrentImageRowPitchAlignment{};
    uint32_t m_CurrentImageOutputWidth{}, m_CurrentImageOutputHeight{};
    VkCommandBuffer m_ComputeCommandBuffer{};
    VkFence m_ComputeFence{};
    float m_CurrentSharpness = 100.0f;

    VkImage m_InputImage{};
    VkDeviceMemory m_InputImageMemory{};
    VkImageView m_InputImageView{};
    VkImage m_OutputImage{};
    VkDeviceMemory m_OutputImageMemory{};
    VkImageView m_OutputImageView{};

    void FreeImageResources();
};