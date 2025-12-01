#pragma once

#include "nt_ecs.hpp"

namespace nt
{

class AnimationSystem : public NtSystem
{
public:
    AnimationSystem(NtAstral* astral_ptr) : astral(astral_ptr) {};
    ~AnimationSystem() {};

    void update(float dt);

private:
    NtAstral* astral;
};

}
