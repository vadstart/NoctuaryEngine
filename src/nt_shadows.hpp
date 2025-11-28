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

class NtShadowCubeMap {
public:
    NtShadowCubeMap(NtDevice& device, uint32_t size);
    ~NtShadowCubeMap();

    VkImage getShadowCubeImage() const { return shadowCubeImage; }
    VkImageView getShadowCubeImageView() const { return shadowCubeImageView; }
    VkSampler getShadowCubeSampler() const { return shadowCubeSampler; }
    VkSampler getShadowCubeDebugSampler() const { return shadowCubeDebugSampler; }

    // Get individual face view for rendering
    VkImageView getFaceImageView(uint32_t face) const { return faceImageViews[face]; }

    uint32_t getSize() const { return size; }

private:
    NtDevice& ntDevice;
    uint32_t size; // Cubemap is square

    VkImage shadowCubeImage;
    VkDeviceMemory shadowCubeImageMemory;
    VkImageView shadowCubeImageView; // View for all faces (sampling)
    std::array<VkImageView, 6> faceImageViews; // Individual face views (rendering)
    VkSampler shadowCubeSampler;
    VkSampler shadowCubeDebugSampler;

    void createShadowCubeImage();
    void createShadowCubeImageViews();
    void createShadowCubeSamplers();
};

}
