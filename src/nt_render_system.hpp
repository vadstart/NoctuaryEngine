#pragma once

#include "nt_ecs.hpp"
#include "nt_material.hpp"
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
    RenderSystem(NtNexus* nexus_ptr, NtDevice &device,
                    NtSwapChain &swapChain,
                    std::shared_ptr<NtMaterialLibrary> matLibrary);
    ~RenderSystem();

    RenderSystem(const RenderSystem &) = delete;
    RenderSystem &operator=(const RenderSystem &) = delete;

    void render(FrameInfo& frameInfo);
    void renderShadows(FrameInfo& frameInfo);

private:
    void renderBatch(FrameInfo& frameInfo, std::shared_ptr<NtMaterial> material,
        const std::vector<NtEntity>& batch);

    NtDevice &ntDevice;
    NtNexus* nexus;

    std::shared_ptr<NtMaterialLibrary> materialLibrary;
};

}
