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

#pragma once

#include <iostream>
#include <memory>

#include "VKUtilities.h"
#include "../../NIS/NIS_Config.h"
#include "../vulkan/vulkan_device.h"
#include "../vulkan/vulkan_buffer.h"

class NVSharpen
{
public:
    NVSharpen(VulkanDevice& deviceRef, const std::vector<std::string>& shaderPaths, bool glsl);
    ~NVSharpen();
    void Update(float sharpness, uint32_t inputWidth, uint32_t inputHeight);
    void Dispatch(VkCommandBuffer cmdBuffer, VkImageView inputImageView, VkImageView outputImageView);
    void Cleanup();
private:
    VulkanDevice&                    m_DeviceRef;
    NISConfig                        m_NisConfig{};
    std::unique_ptr<VulkanBuffer>    m_ConstantBuffer;

    VkShaderModule                      m_ShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout               m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout                    m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet                     m_DescriptorSet = VK_NULL_HANDLE;
    VkPipeline                          m_Pipeline = VK_NULL_HANDLE;
    VkSampler                           m_Sampler{};

    uint32_t                            m_OutputWidth;
    uint32_t                            m_OutputHeight;
    uint32_t                            m_BlockWidth;
    uint32_t                            m_BlockHeight;
};