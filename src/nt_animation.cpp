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
    if (!currentAnimation || !model.hasSkeleton()) return;

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

    // std::cout << "[ANIM] Updating animation, channels: " << currentAnimation->channels.size() << std::endl;

    // Update node TRS from animation
    for (size_t chanIdx = 0; chanIdx < currentAnimation->channels.size(); ++chanIdx) {
        const auto& channel = currentAnimation->channels[chanIdx];

        // std::cout << "[ANIM] Channel " << chanIdx << ": samplerIdx=" << channel.samplerIndex
        //           << ", targetNode=" << channel.targetNode << std::endl;

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

        //&NtModel::Node* targetNode = skeleton.bones[channel.targetNode];
        NtModel::Bone& targetBone = skeleton.bones[channel.targetNode];

        // std::cout << "[ANIM] Updating node: " << targetBone.name << std::endl;

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

    // std::cout << "[ANIM] Computing joint matrices..." << std::endl;

    // // Compute joint matrices (Sascha's approach)
    // if (!skeleton.meshNode) {
    //     std::cerr << "[ANIM ERROR] Mesh node is nullptr!" << std::endl;
    //     return;
    // }

    // glm::mat4 inverseMeshTransform = glm::inverse(getNodeMatrix(skeleton.meshNode));

    // boneMatrices.resize(skeleton.bones.size());
    // for (size_t i = 0; i < skeleton.bones.size(); ++i) {
    //     if (!skeleton.bones[i]) {
    //         std::cerr << "[ANIM ERROR] Joint " << i << " is nullptr!" << std::endl;
    //         continue;
    //     }

    //     glm::mat4 jointMatrix = getNodeMatrix(skeleton.bones[i]) *
    //                             skeleton.bones[i].inverseBindMatrix;
    //     boneMatrices[i] = inverseMeshTransform * jointMatrix;
    // }

    // std::cout << "[ANIM] Update complete" << std::endl;
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

// void NtAnimator::updateBoneTransforms() {
//     const std::optional<NtModel::Skeleton>& skeletonOpt = model.getSkeleton();
//     if (!skeletonOpt.has_value()) {
//         return;
//     }
//     const NtModel::Skeleton& skeleton = skeletonOpt.value();

//     // Build TRS components for each bone from animation
//     std::vector<glm::vec3> translations(skeleton.bones.size());
//     std::vector<glm::quat> rotations(skeleton.bones.size());
//     std::vector<glm::vec3> scales(skeleton.bones.size(), glm::vec3(1.0f));

//     // Decompose bind pose into TRS for each bone
//     for (size_t i = 0; i < skeleton.bones.size(); ++i) {
//         glm::mat4 transform = skeleton.bones[i].localTransform;
//         glm::vec3 skew;
//         glm::vec4 perspective;
//         glm::decompose(transform, scales[i], rotations[i], translations[i], skew, perspective);
//     }

//     // Override with animation data
//     for (const auto& channel : currentAnimation->channels) {
//         // Bounds check
//         if (channel.samplerIndex >= currentAnimation->samplers.size()) {
//             std::cerr << "Invalid sampler index: " << channel.samplerIndex << std::endl;
//             continue;
//         }

//         if (channel.targetNode >= static_cast<int>(skeleton.bones.size())) {
//             std::cerr << "Invalid target node: " << channel.targetNode << std::endl;
//             continue;
//         }

//         const NtAnimationSampler& sampler = currentAnimation->samplers[channel.samplerIndex];
//         glm::vec4 value = interpolateSampler(sampler, currentTime);

//         switch (channel.path) {
//             case nt::NtAnimationChannel::TRANSLATION:
//                 translations[channel.targetNode] = glm::vec3(value);
//                 break;
//             case nt::NtAnimationChannel::ROTATION: {
//                 rotations[channel.targetNode] = glm::quat(value.w, value.x, value.y, value.z);
//                 break;
//             }
//             case nt::NtAnimationChannel::SCALE:
//                 scales[channel.targetNode] = glm::vec3(value);
//                 break;
//         }
//     }

//     // Build local transforms from TRS components
//     std::vector<glm::mat4> localTransforms(skeleton.bones.size());
//     for (size_t i = 0; i < skeleton.bones.size(); ++i) {
//         // Build transform as T * R * S
//         glm::mat4 T = glm::translate(glm::mat4(1.0f), translations[i]);
//         glm::mat4 R = glm::mat4_cast(glm::normalize(rotations[i]));
//         glm::mat4 S = glm::scale(glm::mat4(1.0f), scales[i]);
//         localTransforms[i] = T * R * S;
//     }

//     // Compute global transform (walk hierarchy)
//     std::vector<glm::mat4> globalTransforms(skeleton.bones.size());
//     for (size_t i = 0; i < skeleton.bones.size(); ++i) {
//         if (skeleton.bones[i].parentIndex == -1) {
//             globalTransforms[i] = localTransforms[i];
//         } else {
//             int parentIdx = skeleton.bones[i].parentIndex;
//             if (parentIdx >= 0 && parentIdx < static_cast<int>(globalTransforms.size())) {
//                 globalTransforms[i] = globalTransforms[parentIdx] * localTransforms[i];
//             }
//         }
//     }

//     // KEY FIX: We need the inverse transform of the mesh node (skeleton root)
//     // For now, assume it's at the root bone (bone 0)
//     // In a proper implementation, this should be the transform of the node that has the skin attached
//     glm::mat4 meshNodeTransform = glm::mat4(1.0f);
//     if (skeleton.bones.size() > 0) {
//         // Find the root bone's global transform
//         for (size_t i = 0; i < skeleton.bones.size(); ++i) {
//             if (skeleton.bones[i].parentIndex == -1) {
//                 // This is a root bone, but we actually want its PARENT node transform
//                 // Since we don't have it, use identity for now
//                 break;
//             }
//         }
//     }
//     glm::mat4 inverseMeshTransform = glm::inverse(meshNodeTransform);

//     // Final matrices = global * inverse bind
//     for (size_t i = 0; i < skeleton.bones.size(); ++i) {
//         boneMatrices[i] = inverseMeshTransform * globalTransforms[i] * skeleton.bones[i].inverseBindMatrix;
//     }
// }


}
