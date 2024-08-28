#pragma once

/*
 * Reference solution code is boilerplate code that we've precompiled so
 * you don't have to write it before seeing things on your screen.
 *
 * By the end of the tutorial you won't be using any of these functions.
 *
 * Do not edit any of these definitions!
 * (That might break linking or calling of the precompiled code.)
 *
 */

#include "InputEvent.hpp"

#include <vulkan/vulkan_core.h>

#include <optional>
#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace refsol {

//----- Used by `struct Tutorial` ------

//called by Tutorial::Tutorial:
template< typename RTG_T >
void Tutorial_constructor(
	RTG_T const &rtg,
	//looks up:
	VkFormat *depth_format,
	//creates:
	VkRenderPass *render_pass,
	VkCommandPool *command_pool
);
//called by Tutorial::~Tutorial:
template< typename RTG_T >
void Tutorial_destructor(
	RTG_T const &rtg,
	//destroys:
	VkRenderPass *render_pass,
	VkCommandPool *command_pool
) noexcept;

//called by Tutorial::Tutorial (per Workspace):
template< typename RTG_T >
void Tutorial_constructor_workspace(
	RTG_T const &rtg,
	VkCommandPool command_pool,
	//allocates:
	VkCommandBuffer *workspace_command_buffer
);

//called by Tutorial::~Tutorial (per Workspace):
template< typename RTG_T >
void Tutorial_destructor_workspace(
	RTG_T const &rtg,
	VkCommandPool command_pool,
	//frees:
	VkCommandBuffer *workspace_command_buffer
) noexcept;

//called by Tutorial::on_swapchain:
template< typename RTG_T, typename SwapchainEvent_T, typename AllocatedImage_T >
void Tutorial_on_swapchain(
	RTG_T const &rtg,
	SwapchainEvent_T const &swapchain,
	VkFormat depth_format,
	VkRenderPass render_pass,
	//[re]creates:
	AllocatedImage_T *swapchain_depth_image,
	VkImageView *swapchain_depth_image_view,
	std::vector< VkFramebuffer > *swapchain_framebuffers
);

//called by Tutorial::destroy_framebuffers:
template< typename RTG_T, typename AllocatedImage_T >
void Tutorial_destroy_framebuffers(
	RTG_T const &rtg,
	//destroys:
	AllocatedImage_T *swapchain_depth_image,
	VkImageView *swapchain_depth_image_view,
	std::vector< VkFramebuffer > *swapchain_framebuffers
) noexcept;

//called by Tutorial::render:
template< typename RTG_T >
void Tutorial_render_record_blank_frame(
	RTG_T const &rtg,
	//draws with/to:
	VkRenderPass render_pass,
	VkFramebuffer framebuffer,
	//resets + fills:
	VkCommandBuffer *workspace_command_buffer
);

template< typename RTG_T, typename RenderParams_T >
void Tutorial_render_submit(
	RTG_T const &rtg,
	RenderParams_T const &render_params,
	VkCommandBuffer workspace_command_buffer
);

//----- Used by `struct Tutorial::BackgroundPipeline` ------

template< typename RTG_T >
void BackgroundPipeline_create(
	RTG_T const &rtg,
	VkRenderPass render_pass,
	uint32_t subpass,
	VkShaderModule vert_module,
	VkShaderModule frag_module,
	VkPipelineLayout *layout,
	VkPipeline *handle
);

template< typename RTG_T >
void BackgroundPipeline_destroy(
	RTG_T const &rtg,
	VkPipelineLayout *layout,
	VkPipeline *handle
);

//----- Used by `struct Helpers` ------

//called by Helpers::create_shader_module:
template< typename RTG_T >
void Helpers_create_shader_module(
	RTG_T const &rtg,
	uint32_t const *code,
	size_t bytes,
	VkShaderModule *shader_module
);

//called by Helpers::find_memory_type: (also used internally by some refsol functions)
template< typename RTG_T >
uint32_t Helpers_find_memory_type(
	RTG_T const &rtg,
	uint32_t type_filter,
	VkMemoryPropertyFlags flags
);

//called by Helpers::find_image_format: (also used internally by some refsol functions)
template< typename RTG_T >
VkFormat Helpers_find_image_format(
	RTG_T const &rtg,
	std::vector< VkFormat > const &candidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features
);

//called by Helpers::free: (also used internally by Helpers_destroy_*):
template< typename RTG_T, typename Allocation_T >
void Helpers_free(
	RTG_T const &rtg,
	Allocation_T *allocation
) noexcept;

//called by Helpers::create_buffer: (also used internally by some refsol functions)
template< typename RTG_T, typename AllocatedBuffer_T >
void Helpers_create_buffer(
	RTG_T const &rtg,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	bool should_map,
	//creates:
	AllocatedBuffer_T *buffer
);

//called by Helpers::destroy_buffer: (also used internally by some refsol functions)
template< typename RTG_T, typename AllocatedBuffer_T >
void Helpers_destroy_buffer(
	RTG_T const &rtg,
	//destroys:
	AllocatedBuffer_T *buffer
);

//called by Helpers::create_image: (also used internally by some refsol functions)
template< typename RTG_T, typename AllocatedImage_T >
void Helpers_create_image(
	RTG_T const &rtg,
	VkExtent2D const &extent,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	bool should_map,
	//creates:
	AllocatedImage_T *image
);

//called by Helpers::destroy_image: (also used internally by some refsol functions)
template< typename RTG_T, typename AllocatedImage_T >
void Helpers_destroy_image(
	RTG_T const &rtg,
	//destroys:
	AllocatedImage_T &image
) noexcept;

//called by Helpers::transfer_to_buffer:
template< typename RTG_T, typename AllocatedBuffer_T >
void Helpers_transfer_to_buffer(
	RTG_T const &rtg,
	//copies this data:
	void *data,
	size_t size,
	//into this (already-allocated and large-enough) buffer:
	AllocatedBuffer_T *target
);

//called by Helpers::transfer_to_image:
template< typename RTG_T, typename AllocatedImage_T >
void Helpers_transfer_to_image(
	RTG_T const &rtg,
	//copies this data:
	void *data,
	size_t size, //must be exactly the amount of bytes needed for image
	//into this (already-allocated) image:
	AllocatedImage_T *target
);
//NOTE: after transfer, image will be transitioned to 'VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL' layout.

//----- Used by `struct RTG` ------

//Used by RTG::RTG:
void RTG_constructor_create_instance(
	VkApplicationInfo const &configuration_application_info,
	bool configuration_debug,
	//output:
	VkInstance *instance,
	VkDebugUtilsMessengerEXT *debug_messenger
);

void RTG_constructor_create_surface(
	VkApplicationInfo const &configuration_application_info,
	bool configuration_debug,
	VkExtent2D const &configuration_surface_extent,
	VkInstance instance,
	//output:
	GLFWwindow **window,
	VkSurfaceKHR *surface
);

void RTG_constructor_select_physical_device(
	bool configuration_debug,
	std::string const &configuration_physical_device_name,
	VkInstance instance,
	//output:
	VkPhysicalDevice *physical_device
);

void RTG_constructor_select_format_and_mode(
	bool configuration_debug,
	std::vector< VkSurfaceFormatKHR > const &configuration_surface_formats,
	std::vector< VkPresentModeKHR > const &configuration_present_modes,
	VkPhysicalDevice physical_device,
	VkSurfaceKHR surface,
	//output:
	VkSurfaceFormatKHR *surface_format,
	VkPresentModeKHR *present_mode
);

void RTG_constructor_create_device(
	bool configuration_debug,
	//uses:
	VkPhysicalDevice physical_device,
	VkSurfaceKHR surface,
	//creates:
	VkDevice *device_,
	std::optional< uint32_t > *graphics_queue_family_,
	VkQueue *graphics_queue_,
	std::optional< uint32_t > *present_queue_family_,
	VkQueue *present_queue_
);

template< typename PerWorkspace_T >
void RTG_constructor_per_workspace(
	VkDevice device,
	PerWorkspace_T *workspace
);

//Used by RTG::~RTG:
template< typename PerWorkspace_T >
void RTG_destructor_per_workspace(
	VkDevice device,
	PerWorkspace_T *workspace
) noexcept;

void RTG_destructor(
	VkDevice *device,
	VkSurfaceKHR *surface,
	GLFWwindow **window,
	VkDebugUtilsMessengerEXT *debug_messenger,
	VkInstance *instance
) noexcept;

//used by RTG::recreate_swapchain:
void RTG_recreate_swapchain(
	bool configuration_debug,
	VkDevice device,
	VkPhysicalDevice physical_device,
	VkSurfaceKHR surface,
	VkSurfaceFormatKHR surface_format,
	VkPresentModeKHR present_mode,
	std::optional< uint32_t > const &graphics_queue_family,
	std::optional< uint32_t > const &present_queue_family,
	//[re]creates:
	VkSwapchainKHR *swapchain_,
	VkExtent2D *swapchain_extent_,
	std::vector< VkImage > *swapchain_images_,
	std::vector< VkImageView > *swapchain_image_views_
);

//used by RTG::destroy_swapchain:
void RTG_destroy_swapchain(
	VkDevice device,
	//destroys:
	VkSwapchainKHR *swapchain_,
	std::vector< VkImage > *swapchain_images_,
	std::vector< VkImageView > *swapchain_image_views_
);

//used by RTG::run:
template< typename RTG_T, typename Application_T >
void RTG_run(RTG_T &rtg, Application_T &application);

//----- internals ------

//helpers used to pass around pointers to Allocated* and Allocation:
// (otherwise function signatures get unwieldy)
struct AllocationPtr {
	template< typename Allocation_T >
	explicit AllocationPtr(Allocation_T *allocation) :
		handle(&allocation->handle),
		offset(&allocation->offset),
		size(&allocation->size),
		mapped(&allocation->mapped) { }
	VkDeviceMemory *handle;
	VkDeviceSize *offset;
	VkDeviceSize *size;
	void **mapped;
};

struct AllocatedBufferPtr {
	template< typename AllocatedBuffer_T >
	explicit AllocatedBufferPtr(AllocatedBuffer_T *buffer) :
		handle(&buffer->handle),
		size(&buffer->size),
		allocation(&buffer->allocation) { }
	VkBuffer *handle;
	VkDeviceSize *size;
	AllocationPtr allocation;
};


struct AllocatedImagePtr {
	template< typename AllocatedImage_T >
	explicit AllocatedImagePtr(AllocatedImage_T *image) :
		handle(&image->handle),
		extent(&image->extent),
		format(&image->format),
		allocation(&image->allocation) { }
	VkImage *handle;
	VkExtent2D *extent;
	VkFormat *format;
	AllocationPtr allocation;
};


//this "two-stage" template + _impl structure is used so that
//the compiled refsol object (with the _impl functions) does
//not depend on the exact data layout or size of the RTG,
//Helpers, etc. classes.


void Tutorial_constructor_impl(
	VkDevice rtg_device,
	VkPhysicalDevice rtg_physical_device,
	std::optional< uint32_t > const &rtg_graphics_queue_family,
	VkSurfaceFormatKHR rtg_surface_format,
	VkFormat *depth_format,
	VkRenderPass *render_pass,
	VkCommandPool *command_pool
);
template< typename RTG_T >
void Tutorial_constructor(
	RTG_T const &rtg,
	VkFormat *depth_format,
	VkRenderPass *render_pass,
	VkCommandPool *command_pool
) {
	Tutorial_constructor_impl(
		rtg.device,
		rtg.physical_device,
		rtg.graphics_queue_family,
		rtg.surface_format,
		depth_format,
		render_pass,
		command_pool
	);
}

void Tutorial_destructor_impl(
	VkDevice rtg_device,
	VkRenderPass *render_pass,
	VkCommandPool *command_pool
) noexcept;
template< typename RTG_T >
void Tutorial_destructor(
	RTG_T const &rtg,
	VkRenderPass *render_pass,
	VkCommandPool *command_pool
) noexcept {
	Tutorial_destructor_impl(
		rtg.device,
		render_pass,
		command_pool
	);
}

void Tutorial_constructor_workspace_impl(
	VkDevice rtg_device,
	VkCommandPool command_pool,
	VkCommandBuffer *workspace_command_buffer
);
template< typename RTG_T >
void Tutorial_constructor_workspace(
	RTG_T const &rtg,
	VkCommandPool command_pool,
	VkCommandBuffer *workspace_command_buffer
) {
	Tutorial_constructor_workspace_impl(
		rtg.device,
		command_pool,
		workspace_command_buffer
	);
}

void Tutorial_destructor_workspace_impl(
	VkDevice rtg_device,
	VkCommandPool command_pool,
	VkCommandBuffer *workspace_command_buffer
) noexcept;
template< typename RTG_T >
void Tutorial_destructor_workspace(
	RTG_T const &rtg,
	VkCommandPool command_pool,
	VkCommandBuffer *workspace_command_buffer
) noexcept {
	Tutorial_destructor_workspace_impl(
		rtg.device,
		command_pool,
		workspace_command_buffer
	);
}

void Tutorial_on_swapchain_impl(
	VkDevice rtg_device,
	VkPhysicalDevice rtg_physical_device,
	VkExtent2D const &swapchain_extent,
	std::vector< VkImage > const &swapchain_images,
	std::vector< VkImageView > const &swapchain_image_views,
	VkFormat depth_format,
	VkRenderPass render_pass,
	AllocatedImagePtr swapchain_depth_image,
	VkImageView *swapchain_depth_image_view,
	std::vector< VkFramebuffer > *swapchain_framebuffers
);
template< typename RTG_T, typename SwapchainEvent_T, typename AllocatedImage_T >
void Tutorial_on_swapchain(
	RTG_T const &rtg,
	SwapchainEvent_T const &swapchain,
	VkFormat depth_format,
	VkRenderPass render_pass,
	AllocatedImage_T *swapchain_depth_image,
	VkImageView *swapchain_depth_image_view,
	std::vector< VkFramebuffer > *swapchain_framebuffers
) {
	Tutorial_on_swapchain_impl(
		rtg.device,
		rtg.physical_device,
		swapchain.extent,
		swapchain.images,
		swapchain.image_views,
		depth_format,
		render_pass,
		AllocatedImagePtr(swapchain_depth_image),
		swapchain_depth_image_view,
		swapchain_framebuffers
	);
}


void Tutorial_destroy_framebuffers_impl(
	VkDevice rtg_device,
	AllocatedImagePtr const &swapchain_depth_image,
	VkImageView *swapchain_depth_image_view_,
	std::vector< VkFramebuffer > *swapchain_framebuffers_
) noexcept;
template< typename RTG_T, typename AllocatedImage_T >
void Tutorial_destroy_framebuffers(
	RTG_T const &rtg,
	//destroys:
	AllocatedImage_T *swapchain_depth_image,
	VkImageView *swapchain_depth_image_view,
	std::vector< VkFramebuffer > *swapchain_framebuffers
) noexcept {
	Tutorial_destroy_framebuffers_impl(
		rtg.device,
		AllocatedImagePtr(swapchain_depth_image),
		swapchain_depth_image_view,
		swapchain_framebuffers
	);
}

void Tutorial_render_record_blank_frame_impl(
	VkExtent2D const &swapchain_extent,
	VkRenderPass render_pass,
	VkFramebuffer framebuffer,
	VkCommandBuffer *workspace_command_buffer
);
template< typename RTG_T >
void Tutorial_render_record_blank_frame(
	RTG_T const &rtg,
	VkRenderPass render_pass,
	VkFramebuffer framebuffer,
	VkCommandBuffer *workspace_command_buffer
) {
	Tutorial_render_record_blank_frame_impl(
		rtg.swapchain_extent,
		render_pass,
		framebuffer,
		workspace_command_buffer
	);
}

void Tutorial_render_submit_impl(
	VkQueue rtg_graphics_queue,
	VkSemaphore render_params_image_available,
	VkSemaphore render_params_image_done,
	VkFence render_params_workspace_available,
	VkCommandBuffer workspace_command_buffer
);
template< typename RTG_T, typename RenderParams_T >
void Tutorial_render_submit(
	RTG_T const &rtg,
	RenderParams_T const &render_params,
	VkCommandBuffer workspace_command_buffer
) {
	Tutorial_render_submit_impl(
		rtg.graphics_queue,
		render_params.image_available,
		render_params.image_done,
		render_params.workspace_available,
		workspace_command_buffer
	);
}


void BackgroundPipeline_create_impl(
	VkDevice rtg_device,
	VkRenderPass render_pass,
	uint32_t subpass,
	VkShaderModule vert_module,
	VkShaderModule frag_module,
	VkPipelineLayout *layout,
	VkPipeline *handle
);
template< typename RTG_T >
void BackgroundPipeline_create(
	RTG_T const &rtg,
	VkRenderPass render_pass,
	uint32_t subpass,
	VkShaderModule vert_module,
	VkShaderModule frag_module,
	VkPipelineLayout *layout,
	VkPipeline *handle
) {
	BackgroundPipeline_create_impl(
		rtg.device,
		render_pass,
		subpass,
		vert_module,
		frag_module,
		layout,
		handle
	);
}


void BackgroundPipeline_destroy_impl(
	VkDevice rtg_device,
	VkPipelineLayout *layout,
	VkPipeline *handle
);
template< typename RTG_T >
void BackgroundPipeline_destroy(
	RTG_T const &rtg,
	VkPipelineLayout *layout,
	VkPipeline *handle
) {
	BackgroundPipeline_destroy_impl(
		rtg.device,
		layout,
		handle
	);
}



uint32_t Helpers_find_memory_type_impl(
	VkPhysicalDevice rtg_physical_device,
	uint32_t type_filter,
	VkMemoryPropertyFlags flags
);
template< typename RTG_T >
uint32_t Helpers_find_memory_type(
	RTG_T const &rtg,
	uint32_t type_filter,
	VkMemoryPropertyFlags flags
) {
	return Helpers_find_memory_type_impl(
		rtg.physical_device,
		type_filter,
		flags
	);
}


VkFormat Helpers_find_image_format_impl(
	VkPhysicalDevice rtg_physical_device,
	std::vector< VkFormat > const &candidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features
);
template< typename RTG_T >
VkFormat Helpers_find_image_format(
	RTG_T const &rtg,
	std::vector< VkFormat > const &candidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features
) {
	return Helpers_find_image_format_impl(
		rtg.physical_device,
		candidates,
		tiling,
		features
	);
}

void Helpers_allocate_impl(
	VkDevice rtg_device,
	VkDeviceSize size,
	VkDeviceSize alignment,
	uint32_t memory_type_index,
	bool should_map,
	AllocationPtr const &allocation
);
template< typename RTG_T, typename Allocation_T >
void Helpers_allocate(
	RTG_T const &rtg,
	VkDeviceSize size,
	VkDeviceSize alignment,
	uint32_t memory_type_index,
	bool should_map,
	Allocation_T *allocation
) {
	Helpers_allocate_impl(
		rtg.device,
		size,
		alignment,
		memory_type_index,
		should_map,
		AllocationPtr(allocation)
	);
}

void Helpers_free_impl(
	VkDevice rtg_device,
	AllocationPtr const &allocation
) noexcept;
template< typename RTG_T, typename Allocation_T >
void Helpers_free(
	RTG_T const &rtg,
	Allocation_T *allocation
) noexcept {
	Helpers_free_impl(
		rtg.device,
		AllocationPtr(allocation)
	);
}

void Helpers_create_buffer_impl(
	VkDevice rtg_device,
	VkPhysicalDevice rtg_physical_device,
	VkDeviceSize size,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	bool should_map,
	//creates:
	AllocatedBufferPtr const &buffer
);
template< typename RTG_T, typename AllocatedBuffer_T >
void Helpers_create_buffer(
	RTG_T const &rtg,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	bool should_map,
	//creates:
	AllocatedBuffer_T *buffer
) {
	Helpers_create_buffer_impl(
		rtg.device,
		rtg.physical_device,
		size,
		usage,
		properties,
		should_map,
		AllocatedBufferPtr( buffer )
	);
}

void Helpers_destroy_buffer_impl(
	VkDevice rtg_device,
	AllocatedBufferPtr const &buffer_
) noexcept ;
template< typename RTG_T, typename AllocatedBuffer_T >
void Helpers_destroy_buffer(
	RTG_T const &rtg,
	//destroys:
	AllocatedBuffer_T *buffer
) {
	Helpers_destroy_buffer_impl(
		rtg.device,
		AllocatedBufferPtr( buffer )
	);
}

void Helpers_create_image_impl(
	VkDevice rtg_device,
	VkPhysicalDevice rtg_physical_device,
	VkExtent2D const &extent,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	bool should_map,
	AllocatedImagePtr const &image
);
template< typename RTG_T, typename AllocatedImage_T >
void Helpers_create_image(
	RTG_T const &rtg,
	VkExtent2D const &extent,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	bool should_map,
	AllocatedImage_T *image
) {
	Helpers_create_image_impl(
		rtg.device,
		rtg.physical_device,
		extent,
		format,
		tiling,
		usage,
		properties,
		should_map,
		AllocatedImagePtr( image )
	);
}

void Helpers_destroy_image_impl(
	VkDevice rtg_device,
	AllocatedImagePtr const &image
) noexcept;
template< typename RTG_T, typename AllocatedImage_T >
void Helpers_destroy_image(
	RTG_T const &rtg,
	//destroys:
	AllocatedImage_T *image
) noexcept {
	Helpers_destroy_image_impl(
		rtg.device,
		AllocatedImagePtr( image )
	);
}

void Helpers_transfer_to_buffer_impl(
	VkDevice rtg_device,
	VkPhysicalDevice rtg_physical_device,
	std::optional< uint32_t > const &rtg_graphics_queue_family,
	VkQueue rtg_graphics_queue,
	void *data,
	size_t size,
	AllocatedBufferPtr const &target
);
template< typename RTG_T, typename AllocatedBuffer_T >
void Helpers_transfer_to_buffer(
	RTG_T const &rtg,
	void *data,
	size_t size,
	AllocatedBuffer_T *target
) {
	Helpers_transfer_to_buffer_impl(
		rtg.device,
		rtg.physical_device,
		rtg.graphics_queue_family,
		rtg.graphics_queue,
		data,
		size,
		AllocatedBufferPtr( target )
	);
}

void Helpers_transfer_to_image_impl(
	VkDevice rtg_device,
	VkPhysicalDevice rtg_physical_device,
	std::optional< uint32_t > const &rtg_graphics_queue_family,
	VkQueue rtg_graphics_queue,
	void *data,
	size_t size,
	AllocatedImagePtr const &target
);
template< typename RTG_T, typename AllocatedImage_T >
void Helpers_transfer_to_image(
	RTG_T const &rtg,
	void *data,
	size_t size,
	AllocatedImage_T *target
) {
	Helpers_transfer_to_image_impl(
		rtg.device,
		rtg.physical_device,
		rtg.graphics_queue_family,
		rtg.graphics_queue,
		data,
		size,
		AllocatedImagePtr( target )
	);
}

void Helpers_create_shader_module_impl(
	VkDevice rtg_device,
	uint32_t const *code,
	size_t bytes,
	VkShaderModule *shader_module
);
template< typename RTG_T >
void Helpers_create_shader_module(
	RTG_T const &rtg,
	uint32_t const *code,
	size_t bytes,
	VkShaderModule *shader_module
) {
	Helpers_create_shader_module_impl(
		rtg.device,
		code,
		bytes,
		shader_module
	);
}



void RTG_constructor_per_workspace_impl(
	VkDevice device,
	VkFence *workspace_available_,
	VkSemaphore *image_available_,
	VkSemaphore *image_done_
);
template< typename PerWorkspace_T >
void RTG_constructor_per_workspace(
	VkDevice device,
	PerWorkspace_T *workspace
) {
	RTG_constructor_per_workspace_impl(
		device,
		&(workspace->workspace_available),
		&(workspace->image_available),
		&(workspace->image_done)
	);
}


void RTG_destructor_per_workspace_impl(
	VkDevice device,
	VkFence *workspace_available_,
	VkSemaphore *image_available_,
	VkSemaphore *image_done_
) noexcept;
template< typename PerWorkspace_T >
void RTG_destructor_per_workspace(
	VkDevice device,
	PerWorkspace_T *workspace
) noexcept {
	RTG_destructor_per_workspace_impl(
		device,
		&(workspace->workspace_available),
		&(workspace->image_available),
		&(workspace->image_done)
	);
}

void RTG_run_impl(
	bool configuration_debug,
	VkDevice device,
	VkQueue present_queue,
	VkSwapchainKHR swapchain,
	GLFWwindow *window,

	std::vector< VkFence > const &workspace_availables,
	std::vector< VkSemaphore > const &image_availables,
	std::vector< VkSemaphore > const &image_dones,
	uint32_t *next_workspace_,

	std::function< VkSwapchainKHR() > const &recreate_swapchain,

	std::function< void(InputEvent) > const &on_input,
	std::function< void() > const &on_swapchain,
	std::function< void(float) > const &update,
	std::function< void(uint32_t, uint32_t) > const &render
);
template< typename RTG_T, typename Application_T >
void RTG_run(RTG_T &rtg, Application_T &application) {
	//Unfortunately, this one is a bit messy because it touches
	// a bunch of classes whose layout it won't know at compile time.
	//(And, generally, just touches a lot of RTG's data and functionality!)

	//de-structure workspaces:
	std::vector< VkFence > workspace_availables;
	std::vector< VkSemaphore > image_availables;
	std::vector< VkSemaphore > image_dones;
	for (auto const &workspace : rtg.workspaces) {
		workspace_availables.emplace_back(workspace.workspace_available);
		image_availables.emplace_back(workspace.image_available);
		image_dones.emplace_back(workspace.image_done);
	}
	
	RTG_run_impl(
		rtg.configuration.debug,
		rtg.device,
		rtg.present_queue,
		rtg.swapchain,
		rtg.window,
		workspace_availables,
		image_availables,
		image_dones,
		&rtg.next_workspace,
		[&rtg]() -> VkSwapchainKHR {
			rtg.recreate_swapchain();
			return rtg.swapchain;
		},
		[&application](InputEvent const &event) {
			application.on_input(event);
		},
		[&rtg, &application]() {
			application.on_swapchain(rtg, typename RTG_T::SwapchainEvent{
				.extent = rtg.swapchain_extent,
				.images = rtg.swapchain_images,
				.image_views = rtg.swapchain_image_views,
			});
		},
		[&application](float dt) {
			application.update(dt);
		},
		[&rtg, &application](uint32_t workspace_index, uint32_t image_index) {
			application.render(rtg, typename RTG_T::RenderParams{
				.workspace_index = workspace_index,
				.image_index = image_index,
				.image_available = rtg.workspaces[workspace_index].image_available,
				.image_done = rtg.workspaces[workspace_index].image_done,
				.workspace_available = rtg.workspaces[workspace_index].workspace_available,
			});
		}
	);
}


} //namespace refsol
