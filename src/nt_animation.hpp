#pragma once

#include <glm/fwd.hpp>
#include <string>
#include <vector>

namespace nt {

struct NtAnimationSampler {
    std::vector<float> inputTimestamps;
    std::vector<glm::vec4> outputValues; // Translation/Rotation/Scale
    enum Interpolation { LINEAR, STEP, CUBICSPLINE } interpolation;
};

struct NtAnimationChannel {
    int samplerIndex;
    int targetNode;
    enum TargetPath { TRANSLATION, ROTATION, SCALE } path;
};

struct NtAnimation {
    std::string name;
    float duration;
    std::vector<NtAnimationSampler> samplers;
    std::vector<NtAnimationChannel> channels;
};

}
