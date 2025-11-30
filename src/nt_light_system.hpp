#pragma once

#include "nt_ecs.hpp"
#include "nt_frame_info.hpp"
#include "nt_swap_chain.hpp"

namespace nt
{

class LightSystem : public NtSystem
{
public:
    void updateLights(NtAstral &astral, FrameInfo &frameInfo, GlobalUbo &ubo, glm::vec3 O_dir,
                    float O_scale, float O_near, float O_far);
};

}
