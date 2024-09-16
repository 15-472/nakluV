#include "Tutorial.hpp"

#include "Helpers.hpp"
#include "VK.hpp"

static uint32_t vert_code[] = 
#include "spv/lines.vert.inl"
;

static uint32_t frag_code[] = 
#include "spv/lines.frag.inl"
;

void Tutorial::LinesPipeline::create(RTG &rtg, VkRenderPass render_pass, 
    uint32_t subpass) {
    VkShaderModule vert_module = rtg.helpers.create_shader_module(vert_code);
    VkShaderModule frag_module = rtg.helpers.create_shader_module(frag_code);

    { //the set0_Camera layout holds a Camera structure in a uniform buffer used in the vertex shader:
        std::array< VkDescriptorSetLayoutBinding, 1> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_Camera));

    }

    { //create pipeline layout:
        std::array< VkDescriptorSetLayout, 1> layouts{
            set0_Camera,
        };
       

        VkPipelineLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = uint32_t(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };

        VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout));
    }

    {//create pipeline
        //shader code for vertex and fragment pipeline stages:
        std::array< VkPipelineShaderStageCreateInfo, 2 > stages{
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert_module,
                .pName = "main"
            },
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag_module,
                .pName = "main",
            },
        };

        //the viewport and scissor state will be set at runtime for the pipeline:
		std::vector< VkDynamicState> dynamic_states{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = uint32_t(dynamic_states.size()),
			.pDynamicStates = dynamic_states.data()
		};

    

        //this pipeline will draw lines:
		VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
			.primitiveRestartEnable = VK_FALSE
		};

        //this pipeline will render to one viewport and scissor rectangle:
        VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        //the rasterizer will cull back faces and fill polygons:
        VkPipelineRasterizationStateCreateInfo rasterization_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f,
        };

        //multisampling will be disabled (one sample per pixel);
        VkPipelineMultisampleStateCreateInfo multisample_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable  = VK_FALSE,
        };

        //depth test will be less, and stencil tests will be disabled:
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };

        //there will be one color attachment with blednig disabled:
        std::array< VkPipelineColorBlendAttachmentState, 1> attachment_states{
            VkPipelineColorBlendAttachmentState{
				.blendEnable = VK_FALSE,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			}, 
        };
        VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = uint32_t(attachment_states.size()),
            .pAttachments = attachment_states.data(),
            .blendConstants{0.0f, 0.0f, 0.0f, 0.f},
        };

        //all of the above structures get bundled together into one very large create_info:
        VkGraphicsPipelineCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = uint32_t(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &Vertex::array_input_state,
			.pInputAssemblyState = &input_assembly_state,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterization_state,
			.pMultisampleState = &multisample_state,
			.pDepthStencilState = &depth_stencil_state,
			.pColorBlendState = &color_blend_state,
			.pDynamicState = &dynamic_state,
			.layout = layout,
			.renderPass = render_pass,
			.subpass = subpass,
        };

        VK( vkCreateGraphicsPipelines(rtg.device, VK_NULL_HANDLE, 1, 
            &create_info, nullptr, &handle) );
    }

    //modules no longer needed now that pipeline is created:
    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
}

void Tutorial::LinesPipeline::destroy(RTG &rtg) {
    if (set0_Camera != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_Camera, nullptr);
        set0_Camera = VK_NULL_HANDLE;
    }

    if (layout != VK_NULL_HANDLE){
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (handle != VK_NULL_HANDLE){
        vkDestroyPipeline(rtg.device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
}