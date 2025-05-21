#include "astral_app.h"
#include "generic_render_system.h"
#include "nt_camera.h"

#include <algorithm>
#include <chrono>
// #include "vulkan/vulkan_core.h"

// Libraries
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// Std
#include <iostream>
#include <cassert>
#include <memory>

namespace nt
{

AstralApp::AstralApp() {
  loadGameObjects();
}
AstralApp::~AstralApp() {}

void AstralApp::run() {
  GenericRenderSystem genericRenderSystem(ntDevice, ntRenderer.getSwapChainRenderPass());
  NtCamera camera{};

  std::cout << "maxPushConstantSize = " << ntDevice.properties.limits.maxPushConstantsSize << "\n";

  // Temporary implementation of DeltaTime
  auto currentTime = std::chrono::high_resolution_clock::now();

  while (!ntWindow.shouldClose()) {
    auto newTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;

    glfwPollEvents();

    float aspect = ntRenderer.getAspectRatio();
    // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
    camera.setPerspectiveProjection(glm::radians(45.f), aspect, 0.1f, 10.f);

    if (auto commandBuffer = ntRenderer.beginFrame()) {
      // TODO: Add Reflections, Shadows, Postprocessing, etc
      
      ntRenderer.beginSwapChainRenderPass(commandBuffer);
      genericRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera, deltaTime);
      ntRenderer.endSwapChainRenderPass(commandBuffer);
      ntRenderer.endFrame();
    }
  }

  vkDeviceWaitIdle(ntDevice.device());
}

// Temp cube creation helper
std::unique_ptr<NtModel> createCubeModel(NtDevice& device, glm::vec3 offset) {
  using Vertex = NtModel::Vertex;

  // Define the 8 corners of the cube and assign each a unique color (rainbow-ish)
  std::vector<Vertex> corners = {
    Vertex{{-.5f, -.5f, -.5f}, {1.0f, 0.0f, 0.0f}}, // 0: red
    Vertex{{.5f, -.5f, -.5f}, {1.0f, 0.5f, 0.0f}},  // 1: orange
    Vertex{{.5f, .5f, -.5f}, {1.0f, 1.0f, 0.0f}},   // 2: yellow
    Vertex{{-.5f, .5f, -.5f}, {0.0f, 1.0f, 0.0f}},  // 3: green
    Vertex{{-.5f, -.5f, .5f}, {0.0f, 0.0f, 1.0f}},  // 4: blue
    Vertex{{.5f, -.5f, .5f}, {0.29f, 0.0f, 0.51f}}, // 5: indigo
    Vertex{{.5f, .5f, .5f}, {0.56f, 0.0f, 1.0f}},   // 6: violet
    Vertex{{-.5f, .5f, .5f}, {1.0f, 0.0f, 1.0f}},   // 7: magenta
  };

  // Indices into the `corners` array to form 12 triangles (2 per face)
  std::vector<uint32_t> indices = {
    // front face (z = +0.5)
    4, 5, 6,
    4, 6, 7,
    // back face (z = -0.5)
    0, 2, 1,
    0, 3, 2,
    // left face (x = -0.5)
    0, 7, 3,
    0, 4, 7,
    // right face (x = +0.5)
    1, 2, 6,
    1, 6, 5,
    // top face (y = +0.5)
    3, 7, 6,
    3, 6, 2,
    // bottom face (y = -0.5)
    0, 1, 5,
    0, 5, 4,
  };

  std::vector<Vertex> vertices;
  for (auto index : indices) {
    Vertex v = corners[index];
    v.position += offset;
    vertices.push_back(v);
  }

  return std::make_unique<NtModel>(device, vertices);
}

void AstralApp::loadGameObjects() {
  std::shared_ptr<NtModel> ntModel = createCubeModel(ntDevice, {.0f, .0f, .0f});

  auto cube = NtGameObject::createGameObject();
  cube.model = ntModel;
  cube.transform.translation = {.0f, .0f, 1.25f};
  cube.transform.scale = {.5f, .5f, .5f};

  gameObjects.push_back(std::move(cube));
}

}
