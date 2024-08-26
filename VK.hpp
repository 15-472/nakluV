#pragma once

#include <vulkan/vk_enum_string_helper.h>

#include <stdexcept>

#define VK( FN ) \
	if (VkResult result = FN; result != VK_SUCCESS) { \
		throw std::runtime_error("Call '" #FN "' returned " + std::to_string(result) + " [" + std::string(string_VkResult(result)) + "]." ); \
	}

