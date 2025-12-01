#pragma once

#include "nt_ecs.hpp"
#include "nt_pipeline.hpp"
#include "nt_device.hpp"
#include "nt_swap_chain.hpp"
#include "nt_types.hpp"
#include "nt_frame_info.hpp"
#include "vulkan/vulkan_core.h"

#include <memory>
using std::vector;

namespace nt
{

class RenderSystem : public NtSystem
{
public:
    RenderSystem(NtAstral* astral_ptr, NtDevice &device,
                        NtSwapChain &swapChain,
                        VkDescriptorSetLayout globalSetLayout,
                        VkDescriptorSetLayout modelSetLayout,
                        VkDescriptorSetLayout boneSetLayout);
    ~RenderSystem();

    RenderSystem(const RenderSystem &) = delete;
    RenderSystem &operator=(const RenderSystem &) = delete;\

    void renderGameObjects(FrameInfo &frameInfo, bool bShadowPass = false);
    // void renderLightBillboards(FrameInfo &frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout,
                            VkDescriptorSetLayout modelSetLayout,
                            VkDescriptorSetLayout boneSetLayout);
    void createPipelines(NtSwapChain &swapChain);

    NtDevice &ntDevice;
    NtAstral* astral;

    std::unique_ptr<NtPipeline> shadowMapPipeline;
    std::unique_ptr<NtPipeline> pbrPipeline;
    std::unique_ptr<NtPipeline> nprPipeline;
    // std::unique_ptr<NtPipeline> billboardPipeline;
    VkPipelineLayout pipelineLayout;


};

}
