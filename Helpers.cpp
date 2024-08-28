#include "Helpers.hpp"

#include "RTG.hpp"
#include "VK.hpp"
#include "refsol.hpp"

#include <utility>
#include <cassert>
#include <cstring>
#include <iostream>

Helpers::Allocation::Allocation(Allocation &&from) {
	assert(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr);

	std::swap(handle, from.handle);
	std::swap(size, from.size);
	std::swap(offset, from.offset);
	std::swap(mapped, from.mapped);
}

Helpers::Allocation &Helpers::Allocation::operator=(Allocation &&from) {
	if (!(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr)) {
		//not fatal, just sloppy, so complain but don't throw:
		std::cerr << "Replacing a non-empty allocation; device memory will leak." << std::endl;
	}

	std::swap(handle, from.handle);
	std::swap(size, from.size);
	std::swap(offset, from.offset);
	std::swap(mapped, from.mapped);

	return *this;
}

Helpers::Allocation::~Allocation() {
	if (!(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr)) {
		std::cerr << "Destructing a non-empty Allocation; device memory will leak." << std::endl;
	}
}

//----------------------------

Helpers::AllocatedBuffer Helpers::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map) {
	AllocatedBuffer buffer;
	refsol::Helpers_create_buffer(rtg, size, usage, properties, (map == Mapped), &buffer);
	return buffer;
}

void Helpers::destroy_buffer(AllocatedBuffer &&buffer) {
	refsol::Helpers_destroy_buffer(rtg, &buffer);
}


Helpers::AllocatedImage Helpers::create_image(VkExtent2D const &extent, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map) {
	AllocatedImage image;
	refsol::Helpers_create_image(rtg, extent, format, tiling, usage, properties, (map == Mapped), &image);
	return image;
}

void Helpers::destroy_image(AllocatedImage &&image) {
	refsol::Helpers_destroy_image(rtg, &image);
}

//----------------------------

void Helpers::transfer_to_buffer(void *data, size_t size, AllocatedBuffer &target) {
	refsol::Helpers_transfer_to_buffer(rtg, data, size, &target);
}

void Helpers::transfer_to_image(void *data, size_t size, AllocatedImage &target) {
	refsol::Helpers_transfer_to_image(rtg, data, size, &target);
}

//----------------------------

VkFormat Helpers::find_image_format(std::vector< VkFormat > const &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
	return refsol::Helpers_find_image_format(rtg, candidates, tiling, features);
}

VkShaderModule Helpers::create_shader_module(uint32_t const *code, size_t bytes) const {
	VkShaderModule shader_module = VK_NULL_HANDLE;
	refsol::Helpers_create_shader_module(rtg, code, bytes, &shader_module);
	return shader_module;
}

//----------------------------

Helpers::Helpers(RTG const &rtg_) : rtg(rtg_) {
}

Helpers::~Helpers() {
}

void Helpers::create() {
}

void Helpers::destroy() {
}
