#include "nt_animation.hpp"
#include <cmath>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <ostream>
#include <vector>

#include "nt_model.hpp"

namespace nt {

NtAnimator::NtAnimator(NtModel& model) : model(model) {
    if (model.hasSkeleton()) {
        boneMatrices.resize(model.getBonesCount(), glm::mat4(1.0f));
    }
}

void NtAnimator::play(const std::string& animationName, bool loop) {
    for (const auto& anim : model.getAnimations()) {
        if (anim.name == animationName) {
            currentAnimation = &anim;
            currentTime = 0.0f;
            isLooping = loop;
            return;
        }
    }
    std::cerr << "Animation not found: " << animationName << std::endl;
}

void NtAnimator::update(float deltaTime) {
    if (currentAnimation == nullptr || !model.hasSkeleton()) return;

    currentTime += deltaTime;
    if (currentTime > currentAnimation->duration) {
        if (isLooping) {
            currentTime = std::fmod(currentTime, currentAnimation->duration);
        } else {
            currentTime = currentAnimation->duration;
        }
    }

    std::optional<NtModel::Skeleton>& skeletonOpt = const_cast<std::optional<NtModel::Skeleton>&>(model.getSkeleton());
    if (!skeletonOpt.has_value()) return;
    NtModel::Skeleton& skeleton = skeletonOpt.value();

    // Update node TRS from animation
    for (size_t chanIdx = 0; chanIdx < currentAnimation->channels.size(); ++chanIdx) {
        const auto& channel = currentAnimation->channels[chanIdx];

        if (channel.samplerIndex >= currentAnimation->samplers.size()) {
            std::cerr << "[ANIM ERROR] Invalid sampler index!" << std::endl;
            continue;
        }

        if (channel.targetNode >= static_cast<int>(skeleton.bones.size())) {
            std::cerr << "[ANIM ERROR] Target node " << channel.targetNode
                      << " out of range (joints size: " << skeleton.bones.size() << ")" << std::endl;
            continue;
        }

        const NtAnimationSampler& sampler = currentAnimation->samplers[channel.samplerIndex];
        glm::vec4 value = interpolateSampler(sampler, currentTime);

        NtModel::Bone& targetBone = skeleton.bones[channel.targetNode];

        switch (channel.path) {
            case NtAnimationChannel::TRANSLATION:
                targetBone.animatedNodeTranslation = glm::vec3(value);
                break;
            case NtAnimationChannel::ROTATION:
                targetBone.animatedNodeRotation = glm::quat(value.w, value.x, value.y, value.z);
                break;
            case NtAnimationChannel::SCALE:
                targetBone.animatedNodeScale = glm::vec3(value);
                break;
        }
    }
}

glm::vec4 NtAnimator::interpolateSampler(const NtAnimationSampler& sampler, float time) {
    // Safety checks
    if (sampler.inputTimestamps.empty()) {
        return glm::vec4(0.0f);
    }

    if (sampler.inputTimestamps.size() == 1) {
        return sampler.outputValues[0];
    }

    // Find surrounding keyframes
    size_t nextFrame = 0;
    for (size_t i = 0; i < sampler.inputTimestamps.size() - 1; ++i) {
        if (time < sampler.inputTimestamps[i + 1]) {
            nextFrame = i + 1;
            break;
        }
    }
    size_t prevFrame = (nextFrame == 0) ? 0 : nextFrame - 1;

    if (sampler.interpolation == NtAnimationSampler::STEP) {
        return sampler.outputValues[prevFrame];
    }

    // Linear interpolation
    float t0 = sampler.inputTimestamps[prevFrame];
    float t1 = sampler.inputTimestamps[nextFrame];
    float duration = t1 - t0;

    if (duration < 0.0001f) {
        return sampler.outputValues[prevFrame];
    }

    float factor = (time - t0) / (t1 - t0);
    factor = glm::clamp(factor, 0.0f, 1.0f);

    return glm::mix(sampler.outputValues[prevFrame],
                    sampler.outputValues[nextFrame],
                    factor);
}

}
