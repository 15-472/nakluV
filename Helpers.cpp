#include "Helpers.hpp"

#include "RTG.hpp"
#include "VK.hpp"

#include <vulkan/utility/vk_format_utils.h> //useful for byte counting

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

Helpers::Allocation Helpers::allocate(VkDeviceSize size, VkDeviceSize alignment, 
									uint32_t memory_type_index, MapFlag map) {
	Helpers::Allocation allocation;

	VkMemoryAllocateInfo alloc_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = size,
		.memoryTypeIndex = memory_type_index
	};

	VK( vkAllocateMemory( rtg.device, &alloc_info, nullptr, &allocation.handle ) );

	allocation.size = size;
	allocation.offset = 0;

	if (map == Mapped) {
		VK( vkMapMemory(rtg.device, allocation.handle, 0, allocation.size, 0, &allocation.mapped));
	}

	return allocation;
}

Helpers::Allocation Helpers::allocate(VkMemoryRequirements const &req, 
								VkMemoryPropertyFlags properties, MapFlag map) {
	return allocate(req.size, req.alignment, 
			find_memory_type(req.memoryTypeBits, properties), map);
}

void Helpers::free(Helpers::Allocation &&allocation) {
	if (allocation.mapped != nullptr) {
		vkUnmapMemory(rtg.device, allocation.handle);
		allocation.mapped = nullptr;
	}

	vkFreeMemory(rtg.device, allocation.handle, nullptr);

	allocation.handle = VK_NULL_HANDLE;
	allocation.offset = 0;
	allocation.size = 0;
}

//----------------------------

Helpers::AllocatedBuffer Helpers::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map) {
	AllocatedBuffer buffer;
	VkBufferCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	VK( vkCreateBuffer(rtg.device, &create_info, nullptr, &buffer.handle) );
	buffer.size = size;

	//determine memory requirements
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(rtg.device, buffer.handle, &req);

	//allocate memory
	buffer.allocation = allocate(req, properties, map);

	//bind memory
	VK( vkBindBufferMemory(rtg.device, buffer.handle, buffer.allocation.handle, buffer.allocation.offset));

	return buffer;
}

void Helpers::destroy_buffer(AllocatedBuffer &&buffer) {
	vkDestroyBuffer(rtg.device, buffer.handle, nullptr);
	buffer.handle = VK_NULL_HANDLE;
	buffer.size = 0;

	this->free(std::move(buffer.allocation));
}


Helpers::AllocatedImage Helpers::create_image(VkExtent2D const &extent, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map) {
	AllocatedImage image;
	image.extent = extent;
	image.format = format;

	VkImageCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent{
			.width = extent.width,
			.height = extent.height,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VK( vkCreateImage(rtg.device, &create_info, nullptr, &image.handle) );

	VkMemoryRequirements req;
	vkGetImageMemoryRequirements(rtg.device, image.handle, &req);

	image.allocation = allocate(req, properties, map);

	VK( vkBindImageMemory(rtg.device, image.handle, image.allocation.handle, image.allocation.offset) );
	return image;
}

void Helpers::destroy_image(AllocatedImage &&image) {
	vkDestroyImage(rtg.device, image.handle, nullptr);

	image.handle = VK_NULL_HANDLE;
	image.extent = VkExtent2D{.width = 0, .height = 0};
	image.format = VK_FORMAT_UNDEFINED;

	this->free(std::move(image.allocation));
}

//----------------------------

void Helpers::transfer_to_buffer(void *data, size_t size, AllocatedBuffer &target) {
	//NOTE: could let this stick around and use it for all uploads, but this function isn;t for performant transfers anyway:
	AllocatedBuffer transfer_src = create_buffer(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		Mapped
	);

	//copy data into transfer buffer:
	std::memcpy(transfer_src.allocation.data(), data, size);

	{ //record command buffer that does CPU->GPU transfer:
		VK( vkResetCommandBuffer(transfer_command_buffer, 0) );

		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};

		VK( vkBeginCommandBuffer(transfer_command_buffer, &begin_info) );

		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size
		};
		vkCmdCopyBuffer(transfer_command_buffer, transfer_src.handle, 
				target.handle, 1, &copy_region);

		VK( vkEndCommandBuffer(transfer_command_buffer) );
	}

	{//run command buffer
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &transfer_command_buffer
		};

		VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, VK_NULL_HANDLE) );
	}

	//wait for command buffer to finish
	VK( vkQueueWaitIdle(rtg.graphics_queue) );

	//don't leak buffer memory:
	destroy_buffer(std::move(transfer_src));
}

void Helpers::transfer_to_image(void *data, size_t size, AllocatedImage &target) {
	assert(target.handle != VK_NULL_HANDLE); //target image should be allocated already

	//check data is the right size:
	size_t bytes_per_pixel = vkuFormatElementSize(target.format);
	assert(size == target.extent.width * target.extent.height * bytes_per_pixel);

	//create a host-coherent source buffer
	AllocatedBuffer transfer_src = create_buffer(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		Mapped
	);

	//copy image data into the source buffer
	std::memcpy(transfer_src.allocation.data(), data, size);

	//begin recording a command buffer
	VK( vkResetCommandBuffer(transfer_command_buffer, 0) );

	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
	};

	VK( vkBeginCommandBuffer(transfer_command_buffer, &begin_info) );

	VkImageSubresourceRange whole_image{
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1,
	};

	{ //put the receiving image in destination-optimal layout
		VkImageMemoryBarrier barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, //throw away old image
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.handle,
			.subresourceRange = whole_image,
		};

		vkCmdPipelineBarrier(
			transfer_command_buffer, //commandBuffer
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, //srcStageMask
			VK_PIPELINE_STAGE_TRANSFER_BIT, //dstStageMask
			0, //dependencyFlags
			0, nullptr, //memory barrier count, pointer
			0, nullptr, //buffer memory barrier count, pointer
			1, &barrier //image memory barrier count, pointer
		);
	}

	{ // copy the source buffer to the image
		VkBufferImageCopy region{
			.bufferOffset = 0,
			.bufferRowLength = target.extent.width,
			.bufferImageHeight = target.extent.height,
			.imageSubresource{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.imageOffset{ .x = 0, .y = 0, .z = 0 },
			.imageExtent{
				.width = target.extent.width,
				.height = target.extent.height,
				.depth = 1
			},
		};

		vkCmdCopyBufferToImage(
			transfer_command_buffer,
			transfer_src.handle,
			target.handle,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region
		);

		//NOTE: if image had mip levels, would need to copy as additional regions here.
	}

	{ // transition the image memory to shader-read-only-optimal layout:
		VkImageMemoryBarrier barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.handle,
			.subresourceRange = whole_image,
		};

		vkCmdPipelineBarrier(
			transfer_command_buffer, //commandBuffer
			VK_PIPELINE_STAGE_TRANSFER_BIT, //srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, //dstStageMask
			0, //dependencyFlags
			0, nullptr, //memory barrier count, pointer
			0, nullptr, //buffer memory barrier count, pointer
			1, &barrier //image memory barrier count, pointer
		);
	}

	//end and submit the command buffer
	VK( vkEndCommandBuffer(transfer_command_buffer) );

	VkSubmitInfo submit_info{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &transfer_command_buffer
	};

	VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, VK_NULL_HANDLE) );

	//wait for command buffer to finish executing
	VK( vkQueueWaitIdle(rtg.graphics_queue) );

	//destroy the source buffer
	destroy_buffer(std::move(transfer_src));
}

//----------------------------

uint32_t Helpers::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags) const {
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		VkMemoryType const &type = memory_properties.memoryTypes[i];
		if ((type_filter & (1 << i)) != 0
		 && (type.propertyFlags & flags) == flags) {
			return i;
		}
	}
	throw std::runtime_error("No suitable memory type found.");
}

VkFormat Helpers::find_image_format(std::vector< VkFormat > const &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(rtg.physical_device, format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
	throw std::runtime_error("No supported format matches request.");
}

VkShaderModule Helpers::create_shader_module(uint32_t const *code, size_t bytes) const {
	VkShaderModule shader_module = VK_NULL_HANDLE;
	VkShaderModuleCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = bytes,
		.pCode = code
	};
	VK( vkCreateShaderModule(rtg.device, &create_info, nullptr, &shader_module) );
	return shader_module;
}

//----------------------------

Helpers::Helpers(RTG const &rtg_) : rtg(rtg_) {
}

Helpers::~Helpers() {
}

void Helpers::create() {
	VkCommandPoolCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = rtg.graphics_queue_family.value(),
	};
	VK( vkCreateCommandPool(rtg.device, &create_info, nullptr, &transfer_command_pool) );

	VkCommandBufferAllocateInfo alloc_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = transfer_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &transfer_command_buffer) );

	vkGetPhysicalDeviceMemoryProperties(rtg.physical_device, &memory_properties);
	if (rtg.configuration.debug) {
		std::cout << "Memory types:\n";
		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
			VkMemoryType const &type = memory_properties.memoryTypes[i];
			std::cout << " [" << i << "] heap " << type.heapIndex << ", flags: " << string_VkMemoryPropertyFlags(type.propertyFlags) << '\n';
		}
		std::cout << "Memory heaps:\n";
		for (uint32_t i = 0; i < memory_properties.memoryHeapCount; ++i) {
			VkMemoryHeap const &heap = memory_properties.memoryHeaps[i];
			std::cout << " [" << i << "] " << heap.size << " bytes, flags: " << string_VkMemoryHeapFlags( heap.flags ) << '\n';
		}
		std::cout.flush();
	}
}

void Helpers::destroy() {
	//technically not needed since freeing the pool will free all contained buffers:
	if(transfer_command_buffer != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(rtg.device, transfer_command_pool, 1, &transfer_command_buffer);
		transfer_command_buffer = VK_NULL_HANDLE;
	}

	if(transfer_command_pool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(rtg.device, transfer_command_pool, nullptr);
		transfer_command_pool = VK_NULL_HANDLE;
	}
}
