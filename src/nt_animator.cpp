#include "nt_animator.hpp"
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

void NtAnimator::play(const std::string& animationName, bool loop) {
    currentAnimationName = animationName;
    currentTime = 0.0f;
    isLooping = loop;
    isPlaying = true;
    cachedDuration = -1.0f;
}

void NtAnimator::update(NtModel& model, float deltaTime) {
    if (!isPlaying || !model.hasSkeleton()) return;

    // Find animation by name
    const NtAnimation* animation = findAnimation(model, currentAnimationName);
    if (!animation) return;

    // Cache duration on first update
    if (cachedDuration < 0.0f)
        cachedDuration = animation->duration;

    currentTime += deltaTime;

    if (currentTime > animation->duration) {
        if (isLooping) {
            currentTime = std::fmod(currentTime, animation->duration);
        } else {
            currentTime = animation->duration;
        }
    }

    std::optional<NtModel::Skeleton>& skeletonOpt = const_cast<std::optional<NtModel::Skeleton>&>(model.getSkeleton());
    if (!skeletonOpt.has_value()) return;
    NtModel::Skeleton& skeleton = skeletonOpt.value();

    // Update node TRS from animation
    for (size_t chanIdx = 0; chanIdx < animation->channels.size(); ++chanIdx) {
        const auto& channel = animation->channels[chanIdx];

        if (channel.samplerIndex >= animation->samplers.size()) {
            std::cerr << "[ANIM ERROR] Invalid sampler index!" << std::endl;
            continue;
        }

        if (channel.targetNode >= static_cast<int>(skeleton.bones.size())) {
            std::cerr << "[ANIM ERROR] Target node " << channel.targetNode
                      << " out of range (joints size: " << skeleton.bones.size() << ")" << std::endl;
            continue;
        }

        const NtAnimationSampler& sampler = animation->samplers[channel.samplerIndex];
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

} // namespace nt
