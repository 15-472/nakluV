#include "PosNorTexVertex.hpp"

#include <array>

static std::array< VkVertexInputBindingDescription, 1 > bindings{
    VkVertexInputBindingDescription{
        .binding = 0,
        .stride = sizeof(PosNorTexVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }
};

static std::array< VkVertexInputAttributeDescription, 3> attributes {
    VkVertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(PosNorTexVertex, Position),
    },
    VkVertexInputAttributeDescription{
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(PosNorTexVertex, Normal),
    },
    VkVertexInputAttributeDescription{
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(PosNorTexVertex, TexCoord),
    },
};

const VkPipelineVertexInputStateCreateInfo PosNorTexVertex::array_input_state{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = uint32_t(bindings.size()),
    .pVertexBindingDescriptions = bindings.data(),
    .vertexAttributeDescriptionCount = uint32_t(attributes.size()),
    .pVertexAttributeDescriptions = attributes.data(),
};