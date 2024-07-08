// The MIT License(MIT)
//
// Copyright(c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files(the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "NVSharpen.h"

#include <iostream>
#include <array>
#include <filesystem>

#include "VKUtilities.h"
#include "../vulkan/vulkan_utils.h"


NVSharpen::NVSharpen(VulkanDevice& deviceRef, const std::vector<std::string>& shaderPaths, bool glsl)
    : m_DeviceRef(deviceRef) , m_OutputWidth(1), m_OutputHeight(1)
{
    NISOptimizer opt(false, NISGPUArchitecture::NVIDIA_Generic);
    m_BlockWidth = opt.GetOptimalBlockWidth();
    m_BlockHeight = opt.GetOptimalBlockHeight();
    uint32_t threadGroupSize = opt.GetOptimalThreadGroupSize();

    // Shader
    {
        std::string shaderName = glsl ? "/nis_sharpen_glsl.spv" : "/nis_sharpen.spv";
        std::string shaderPath;
        for (auto& e : shaderPaths)
        {
            if (std::filesystem::exists(e + "/" + shaderName))
            {
                shaderPath = e + "/" + shaderName;
                break;
            }
        }
        if (shaderPath.empty())
            throw std::runtime_error("Shader file not found" + shaderName);

        auto shaderBytes = readBytes(shaderPath);
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = shaderBytes.size();
        info.pCode = reinterpret_cast<uint32_t*>(shaderBytes.data());
        VK_CHECK_RESULT(vkCreateShaderModule(m_DeviceRef.GetDevice(), &info, nullptr, &m_ShaderModule));
    }

    // Texture sampler
    {
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        VK_CHECK_RESULT(vkCreateSampler(m_DeviceRef.GetDevice(), &info, nullptr, &m_Sampler));
    }

    // Descriptor set
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindLayout
        {
            { VK_COMMON_DESC_LAYOUT(&m_Sampler) }
        };

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = (uint32_t)bindLayout.size();
        info.pBindings = bindLayout.data();
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_DeviceRef.GetDevice(), &info, nullptr, &m_DescriptorSetLayout));
    }

    {
        VkDescriptorSetAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = m_DeviceRef.GetDescriptorPool();
        info.descriptorSetCount = 1;
        info.pSetLayouts = &m_DescriptorSetLayout;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_DeviceRef.GetDevice(), &info, &m_DescriptorSet));
    }

    // Constant buffer
    {
        m_ConstantBuffer = std::make_unique<VulkanBuffer>(
                m_DeviceRef,
                sizeof(NISConfig),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_ConstantBuffer->Map();

        VkDescriptorBufferInfo descBuffInfo = m_ConstantBuffer->DescriptorInfoForIndex(0);
        VkWriteDescriptorSet writeDescSet{};
        writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescSet.dstSet = m_DescriptorSet;
        writeDescSet.dstBinding = CB_BINDING;
        writeDescSet.descriptorCount = 1;
        writeDescSet.descriptorType = CB_DESC_TYPE;
        writeDescSet.dstArrayElement = 0;
        writeDescSet.pBufferInfo = &descBuffInfo;
        vkUpdateDescriptorSets(m_DeviceRef.GetDevice(), 1, &writeDescSet, 0, nullptr);
    }

    // Pipeline layout
    {
        VkPushConstantRange pushConstRange{};
        pushConstRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstRange.size = sizeof(m_NisConfig);
        VkPipelineLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount = 1;
        info.pSetLayouts = &m_DescriptorSetLayout;
        info.pushConstantRangeCount = 1;
        info.pPushConstantRanges = &pushConstRange;

        VK_CHECK_RESULT(vkCreatePipelineLayout(m_DeviceRef.GetDevice(), &info, nullptr, &m_PipelineLayout));
    }

    // Compute pipeline
    {
        VkPipelineShaderStageCreateInfo pipeShaderStageCreateInfo{};
        pipeShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeShaderStageCreateInfo.module = m_ShaderModule;
        pipeShaderStageCreateInfo.pName = "main";

        VkComputePipelineCreateInfo csPipeCreateInfo{};
        csPipeCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        csPipeCreateInfo.stage = pipeShaderStageCreateInfo;
        csPipeCreateInfo.layout = m_PipelineLayout;
        VK_CHECK_RESULT(vkCreateComputePipelines(m_DeviceRef.GetDevice(), VK_NULL_HANDLE, 1, &csPipeCreateInfo, nullptr, &m_Pipeline));
    }
}

void NVSharpen::Cleanup()
{
    vkDestroyPipeline(m_DeviceRef.GetDevice(), m_Pipeline, nullptr);
    vkDestroyPipelineLayout(m_DeviceRef.GetDevice(), m_PipelineLayout, nullptr);
    vkFreeDescriptorSets(m_DeviceRef.GetDevice(), m_DeviceRef.GetDescriptorPool(), 1, &m_DescriptorSet);
    vkDestroyDescriptorSetLayout(m_DeviceRef.GetDevice(), m_DescriptorSetLayout, nullptr);
    vkDestroySampler (m_DeviceRef.GetDevice(), m_Sampler, nullptr);
    vkDestroyShaderModule(m_DeviceRef.GetDevice(), m_ShaderModule, nullptr);
}

void NVSharpen::Update(float sharpness, uint32_t inputWidth, uint32_t inputHeight)
{
    NVSharpenUpdateConfig(m_NisConfig, sharpness,
                          0, 0,
                          inputWidth, inputHeight,
                          inputWidth, inputHeight,
                          0, 0,
                          NISHDRMode::None);
    m_OutputWidth = inputWidth;
    m_OutputHeight = inputHeight;
}

void NVSharpen::Dispatch(VkCommandBuffer cmdBuffer, VkImageView inputImageView, VkImageView outputImageView)
{
    m_ConstantBuffer->WriteToBuffer(&m_NisConfig);

    VkWriteDescriptorSet inWriteDescSet{};
    VkWriteDescriptorSet outWriteDescSet{};

    VkDescriptorImageInfo inDescInfo{};
    inDescInfo.imageView = inputImageView;
    inDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inWriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inWriteDescSet.dstSet = m_DescriptorSet;
    inWriteDescSet.dstBinding = IN_TEX_BINDING;
    inWriteDescSet.descriptorCount = 1;
    inWriteDescSet.descriptorType = IN_TEX_DESC_TYPE;
    inWriteDescSet.pImageInfo = &inDescInfo;

    VkDescriptorImageInfo outDescInfo{};
    outDescInfo.imageView = outputImageView;
    outDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outWriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outWriteDescSet.dstSet = m_DescriptorSet;
    outWriteDescSet.dstBinding = OUT_TEX_BINDING;
    outWriteDescSet.descriptorCount = 1;
    outWriteDescSet.descriptorType = OUT_TEX_DESC_TYPE;
    outWriteDescSet.pImageInfo = &outDescInfo;
    const VkWriteDescriptorSet writeDescSets[] =
    {
        inWriteDescSet,
        outWriteDescSet
    };
    constexpr auto sizeWriteDescSets = static_cast<uint32_t>(std::size(writeDescSets));
    vkUpdateDescriptorSets(m_DeviceRef.GetDevice(), sizeWriteDescSets, writeDescSets, 0, nullptr);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_PipelineLayout,
            0, 1,
            &m_DescriptorSet,
            0,
            VK_NULL_HANDLE);

    auto gridX = uint32_t(std::ceil(m_OutputWidth / float(m_BlockWidth)));
    auto gridY = uint32_t(std::ceil(m_OutputHeight / float(m_BlockHeight)));
    vkCmdDispatch(cmdBuffer, gridX, gridY, 1);
}

NVSharpen::~NVSharpen()
{
    Cleanup();
}
