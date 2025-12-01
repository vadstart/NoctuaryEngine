#pragma once

#include "nt_animation.hpp"
#include "nt_model.hpp"

#include <glm/glm.hpp>
#include <glm/fwd.hpp>
#include <string>
#include <vector>

namespace nt {

class NtAnimator {
public:
    NtAnimator() = default;

    void play(const std::string &animationName, bool loop = false);
    void update(NtModel &model, float deltaTime);

    bool getIsPlaying() const { return isPlaying; }
    std::string getCurrentAnimationName() const { return currentAnimationName; }
    float getCurrentTime() const { return currentTime; }
    float getDuration() const { return cachedDuration; }

    void stop() { isPlaying = false; }
    void pause() { isPlaying = false; }
    void resume() { isPlaying = true; }

private:
    std::string currentAnimationName;
    float currentTime = 0.0f;
    float cachedDuration = -1.0f;;
    bool isLooping = true;
    bool isPlaying = false;

    const NtAnimation* findAnimation(NtModel& model, const std::string& name) {
        for (const auto& anim : model.getAnimations()) {
            if (anim.name == name) return &anim;
        }
        return nullptr;
    }

    glm::vec4 interpolateSampler(const NtAnimationSampler& sampler, float time);
};

}
