#include "vk_nv_sharpen.h"
#include "common/Image.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_utils.h"
#include <map>
#include <filesystem>
#include <cstring>

VkNVSharpen::VkNVSharpen()
{
    Initialize();
}

VkNVSharpen::~VkNVSharpen()
{
    Cleanup();
}

void VkNVSharpen::Initialize()
{
    m_Device = new VulkanDevice();
    m_NVSharpen = new NVSharpen(*m_Device, std::vector<std::string>({ "NIS/", "../../../NIS/", "." }), false);
    CreateCommandBufferAndFence();
}

void VkNVSharpen::LoadInputImage()
{
    img::load(m_CurrentFilePath,
              m_CurrentImageData,
              m_CurrentImageWidth, m_CurrentImageHeight,
              m_CurrentImageRowPitchAlignment,
              img::Fmt::R8G8B8A8);

    m_CurrentImageOutputWidth = m_CurrentImageWidth;
    m_CurrentImageOutputHeight = m_CurrentImageHeight;
}

void VkNVSharpen::CreateTextures()
{
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    // Create input texture
    CreateTexture2D(
            m_CurrentImageWidth,
            m_CurrentImageHeight,
            format,
            m_CurrentImageData.data(),
            m_CurrentImageRowPitchAlignment,
            m_CurrentImageData.size(),
            &m_InputImage,
            &m_InputImageMemory
    );

    // Create input image view
    CreateSRV(m_InputImage, format, &m_InputImageView);

    // Create output texture
    CreateTexture2D(
            m_CurrentImageOutputWidth,
            m_CurrentImageOutputHeight,
            format,
            &m_OutputImage,
            &m_OutputImageMemory
    );

    // Create output image view
    CreateSRV(m_OutputImage, format, &m_OutputImageView);
}


void VkNVSharpen::CreateCommandBufferAndFence()
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_Device->GetComputeCommandPool();
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_Device->GetDevice(), &commandBufferAllocateInfo, &m_ComputeCommandBuffer);

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK_RESULT(vkCreateFence(m_Device->GetDevice(), &fenceCreateInfo, nullptr, &m_ComputeFence));
}

void VkNVSharpen::UpdateNVSharpen()
{
    m_NVSharpen->Update(
            m_CurrentSharpness / 100.0f,
            m_CurrentImageWidth,
            m_CurrentImageHeight);
}

void VkNVSharpen::DispatchComputeShader()
{
    VkCommandBufferBeginInfo cmdBufferBeginInfo{};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_RESULT(vkBeginCommandBuffer(m_ComputeCommandBuffer, &cmdBufferBeginInfo));

    // Transition input image to shader read optimal if it's not already
    TransitionImageLayout(
            m_ComputeCommandBuffer,
            m_InputImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Transition output image to general layout for compute shader write
    TransitionImageLayout(
            m_ComputeCommandBuffer,
            m_OutputImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL
    );

    m_NVSharpen->Dispatch(
            m_ComputeCommandBuffer,
            m_InputImageView,
            m_OutputImageView
    );

    // Transition output image to transfer src for potential read back
    TransitionImageLayout(
            m_ComputeCommandBuffer,
            m_OutputImage,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );

    VK_CHECK_RESULT(vkEndCommandBuffer(m_ComputeCommandBuffer));

    SubmitAndWait();
}

void VkNVSharpen::SubmitAndWait()
{
    vkResetFences(m_Device->GetDevice(), 1, &m_ComputeFence);

    const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_ComputeCommandBuffer;
    VK_CHECK_RESULT(vkQueueSubmit(m_Device->GetComputeQueue(), 1, &submitInfo, m_ComputeFence));
    VK_CHECK_RESULT(vkWaitForFences(m_Device->GetDevice(), 1, &m_ComputeFence, VK_TRUE, UINT64_MAX));
    vkQueueWaitIdle(m_Device->GetComputeQueue());
}

std::string FloatToString(float value, int precision = 2)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    std::string str = oss.str();

    // Remove trailing zeros
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);

    // If the last character is a decimal point, remove it
    if (str.back() == '.')
        str.pop_back();

    return str;
}

void VkNVSharpen::SaveOutputImage()
{
    VkDeviceSize imageSize = m_CurrentImageOutputWidth * m_CurrentImageOutputHeight * 4;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    VkCommandBuffer cmdBuffer = m_Device->BeginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {m_CurrentImageOutputWidth, m_CurrentImageOutputHeight, 1};

    vkCmdCopyImageToBuffer(
            cmdBuffer,
            m_OutputImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stagingBuffer, 1, &region);

    m_Device->EndSingleTimeCommand(cmdBuffer);

    // Transition the image back to SHADER_READ_ONLY_OPTIMAL layout if needed
    cmdBuffer = m_Device->BeginSingleTimeCommands();
    TransitionImageLayout(cmdBuffer,
                          m_OutputImage,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_Device->EndSingleTimeCommand(cmdBuffer);

    void* data;
    vkMapMemory(m_Device->GetDevice(), stagingBufferMemory, 0, imageSize, 0, &data);

    VkMappedMemoryRange memoryRange = {};
    memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    memoryRange.memory = stagingBufferMemory;
    memoryRange.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(m_Device->GetDevice(), 1, &memoryRange);

    std::string outputName = m_CurrentInputImageName + "_NVSharpened_" + FloatToString(m_CurrentSharpness) + "%.png";
    std::filesystem::path outputPath = std::filesystem::path(m_OutputDirectory) / outputName;
    std::string fullOutputPath = outputPath.string();
    img::savePNG(
            fullOutputPath,
            static_cast<uint8_t*>(data),
            m_CurrentImageOutputWidth,
            m_CurrentImageOutputHeight,
            4,
            m_CurrentImageOutputWidth * 4,
            img::Fmt::R8G8B8A8);
    vkUnmapMemory(m_Device->GetDevice(), stagingBufferMemory);

    vkDestroyBuffer(m_Device->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_Device->GetDevice(), stagingBufferMemory, nullptr);
}

void VkNVSharpen::Cleanup()
{
    vkDestroyFence(m_Device->GetDevice(), m_ComputeFence, nullptr);
    vkFreeCommandBuffers(m_Device->GetDevice(), m_Device->GetComputeCommandPool(), 1, &m_ComputeCommandBuffer);
    delete m_NVSharpen;
    delete m_Device;
}

void VkNVSharpen::ProcessImage(const std::string& inputImagePath, const std::string& outputDirPath)
{
    m_CurrentFilePath = inputImagePath;
    m_OutputDirectory = outputDirPath;
    std::filesystem::path path(inputImagePath);
    m_CurrentInputImageName = path.stem().string();

    LoadInputImage();
    CreateTextures();
    UpdateNVSharpen();
    DispatchComputeShader();
    SaveOutputImage();

    FreeImageResources();
}

void VkNVSharpen::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags buffUsage, VkMemoryPropertyFlags memProps, VkBuffer* outBuffer, VkDeviceMemory* outBuffMem)
{
    {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = buffUsage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(vkCreateBuffer(m_Device->GetDevice(), &info, nullptr, outBuffer));
    }
    {
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_Device->GetDevice(), *outBuffer, &memReqs);

        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = memReqs.size;
        info.memoryTypeIndex = m_Device->FindMemoryType(memReqs.memoryTypeBits, memProps);
        VK_CHECK_RESULT(vkAllocateMemory(m_Device->GetDevice(), &info, nullptr, outBuffMem));
    }

    VK_CHECK_RESULT(vkBindBufferMemory(m_Device->GetDevice(), *outBuffer, *outBuffMem, 0));
}

void VkNVSharpen::CreateTexture2D(int w, int h, VkFormat format, const void* data, uint32_t rowPitch, uint32_t imageSize, VkImage* outImage, VkDeviceMemory* outDeviceMemory)
{
    auto width = static_cast<uint32_t>(w);
    auto height = static_cast<uint32_t>(h);

    VkBuffer stagingBuff;
    VkDeviceMemory stagingBuffMem;

    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuff, &stagingBuffMem);

    void* mappedMem;
    VK_CHECK_RESULT(vkMapMemory(m_Device->GetDevice(), stagingBuffMem, 0, imageSize, 0, &mappedMem));
    memcpy(mappedMem, data, imageSize);
    vkUnmapMemory(m_Device->GetDevice(), stagingBuffMem);

    // Create texture image object and backing memory
    {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.extent.width = width;
        info.extent.height = height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.format = format;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.samples = VK_SAMPLE_COUNT_1_BIT;

        VK_CHECK_RESULT(vkCreateImage(m_Device->GetDevice(), &info, nullptr, outImage));
    }
    {
        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(m_Device->GetDevice(), *outImage, &memReq);

        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = memReq.size;
        info.memoryTypeIndex = m_Device->FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(m_Device->GetDevice(), &info, nullptr, outDeviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(m_Device->GetDevice(), *outImage, *outDeviceMemory, 0));
    }
    VkCommandBuffer cmdBuff = m_Device->BeginSingleTimeCommands();
    {
        VkImageMemoryBarrier transferBarrier{};
        transferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transferBarrier.srcAccessMask = 0;
        transferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transferBarrier.image = *outImage;
        transferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        transferBarrier.subresourceRange.levelCount = 1;
        transferBarrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transferBarrier);

        VkBufferImageCopy buffImageCopyRegion{};
        buffImageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        buffImageCopyRegion.imageSubresource.layerCount = 1;
        buffImageCopyRegion.imageExtent = { width, height, 1 };
        vkCmdCopyBufferToImage(cmdBuff, stagingBuff, *outImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffImageCopyRegion);

        VkImageMemoryBarrier useBarrier{};
        useBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        useBarrier.image = *outImage;
        useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        useBarrier.subresourceRange.levelCount = 1;
        useBarrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &useBarrier);
    }
    m_Device->EndSingleTimeCommand(cmdBuff);

    vkFreeMemory(m_Device->GetDevice(), stagingBuffMem, nullptr);
    vkDestroyBuffer(m_Device->GetDevice(), stagingBuff, nullptr);
}

void VkNVSharpen::CreateTexture2D(int w, int h, VkFormat format, VkImage* outImage, VkDeviceMemory* outDeviceMemory)
{
    auto width = static_cast<uint32_t>(w);
    auto height = static_cast<uint32_t>(h);

    // Create texture image object and backing memory
    {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.extent.width = width;
        info.extent.height = height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.format = format;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.samples = VK_SAMPLE_COUNT_1_BIT;

        VK_CHECK_RESULT(vkCreateImage(m_Device->GetDevice(), &info, nullptr, outImage));
    }
    {
        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(m_Device->GetDevice(), *outImage, &memReq);

        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = memReq.size;
        info.memoryTypeIndex = m_Device->FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(m_Device->GetDevice(), &info, nullptr, outDeviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(m_Device->GetDevice(), *outImage, *outDeviceMemory, 0));
    }
}

void VkNVSharpen::CreateSRV(VkImage inputImage, VkFormat format, VkImageView* outSrv)
{
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = inputImage;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.levelCount = 1;
    VK_CHECK_RESULT(vkCreateImageView(m_Device->GetDevice(), &info, nullptr, outSrv));
}

void VkNVSharpen::TransitionImageLayout(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            }
    };

    struct LayoutTransitionInfo {
        VkAccessFlags srcAccessMask;
        VkAccessFlags dstAccessMask;
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
    };

    const std::map<std::pair<VkImageLayout, VkImageLayout>, LayoutTransitionInfo> transitionMap = {
            {{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                    {0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT}},
            {{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                    {0, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT}},
            {{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                    {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
            {{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                    {VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
            {{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                    {VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT}},
            {{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL},
                    {0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT}},
            {{VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                    {VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT}},
            {{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL},
                    {VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT}},
            {{VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                    {0, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT}}
    };    auto it = transitionMap.find({oldLayout, newLayout});
    if (it == transitionMap.end()) {
        throw std::runtime_error("Unsupported layout transition!");
    }

    const auto& transitionInfo = it->second;
    barrier.srcAccessMask = transitionInfo.srcAccessMask;
    barrier.dstAccessMask = transitionInfo.dstAccessMask;

    vkCmdPipelineBarrier(
            commandBuffer,
            transitionInfo.sourceStage, transitionInfo.destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
    );
}

void VkNVSharpen::FreeImageResources()
{
    vkDestroyImageView(m_Device->GetDevice(), m_InputImageView, nullptr);
    vkDestroyImage(m_Device->GetDevice(), m_InputImage, nullptr);
    vkFreeMemory(m_Device->GetDevice(), m_InputImageMemory, nullptr);

    vkDestroyImageView(m_Device->GetDevice(), m_OutputImageView, nullptr);
    vkDestroyImage(m_Device->GetDevice(), m_OutputImage, nullptr);
    vkFreeMemory(m_Device->GetDevice(), m_OutputImageMemory, nullptr);
}
