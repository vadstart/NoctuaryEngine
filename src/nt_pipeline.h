#pragma once

#include "nt_device.h"
#include "vulkan/vulkan_core.h"

#include <string>
#include <vector>

using std::string, std::vector;

namespace nt {

  struct PipelineConfigInfo { 
    PipelineConfigInfo() = default;
    PipelineConfigInfo(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

    VkPipelineViewportStateCreateInfo viewportInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;
  };

  class NtPipeline {
    public:
      NtPipeline(
          NtDevice& device,
          const PipelineConfigInfo& configInfo, 
          const string& vertFilepath, 
          const string& fragFilepath);
      ~NtPipeline();

      NtPipeline(const NtPipeline&) = delete;
      NtPipeline &operator=(const NtPipeline&) = delete;

      void bind(VkCommandBuffer commandBuffer);

      static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);

    private:
      static vector<char> readFile(const string& filepath);

      void createGraphicalPipeline(
          const PipelineConfigInfo& configInfo, 
          const string& vertFilepath, 
          const string& fragFilepath);

      void createShaderModule(const vector<char>& code, VkShaderModule* shaderModule);

      NtDevice& ntDevice;
      VkPipeline graphicsPipeline;
      VkShaderModule vertShaderModule;
      VkShaderModule fragShaderModule;
  };
}

