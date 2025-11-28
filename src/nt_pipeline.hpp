#pragma once

#include "nt_device.hpp"
#include "nt_types.hpp"
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
    // Dynamic rendering
    VkFormat colorAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
  };

  class NtPipeline {
    public:
      NtPipeline(
          NtDevice& device,
          const PipelineConfigInfo& configInfo,
          const VkPipelineRenderingCreateInfo& pipelineRenderingInfo,
          const string& vertFilepath,
          const string& fragFilepath);
      ~NtPipeline();

      NtPipeline(const NtPipeline&) = delete;
      NtPipeline &operator=(const NtPipeline&) = delete;

      void bind(VkCommandBuffer commandBuffer);

      static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo, RenderMode pipeRenderMode, NtDevice& device);

    private:
      static vector<char> readFile(const string& filepath);

      void createGraphicalPipeline(
          const PipelineConfigInfo& configInfo,
          const VkPipelineRenderingCreateInfo& pipelineRenderingInfo,
          const string& vertFilepath,
          const string& fragFilepath);

      void createShaderModule(const vector<char>& code, VkShaderModule* shaderModule);

      NtDevice& ntDevice;
      VkPipeline graphicsPipeline;
      VkShaderModule vertShaderModule;
      VkShaderModule fragShaderModule;
  };
}
