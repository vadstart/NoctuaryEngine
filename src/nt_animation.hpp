#pragma once

#include <glm/glm.hpp>
#include <glm/fwd.hpp>
#include <string>
#include <vector>

namespace nt {

class NtModel; // Forward declaration

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


class NtAnimator {
public:
    NtAnimator(NtModel& model);

    void play(const std::string& animationName, bool loop = false);
    void update(float deltaTime);

    const std::vector<glm::mat4>& getBoneMatrices() const { return boneMatrices; }
    bool isPlaying() const { return currentAnimation != nullptr; }
    std::string getCurrentAnimationName() const { return currentAnimation->name; }
    float getCurrentTime() const { return currentTime; }
    float getDuration() const { return currentAnimation->duration; }

private:
    NtModel& model;
    const NtAnimation* currentAnimation = nullptr;
    float currentTime = 0.0f;
    bool isLooping = true;

    std::vector<glm::mat4> boneMatrices; // Final matrices for the shader

    void updateBoneTransforms();
    glm::vec4 interpolateSampler(const NtAnimationSampler& sampler, float time);
};

}
