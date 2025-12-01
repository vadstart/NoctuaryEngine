#pragma once

#include "nt_ecs.hpp"
#include "nt_frame_info.hpp"
#include "nt_swap_chain.hpp"

namespace nt
{

class LightSystem : public NtSystem
{
public:
    LightSystem(NtAstral* astral_ptr) : astral(astral_ptr) {};

    void updateLights(FrameInfo &frameInfo, GlobalUbo &ubo,
                    float O_scale, float O_near, float O_far);

private:
    NtAstral* astral;
};

}
