#include "astral_app.h"

#include <stdexcept>

namespace nt
{

AstralApp::AstralApp() {
  createPipelineLayout();
  createPipeline();
  createCommandBuffers();
}
AstralApp::~AstralApp() {
  vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void AstralApp::run() {

  while (!ntWindow.shouldClose()) {
    glfwPollEvents();
  }
}

 void AstralApp::createPipelineLayout() {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  if (vkCreatePipelineLayout(ntDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

 }
    
  void AstralApp::createPipeline() {
    auto pipelineConfig = NtPipeline::defaultPipelineConfigInfo(ntSwapChain.width(), ntSwapChain.height());
    pipelineConfig.renderPass = ntSwapChain.getRenderPass();
    pipelineConfig.pipelineLayout = pipelineLayout;
    ntPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        pipelineConfig,
        "src/shaders/simple_shader.vert.spv",
        "src/shaders/simple_shader.frag.spv");
  }

void AstralApp::createCommandBuffers() {}
void AstralApp::drawFrame() {}


}
