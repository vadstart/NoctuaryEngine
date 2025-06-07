#pragma once

#include "nt_device.hpp"
#include "nt_buffer.hpp"

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace nt {

class NtModel {
    public:

        struct Vertex {
          glm::vec3 position{};
          glm::vec3 color{};
          glm::vec3 normal{};
          glm::vec2 uv{};
          glm::vec3 tangent{};

          static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
          static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

          bool operator==(const Vertex &other) const {
            return position == other.position && color == other.color && normal == other.normal && uv == other.uv && tangent == other.tangent;
          }
        };

        struct Data {
          std::vector<Vertex> vertices{};
          std::vector<uint32_t> indices{};

          void loadModel(const std::string &filepath);
        };

        NtModel(NtDevice &device, const NtModel::Data &data);
        ~NtModel();

        NtModel(const NtModel &) = delete;
        NtModel& operator=(const NtModel &) = delete;

        static std::unique_ptr<NtModel> createModelFromFile(NtDevice &device, const std::string &filepath);
        uint32_t getVertexCount() { return vertexCount; }

        void bind (VkCommandBuffer commandBuffer);
        void draw (VkCommandBuffer commandBuffer);
    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffer(const std::vector<uint32_t> &indices);

        NtDevice &ntDevice;

        std::unique_ptr<NtBuffer> vertexBuffer;
        uint32_t vertexCount;
        
        bool hasIndexBuffer = false;
        std::unique_ptr<NtBuffer> indexBuffer;
        uint32_t indexCount;
};

}
