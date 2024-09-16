#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

struct PosColVertex {
    struct { float x, y, z;} Position;
    struct { uint8_t r,g,b,a; } Color;
    //a pipeline vertex input state that works with a buffer holding a PosColVertex[] array:
    static const VkPipelineVertexInputStateCreateInfo array_input_state;
};

static_assert(sizeof(PosColVertex) == 3*4 + 4*1, "PosColVertex is packed.");