#pragma once

#include "nt_device.h"

#include <string>
#include <vector>

using std::string, std::vector;

namespace nt {

  struct PipelineConfigInfo { 
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;
  };

  class NtPipeline {
    public:
      NtPipeline(
          NtDevice& device,
          const PipelineConfigInfo configInfo, 
          const string& vertFilepath, 
          const string& fragFilepath);
      ~NtPipeline();

      NtPipeline(const NtPipeline&) = delete;
      void operator=(const NtPipeline&) = delete;

      void bind(VkCommandBuffer commandBuffer);

      static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);

    private:
      static vector<char> readFile(const string& filepath);

      void createGraphicalPipeline(
          const PipelineConfigInfo configInfo, 
          const string& vertFilepath, 
          const string& fragFilepath);

      void createShaderModule(const vector<char>& code, VkShaderModule* shaderModule);

      NtDevice& ntDevice;
      VkPipeline graphicsPipeline;
      VkShaderModule vertShaderModule;
      VkShaderModule fragShaderModule;
  };
}

