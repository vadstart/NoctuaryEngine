#pragma once

#include "nt_device.hpp"
#include <array>

namespace nt
{

class NtShadowMap {
public:
    NtShadowMap(NtDevice& device, uint32_t width, uint32_t height);
    ~NtShadowMap();

    VkImage getShadowImage() const { return shadowImage; }
    VkImageView getShadowImageView() const { return shadowImageView; }
    VkSampler getShadowSampler() const { return shadowSampler; }
    VkSampler getShadowDebugSampler() const { return shadowDebugSampler; }

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

private:
    NtDevice& ntDevice;
    uint32_t width;
    uint32_t height;

    VkImage shadowImage;
    VkDeviceMemory shadowImageMemory;
    VkImageView shadowImageView;
    VkSampler shadowSampler;
    VkSampler shadowDebugSampler;

    void createShadowImage();
    void createShadowImageView();
    void createShadowSampler();
    void createShadowDebugSampler();
};

}
