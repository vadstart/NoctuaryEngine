
#include "nt_descriptors.hpp"

// std
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace nt {

// *************** Descriptor Set Layout Builder *********************

NtDescriptorSetLayout::Builder &NtDescriptorSetLayout::Builder::addBinding(
    uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    uint32_t count) {
  assert(bindings.count(binding) == 0 && "Binding already in use");
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = descriptorType;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stageFlags;
  bindings[binding] = layoutBinding;
  return *this;
}

std::unique_ptr<NtDescriptorSetLayout> NtDescriptorSetLayout::Builder::build() const {
  return std::make_unique<NtDescriptorSetLayout>(ntDevice, bindings);
}

// *************** Descriptor Set Layout *********************

NtDescriptorSetLayout::NtDescriptorSetLayout(
    NtDevice &ntDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
    : ntDevice{ntDevice}, bindings{bindings} {
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
  for (auto kv : bindings) {
    setLayoutBindings.push_back(kv.second);
  }

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
  descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
  descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

  if (vkCreateDescriptorSetLayout(
          ntDevice.device(),
          &descriptorSetLayoutInfo,
          nullptr,
          &descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

NtDescriptorSetLayout::~NtDescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(ntDevice.device(), descriptorSetLayout, nullptr);
}

// *************** Descriptor Pool Builder *********************

NtDescriptorPool::Builder &NtDescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) {
  poolSizes.push_back({descriptorType, count});
  return *this;
}

NtDescriptorPool::Builder &NtDescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) {
  poolFlags = flags;
  return *this;
}
NtDescriptorPool::Builder &NtDescriptorPool::Builder::setMaxSets(uint32_t count) {
  maxSets = count;
  return *this;
}

std::unique_ptr<NtDescriptorPool> NtDescriptorPool::Builder::build() const {
  return std::make_unique<NtDescriptorPool>(ntDevice, maxSets, poolFlags, poolSizes);
}

// *************** Descriptor Pool *********************

NtDescriptorPool::NtDescriptorPool(
    NtDevice &ntDevice,
    uint32_t maxSets,
    VkDescriptorPoolCreateFlags poolFlags,
    const std::vector<VkDescriptorPoolSize> &poolSizes)
    : ntDevice{ntDevice} {
  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  descriptorPoolInfo.pPoolSizes = poolSizes.data();
  descriptorPoolInfo.maxSets = maxSets;
  descriptorPoolInfo.flags = poolFlags;

  if (vkCreateDescriptorPool(ntDevice.device(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

NtDescriptorPool::~NtDescriptorPool() {
  vkDestroyDescriptorPool(ntDevice.device(), descriptorPool, nullptr);
}

bool NtDescriptorPool::allocateDescriptorSet(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.pSetLayouts = &descriptorSetLayout;
  allocInfo.descriptorSetCount = 1;

  if (vkAllocateDescriptorSets(ntDevice.device(), &allocInfo, &descriptor) != VK_SUCCESS) {
    return false;
  }
  return true;
}

void NtDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
  vkFreeDescriptorSets(
      ntDevice.device(),
      descriptorPool,
      static_cast<uint32_t>(descriptors.size()),
      descriptors.data());
}

void NtDescriptorPool::resetPool() {
  vkResetDescriptorPool(ntDevice.device(), descriptorPool, 0);
}

// *************** Descriptor Writer *********************

NtDescriptorWriter::NtDescriptorWriter(NtDescriptorSetLayout &setLayout, NtDescriptorPool &pool)
    : setLayout{setLayout}, pool{pool} {}

NtDescriptorWriter &NtDescriptorWriter::writeBuffer(
    uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pBufferInfo = bufferInfo;
  write.descriptorCount = 1;

  writes.push_back(write);
  return *this;
}

NtDescriptorWriter &NtDescriptorWriter::writeImage(
    uint32_t binding, VkDescriptorImageInfo *imageInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pImageInfo = imageInfo;
  write.descriptorCount = 1;



  writes.push_back(write);
  return *this;
}

bool NtDescriptorWriter::build(VkDescriptorSet &set) {
  bool success = pool.allocateDescriptorSet(setLayout.getDescriptorSetLayout(), set);
  if (!success) {
    return false;
  }
  overwrite(set);
  return true;
}

void NtDescriptorWriter::overwrite(VkDescriptorSet &set) {
  for (auto &write : writes) {
    write.dstSet = set;
  }
  vkUpdateDescriptorSets(pool.ntDevice.device(), writes.size(), writes.data(), 0, nullptr);
}

} 
