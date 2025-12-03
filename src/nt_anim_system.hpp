#pragma once

#include "nt_ecs.hpp"

namespace nt
{

class AnimationSystem : public NtSystem
{
public:
    AnimationSystem(NtNexus* nexus_ptr) : nexus(nexus_ptr) {};
    ~AnimationSystem() {};

    void update(float dt);

private:
    NtNexus* nexus;
};

}
