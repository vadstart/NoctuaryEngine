#pragma once

#include "nt_device.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace nt {

class NtDescriptorSetLayout {
 public:
  class Builder {
   public:
    Builder(NtDevice &ntDevice) : ntDevice{ntDevice} {}

    Builder &addBinding(
        uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        uint32_t count = 1);
    std::unique_ptr<NtDescriptorSetLayout> build() const;

   private:
    NtDevice &ntDevice;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
  };

  NtDescriptorSetLayout(
      NtDevice &ntDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
  ~NtDescriptorSetLayout();
  NtDescriptorSetLayout(const NtDescriptorSetLayout &) = delete;
  NtDescriptorSetLayout &operator=(const NtDescriptorSetLayout &) = delete;

  VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

 private:
  NtDevice &ntDevice;
  VkDescriptorSetLayout descriptorSetLayout;
  std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

  friend class NtDescriptorWriter;
};

class NtDescriptorPool {
 public:
  class Builder {
   public:
    Builder(NtDevice &ntDevice) : ntDevice{ntDevice} {}

    Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
    Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
    Builder &setMaxSets(uint32_t count);
    std::unique_ptr<NtDescriptorPool> build() const;

   private:
    NtDevice &ntDevice;
    std::vector<VkDescriptorPoolSize> poolSizes{};
    uint32_t maxSets = 1000;
    VkDescriptorPoolCreateFlags poolFlags = 0;
  };

  NtDescriptorPool(
      NtDevice &ntDevice,
      uint32_t maxSets,
      VkDescriptorPoolCreateFlags poolFlags,
      const std::vector<VkDescriptorPoolSize> &poolSizes);
  ~NtDescriptorPool();
  NtDescriptorPool(const NtDescriptorPool &) = delete;
  NtDescriptorPool &operator=(const NtDescriptorPool &) = delete;

  bool allocateDescriptorSet(
      const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;

  void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;

  void resetPool();

  VkDescriptorPool getDescriptorPool() const { return descriptorPool; }

 private:
  NtDevice &ntDevice;
  VkDescriptorPool descriptorPool;

  friend class NtDescriptorWriter;
};

class NtDescriptorWriter {
 public:
  NtDescriptorWriter(NtDescriptorSetLayout &setLayout, NtDescriptorPool &pool);

  NtDescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
  NtDescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

  bool build(VkDescriptorSet &set);
  void overwrite(VkDescriptorSet &set);

 private:
  NtDescriptorSetLayout &setLayout;
  NtDescriptorPool &pool;
  std::vector<VkWriteDescriptorSet> writes;
};

}
