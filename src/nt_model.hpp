#pragma once

#include "nt_device.hpp"
#include "vulkan/vulkan_core.h"
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace nt {

class NtModel {
    public:

        struct Vertex {
          glm::vec3 position;
          glm::vec3 color;

          static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
          static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };

        struct Data {
          std::vector<Vertex> vertices{};
          std::vector<uint32_t> indices{};
        };

        NtModel(NtDevice &device, const NtModel::Data &data);
        ~NtModel();

        NtModel(const NtModel &) = delete;
        NtModel& operator=(const NtModel &) = delete;

        void bind (VkCommandBuffer commandBuffer);
        void draw (VkCommandBuffer commandBuffer);
    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffer(const std::vector<uint32_t> &indices);

        NtDevice &ntDevice;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        uint32_t vertexCount;
        
        bool hasIndexBuffer = false;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        uint32_t indexCount;

};

}
