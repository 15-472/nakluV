#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "A1.hpp"

#include "VK.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stack>          


A1::A1(RTG &rtg_) : rtg(rtg_) {
	//select a depth format:
	//  (at least one of these two must be supported, according to the spec; but neither are required)
	depth_format = rtg.helpers.find_image_format(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);


	{//create render pass
		//attachments
		std::array< VkAttachmentDescription, 2 > attachments{
			VkAttachmentDescription{ //0 - color attachment:
				.format = rtg.surface_format.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			},
			VkAttachmentDescription{ //1 - depth attachment:
				.format = depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};

		//subpass
		VkAttachmentReference color_attachment_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference depth_attachment_ref{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_ref,
			.pDepthStencilAttachment = &depth_attachment_ref,
		};

		//this defers the image load actions for the attachments:
		std::array< VkSubpassDependency, 2 > dependencies{
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			},
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(dependencies.size()),
			.pDependencies = dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &create_info, nullptr, &render_pass) );
	}

	{//create command pool
		VkCommandPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = rtg.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(rtg.device, &create_info, nullptr, &command_pool) );
	}

	background_pipeline.create(rtg, render_pass, 0);
	lines_pipeline.create(rtg, render_pass, 0);
	objects_pipeline.create(rtg, render_pass, 0);

	{ //create descriptor pool:
		uint32_t per_workspace = uint32_t(rtg.workspaces.size()); //for easire-to-read counting

		std::array< VkDescriptorPoolSize, 2> pool_sizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 2 * per_workspace, //one descriptor per set, two set per workspace
			},
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1 * per_workspace, //one descriptor per set, one set per workspace
			}
		};

		VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 3 * per_workspace,  //three set per workspace
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool));
	}

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces) {
		{ //allocate command buffer:
			VkCommandBufferAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = command_pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};
			VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &workspace.command_buffer) );
		}
		
		workspace.Camera_src = rtg.helpers.create_buffer(
			sizeof(LinesPipeline::Camera),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, //going to have GPU copy from this memory
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //host-visible memory, coherent (no special sync needed)
			Helpers::Mapped //get a pointer to the memory
		);
		workspace.Camera = rtg.helpers.create_buffer(
			sizeof(LinesPipeline::Camera), 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, //going to use as a unifrom buffer, also goint to have GPU copu into this memory
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //GPU-local memory
			Helpers::Unmapped //don't get a pointer to the memory
		);

		{ //allocate descriptor set for Camera descriptor
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &lines_pipeline.set0_Camera,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Camera_descriptors));
		}

		workspace.World_src = rtg.helpers.create_buffer(
			sizeof(ObjectsPipeline::World),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Helpers::Mapped
		);
		workspace.World = rtg.helpers.create_buffer(
			sizeof(ObjectsPipeline::World),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		{ //allocate descriptor set for World descriptor
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set0_World,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.World_descriptors) );
			//NOTE: will actually fill in this descriptor set just a bit lower
		}

		{ //allocate descriptor set for Transforms descriptor
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set1_Transforms,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Transforms_descriptors) );
			//NOTE: will fill in this descriptor set in render when buffers are [re-]allocated
		}

		{ //point descriptor to Camera buffer:
			VkDescriptorBufferInfo Camera_info{
				.buffer = workspace.Camera.handle,
				.offset = 0,
				.range = workspace.Camera.size,
			};

			VkDescriptorBufferInfo World_info{
				.buffer = workspace.World.handle,
				.offset = 0,
				.range = workspace.World.size,
			};

			std::array< VkWriteDescriptorSet, 2> writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Camera_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &Camera_info,
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.World_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &World_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device, //device
				uint32_t(writes.size()), //descriptorWriteCount
				writes.data(), //pDescriptorWrites
				0, //descriptorCopyCount
				nullptr //pDescriptorCopies
			);
		}
	}
	
	{ //create object vertices
		std::vector< PosNorTexVertex > vertices;

		
		// { //A [-1,1]x[-1,1]x{0} quadrilateral:
		// 	plane_vertices.first = uint32_t(vertices.size());
		// 	vertices.emplace_back(PosNorTexVertex{
		// 		.Position{ .x = -1.0f, .y = -1.0f, .z = 0.0f },
		// 		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
		// 		.TexCoord{ .s = 0.0f, .t = 0.0f },
		// 	});
		// 	vertices.emplace_back(PosNorTexVertex{
		// 		.Position{ .x = 1.0f, .y = -1.0f, .z = 0.0f },
		// 		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
		// 		.TexCoord{ .s = 1.0f, .t = 0.0f },
		// 	});
		// 	vertices.emplace_back(PosNorTexVertex{
		// 		.Position{ .x = -1.0f, .y = 1.0f, .z = 0.0f },
		// 		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
		// 		.TexCoord{ .s = 0.0f, .t = 1.0f },
		// 	});
		// 	vertices.emplace_back(PosNorTexVertex{
		// 		.Position{ .x = 1.0f, .y = 1.0f, .z = 0.0f },
		// 		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f },
		// 		.TexCoord{ .s = 1.0f, .t = 1.0f },
		// 	});
		// 	vertices.emplace_back(PosNorTexVertex{
		// 		.Position{ .x = -1.0f, .y = 1.0f, .z = 0.0f },
		// 		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
		// 		.TexCoord{ .s = 0.0f, .t = 1.0f },
		// 	});
		// 	vertices.emplace_back(PosNorTexVertex{
		// 		.Position{ .x = 1.0f, .y = -1.0f, .z = 0.0f },
		// 		.Normal{ .x = 0.0f, .y = 0.0f, .z = 1.0f},
		// 		.TexCoord{ .s = 1.0f, .t = 0.0f },
		// 	});

		// 	plane_vertices.count = uint32_t(vertices.size()) - plane_vertices.first;
		// }

		{// Create mesh vertices
			for(const auto & [key, value] :rtg.meshes){
				RTG::Mesh mesh = value;
				std::cout << "Creating mesh " << key << " with size " << mesh.indices.size() << std::endl;
				std::cout << "position size " << mesh.position.size() << std::endl;
				ObjectVertices this_vertices;
				this_vertices.first	 = uint32_t(vertices.size());
				//Assume Triangle_List
				AABB box;
				box.minX = mesh.position[0];
				box.maxX = mesh.position[0];
				box.minY = mesh.position[1];
				box.maxY = mesh.position[1];
				box.minZ = mesh.position[2];
				box.maxZ = mesh.position[2];
				for(uint32_t idx : mesh.indices){
					vertices.emplace_back(PosNorTexVertex{
						.Position{ .x = mesh.position[idx * 3], .y = mesh.position[idx * 3 + 1], .z = mesh.position[idx * 3 + 2] },
						.Normal{ .x = mesh.normal[idx * 3], .y = mesh.normal[idx * 3 + 1], .z = mesh.normal[idx * 3 + 2]},
						.TexCoord{ .s = mesh.texcoord[idx * 2], .t = mesh.texcoord[idx * 2 + 1] },
					});
					if(mesh.position[idx * 3] < box.minX) box.minX = mesh.position[idx * 3];
					if(mesh.position[idx * 3 + 1] < box.minY) box.minY = mesh.position[idx * 3 + 1];
					if(mesh.position[idx * 3 + 2] < box.minZ) box.minZ = mesh.position[idx * 3 + 2];
					if(mesh.position[idx * 3] > box.minX) box.minX = mesh.position[idx * 3];
					if(mesh.position[idx * 3 + 1] > box.minY) box.minY = mesh.position[idx * 3 + 1];
					if(mesh.position[idx * 3 + 2] > box.minZ) box.minZ = mesh.position[idx * 3 + 2];
				}


				this_vertices.count = uint32_t(vertices.size()) - this_vertices.first;
				mesh_box[key] = box;
				mesh_vertices[key] = this_vertices;

			}
		}

		size_t bytes = vertices.size() * sizeof(vertices[0]);

		object_vertices = rtg.helpers.create_buffer(
			bytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		//copy data to buffer;
		rtg.helpers.transfer_to_buffer(vertices.data(), bytes, object_vertices);
	}


	{ //make some textures
		textures.reserve(2);
		// textures.reserve(rtg.materials.size());

		//Loop over materials to get the textures
		// for(const auto & [key, value] : rtg.materials){
		// 	RTG::Material material = value;

		// }


		{ //texture 0 will be a dark grey / light grey checkerboard with a red square at the origin.
			//actually make the texture:
			uint32_t size = 128;
			std::vector< uint32_t > data;
			data.reserve(size * size);
			for (uint32_t y = 0; y < size; ++y) {
				float fy = (y + 0.5f) / float(size);
				for (uint32_t x = 0; x < size; ++x) {
					float fx = (x + 0.5f) / float(size);
					//highlight the origin:
					if      (fx < 0.05f && fy < 0.05f) data.emplace_back(0xff0000ff); //red
					else if ( (fx < 0.5f) == (fy < 0.5f)) data.emplace_back(0xff444444); //dark grey
					else data.emplace_back(0xffbbbbbb); //light grey
				}
			}
			assert(data.size() == size*size);

			//make a place for the texture to live on the GPU
			textures.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = size , .height = size }, //size of image
				VK_FORMAT_R8G8B8A8_UNORM, //how to interpret image data (in this case, linearly-encoded 8-bit RGBA)
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, //will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //should be device-local
				Helpers::Unmapped
			));

			//transfer data:
			rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures.back());
		}

		{ //texture 1 will be a classic 'xor' texture
			//actually make the texture:
			uint32_t size = 256;
			std::vector< uint32_t > data;
			data.reserve(size * size);
			for (uint32_t y = 0; y < size; ++y) {
				for (uint32_t x = 0; x < size; ++x) {
					uint8_t r = uint8_t(x) ^ uint8_t(y);
					uint8_t g = uint8_t(x + 128) ^ uint8_t(y);
					uint8_t b = uint8_t(x) ^ uint8_t(y + 27);
					uint8_t a = 0xff;
					data.emplace_back( uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24) );
				}
			}
			assert(data.size() == size*size);

			//make a place for the texture to live on the GPU:
			textures.emplace_back(rtg.helpers.create_image(
				VkExtent2D{ .width = size , .height = size }, //size of image
				VK_FORMAT_R8G8B8A8_SRGB, //how to interpret image data (in this case, SRGB-encoded 8-bit RGBA)
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, //will sample and upload
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //should be device-local
				Helpers::Unmapped
			));

			//transfer data:
			rtg.helpers.transfer_to_image(data.data(), sizeof(data[0]) * data.size(), textures.back());
		}
	}

	{ //make image views for the textures
		texture_views.reserve(textures.size());
		for (Helpers::AllocatedImage const &image : textures) {
			VkImageViewCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.flags = 0,
				.image = image.handle,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = image.format,
				// .components sets swizzling and is fine when zero-initialized
				.subresourceRange{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};

			VkImageView image_view = VK_NULL_HANDLE;
			VK( vkCreateImageView(rtg.device, &create_info, nullptr, &image_view) );

			texture_views.emplace_back(image_view);
		}
		assert(texture_views.size() == textures.size());
	}

	{ //make a sampler for the textures
		VkSamplerCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.flags = 0,
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 0.0f, //doesn't matter if anisotropy isn't enabled
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS, //doesn't matter if compare isn't enabled
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VK( vkCreateSampler(rtg.device, &create_info, nullptr, &texture_sampler) );
	}
		
	{ //create the texture descriptor pool
		uint32_t per_texture = uint32_t(textures.size()); //for easier-to-read counting

		std::array< VkDescriptorPoolSize, 1> pool_sizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1 * 1 * per_texture, //one descriptor per set, one set per texture
			},
		};
		
		VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 1 * per_texture, //one set per texture
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &texture_descriptor_pool) );
	}

	{ //allocate and write the texture descriptor sets
		//allocate the descriptors (using the same alloc_info):
		VkDescriptorSetAllocateInfo alloc_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = texture_descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &objects_pipeline.set2_TEXTURE,
		};

		texture_descriptors.assign(textures.size(), VK_NULL_HANDLE);
		for (VkDescriptorSet &descriptor_set : texture_descriptors) {
			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set) );
		}

		//write descriptors for textures
		std::vector< VkDescriptorImageInfo > infos(textures.size());
		std::vector< VkWriteDescriptorSet > writes(textures.size());

		for (Helpers::AllocatedImage const &image : textures) {
			size_t i = &image - &textures[0];
			
			infos[i] = VkDescriptorImageInfo{
				.sampler = texture_sampler,
				.imageView = texture_views[i],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			writes[i] = VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = texture_descriptors[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[i],
			};
		}

		vkUpdateDescriptorSets( rtg.device, uint32_t(writes.size()), writes.data(), 0, nullptr );
	} 


	{//Initialize cameras
		//Scene camera
	}
}

A1::~A1() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in A1::~A1 [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if (texture_descriptor_pool) {
		vkDestroyDescriptorPool(rtg.device, texture_descriptor_pool, nullptr);
		texture_descriptor_pool = nullptr;

		//this also frees the descriptor sets allocated from the pool:
		texture_descriptors.clear();
	}

	if (texture_sampler) {
		vkDestroySampler(rtg.device, texture_sampler, nullptr);
		texture_sampler = VK_NULL_HANDLE;
	}

	for (VkImageView &view : texture_views) {
		vkDestroyImageView(rtg.device, view, nullptr);
		view = VK_NULL_HANDLE;
	}
	texture_views.clear();

	for (auto &texture : textures) {
		rtg.helpers.destroy_image(std::move(texture));
	}
	textures.clear();

	rtg.helpers.destroy_buffer(std::move(object_vertices));

	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	background_pipeline.destroy(rtg);
	lines_pipeline.destroy(rtg);
	objects_pipeline.destroy(rtg);

	for (Workspace &workspace : workspaces) {
		if (workspace.command_buffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(rtg.device, command_pool, 1, &workspace.command_buffer);
			workspace.command_buffer = VK_NULL_HANDLE;
		}

		if (workspace.lines_vertices_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices_src));
		}
		if (workspace.lines_vertices.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.lines_vertices));
		}
		
		if (workspace.Camera_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Camera_src));
		}
		if (workspace.Camera.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Camera));
		}
		//Camera_descriptors freed when pool is destroyed.
		
		if (workspace.World_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.World_src));
		}
		if (workspace.World.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.World));
		}
		//World_descriptors freed when pool is destroyed.

		if (workspace.Transforms_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
		}
		if (workspace.Transforms.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
		}
		//Transforms_descriptors freed when pool is destroyed.
	}
	workspaces.clear();

	if(descriptor_pool) {
		vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
		descriptor_pool = nullptr;
		//(this also frees the descriptor sets allocated from the pool)
	}

	//destroy command pool
	if (command_pool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(rtg.device, command_pool, nullptr);
		command_pool = VK_NULL_HANDLE;
	}

	if (render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, render_pass, nullptr);
		render_pass = VK_NULL_HANDLE;
	}
}

void A1::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	//clean up existing framebuffers (and depth image):
	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	//allocate depth image for framebuffers to share
	swapchain_depth_image = rtg.helpers.create_image(
		swapchain.extent,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	{ //create depth image view:
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_depth_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &create_info, nullptr, &swapchain_depth_image_view) );
	}


	//Make framebuffers for each swapchain image:
	swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain.image_views.size(); ++i) {

		std::array< VkImageView, 2 > attachments{
			swapchain.image_views[i],
			swapchain_depth_image_view,
		};
		VkFramebufferCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1,
		};

		VK( vkCreateFramebuffer(rtg.device, &create_info, nullptr, &swapchain_framebuffers[i]) );
	}
	
	//TODO: Swapchain print
}

void A1::destroy_framebuffers() {
	for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
		assert(framebuffer != VK_NULL_HANDLE);
		vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}
	swapchain_framebuffers.clear();

	assert(swapchain_depth_image_view != VK_NULL_HANDLE);
	vkDestroyImageView(rtg.device, swapchain_depth_image_view, nullptr);
	swapchain_depth_image_view = VK_NULL_HANDLE;

	rtg.helpers.destroy_image(std::move(swapchain_depth_image));
}


void A1::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	//reset the command buffer (clear old commands):
	VK( vkResetCommandBuffer(workspace.command_buffer, 0));
	{//Begin recording:
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};
		VK( vkBeginCommandBuffer(workspace.command_buffer, &begin_info) );
	}

	if (!object_instances.empty()) { //upload object transforms:
		size_t needed_bytes = object_instances.size() * sizeof(ObjectsPipeline::Transform);
		if (workspace.Transforms_src.handle == VK_NULL_HANDLE || workspace.Transforms_src.size < needed_bytes) {
			// round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly;
			size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
			if(workspace.Transforms.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
			}
			if(workspace.Transforms.handle) {
				rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
			}

			workspace.Transforms_src = rtg.helpers.create_buffer(
				new_bytes,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, //going to have GPU copy from this memory
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //host-visibble memory, coherent (no special sync needed)
				Helpers::Mapped //get a pointer to the memory
			);
			workspace.Transforms = rtg.helpers.create_buffer(
				new_bytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // going to use as storage buffer, also going to have GPU into this memory
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //GPU-local memory
				Helpers::Unmapped //don't get a pointer to the memory
			);

			//update the descriptor set:
			VkDescriptorBufferInfo Transforms_info{
				.buffer = workspace.Transforms.handle,
				.offset = 0,
				.range = workspace.Transforms.size,
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.Transforms_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &Transforms_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device,
				uint32_t(writes.size()), writes.data(), //descriptorWrites count, data
				0, nullptr //descriptorCopies count, data
			);

			std::cout << "Re-allocated object transforms buffer to " << new_bytes << " bytes." << std::endl;
		}

		assert(workspace.Transforms_src.size == workspace.Transforms.size);
		assert(workspace.Transforms_src.size >= needed_bytes);

		{//copy transforms into Transforms_src:
			assert(workspace.Transforms_src.allocation.mapped);
			ObjectsPipeline::Transform *out = reinterpret_cast< ObjectsPipeline::Transform *>(
				workspace.Transforms_src.allocation.data()
			); //Strict aliasing violation, but it doesn't matter
			for (ObjectInstance const &inst : object_instances) {
				*out = inst.transform;
				++out;
			}
		}
		// std::memcpy(workspace.lines_vertices_src.allocation.data(),
		// 			lines_vertices.data(), needed_bytes);

		//device-side copy from lines_vertices_src -> lines_vertices:
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = needed_bytes,
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.Transforms_src.handle,
					workspace.Transforms.handle, 1, &copy_region);
	}

	{ //upload camera info:
		LinesPipeline::Camera camera{
			.CLIP_FROM_WORLD = CLIP_FROM_WORLD
		};

		assert(workspace.Camera_src.size == sizeof(camera));

		//host-side copy into Camera_src:
		memcpy(workspace.Camera_src.allocation.data(), &camera, sizeof(camera));

		//add device-side copy from Camera_src -> Camera:
		assert(workspace.Camera_src.size == workspace.Camera.size);
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.Camera_src.size,
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.Camera_src.handle,
			workspace.Camera.handle, 1, &copy_region);
	}

	{ //upload world info:
		assert(workspace.Camera_src.size == sizeof(world));

		//host-side copy into World_src:
		memcpy(workspace.World_src.allocation.data(), &world, sizeof(world));

		//add device-side copy from World_src -> World:
		assert(workspace.World_src.size == workspace.World.size);
		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = workspace.World_src.size,
		};
		vkCmdCopyBuffer(workspace.command_buffer, workspace.World_src.handle, workspace.World.handle, 1, &copy_region);
	}

	{ //memory barrier to make sure copies complete before rendering happens:
		VkMemoryBarrier memory_barrier{
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		};

		vkCmdPipelineBarrier( workspace.command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, //srcStageMask
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, //dstStageMask
			0, //dependencyFlags
			1, &memory_barrier, //memoryBarriers (count, data)
			0, nullptr, //bufferMemoryBarriers (count, data)
			0, nullptr //imageMemoryBarriers (count, data)
		);

	}

	{// render pass
		std::array< VkClearValue, 2 > clear_values{
			VkClearValue{ .color{ .float32{0.8f, 0.5f, .3f, 1.f}}},
			VkClearValue{ .depthStencil{ .depth = 1.f, .stencil = 0}},
		};

		VkRenderPassBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = render_pass,
			.framebuffer = framebuffer,
			.renderArea{
				.offset = {.x = 0, .y = 0},
				.extent = rtg.swapchain_extent,
			},
			.clearValueCount = uint32_t(clear_values.size()),
			.pClearValues = clear_values.data(),
		};

		vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		{ //set scissor rectangle:
			VkRect2D scissor{
				.offset = {.x = 0, .y = 0},
				.extent = rtg.swapchain_extent,
			};
			vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor);
		}
		{ //configure viewport transform;
			VkViewport viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = float(rtg.swapchain_extent.width),
				.height = float(rtg.swapchain_extent.height),
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			};
			vkCmdSetViewport(workspace.command_buffer, 0, 1, &viewport);
		}

		{ //draw with the background pipeline:
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				background_pipeline.handle);
			
			{ //push time:
				BackgroundPipeline::Push push{
					.time = float(time),
				};
				
				vkCmdPushConstants(workspace.command_buffer , background_pipeline.layout ,
					 VK_SHADER_STAGE_FRAGMENT_BIT , 0 , sizeof(push) , &push);
			}

			vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
		}

		// { //draw with the lines pipeline:
		// 	vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline.handle);
			
		// 	{ //use line_vertices (offset 0 ) as vertex buffer binding 0:
		// 		std::array< VkBuffer, 1> vertex_buffers{workspace.lines_vertices.handle };
		// 		std::array< VkDeviceSize, 1> offsets{0};
		// 		vkCmdBindVertexBuffers(workspace.command_buffer, 0, 
		// 							uint32_t(vertex_buffers.size()), 
		// 							vertex_buffers.data(), offsets.data());
		// 	}

		// 	{ //bind Camera descriptor set:
		// 		std::array< VkDescriptorSet, 1 > descriptor_sets{
		// 			workspace.Camera_descriptors, //0: Camera
		// 		};
		// 		vkCmdBindDescriptorSets(
		// 			workspace.command_buffer, //command buffer
		// 			VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
		// 			lines_pipeline.layout, //pipeline layout
		// 			0, //first set
		// 			uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor sets count, ptr
		// 			0, nullptr //dynamic offsets count, ptr
		// 		);
		// 	}
		// 	//draw lines vertices:
		// 	vkCmdDraw(workspace.command_buffer, uint32_t(lines_vertices.size()), 1, 0, 0);
		// }

		if (!object_instances.empty()) { //draw with the objects pipeline:
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objects_pipeline.handle);

			{ //use object_vertices (offset 0) as vertex buffer binding 0:
				std::array< VkBuffer, 1> vertex_buffers{ object_vertices.handle };
				std::array< VkDeviceSize, 1> offsets{ 0 };
				vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), 
								vertex_buffers.data(), offsets.data());
			}

			{ //bind Transforms descriptor set:
				std::array< VkDescriptorSet, 2 > descriptor_sets{
					workspace.World_descriptors, //0: World
					workspace.Transforms_descriptors, //1: Transforms
				};
				vkCmdBindDescriptorSets(
					workspace.command_buffer, //command buffer
					VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
					objects_pipeline.layout, //pipeline layout
					0, //first set
					uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor sets count, ptr
					0, nullptr //dynamic offsets count, ptr
				);
			}

			//Camera descriptor set is still bound, but unused(!)

			//draw all instances:
			for (ObjectInstance const &inst : object_instances) {
				uint32_t index = uint32_t(&inst - &object_instances[0]);

				//bind texture descriptor set:
				vkCmdBindDescriptorSets(
					workspace.command_buffer, //command buffer
					VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
					objects_pipeline.layout, //pipeline layout
					2, //second set
					1, &texture_descriptors[inst.texture], //descriptor sets count, ptr
					0, nullptr //dynamic offsets count, ptr
				);

				vkCmdDraw(workspace.command_buffer, inst.vertices.count, 1, inst.vertices.first, index);
			}

		}


		vkCmdEndRenderPass(workspace.command_buffer);
	}

	//End recording:
	VK( vkEndCommandBuffer(workspace.command_buffer) );

	{//submit `workspace.command buffer` for the GPU to run:
		std::array< VkSemaphore, 1 > wait_semaphores{
			render_params.image_available
		};
		std::array< VkPipelineStageFlags, 1 > wait_stages{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		static_assert(wait_semaphores.size() == wait_stages.size(), "every semaphore needs a stage");

		std::array< VkSemaphore, 1 > signal_semaphores{
			render_params.image_done
		};
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = uint32_t(wait_semaphores.size()),
			.pWaitSemaphores = wait_semaphores.data(),
			.pWaitDstStageMask = wait_stages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &workspace.command_buffer,
			.signalSemaphoreCount = uint32_t(signal_semaphores.size()),
			.pSignalSemaphores = signal_semaphores.data(),
		};

		VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, render_params.workspace_available) );
	}
}



void A1::update(float dt) {
	time = std::fmod( time + dt, 60.0f);

	object_instances.clear();

	// { //camera orbiting the origin:
	// 	float ang = float(M_PI) * 2.f * 4.f * (time / 60.f);
	// 	CLIP_FROM_WORLD = perspective(
	// 		60.f / float(M_PI) * 180.f, //vfov
	// 		rtg.swapchain_extent.width / float(rtg.swapchain_extent.height), //aspect
	// 		0.1f, //near
	// 		1000.f //far
	// 	) * look_at(
	// 		3.f * std::cos(ang), 3.f * std::sin(ang), 1.f, //eye
	// 		0.f, 0.f, 0.5f, //target
	// 		0.f, 0.f, 1.f //up
	// 	);
	// }

	
	 
	{ //static sun and sky:
		world.SKY_DIRECTION.x = 0.0f;
		world.SKY_DIRECTION.y = 0.0f;
		world.SKY_DIRECTION.z = 1.0f;

		world.SKY_ENERGY.r = 0.2f;
		world.SKY_ENERGY.g = 0.2f;
		world.SKY_ENERGY.b = 0.4f;

		world.SUN_DIRECTION.x = 6.0f / 23.0f;
		world.SUN_DIRECTION.y = 13.0f / 23.0f;
		world.SUN_DIRECTION.z = 18.0f / 23.0f;

		world.SUN_ENERGY.r = 1.0f;
		world.SUN_ENERGY.g = 1.0f;
		world.SUN_ENERGY.b = 0.9f;
	}



	// MOM get the camera
	if(rtg.camera_mode.compare("user") == 0){
		RTG::OrbitCamera oc = rtg.user_camera;
		CLIP_FROM_WORLD = perspective(
			rtg.active_camera.vfov, //vfov
			rtg.active_camera.aspect, //aspect
			rtg.active_camera.near, //near
			rtg.active_camera.far //far
		) * look_at(
			oc.radius * std::cos(oc.azimuth) * std::sin(oc.elevation), 
			oc.radius * std::sin(oc.azimuth) * std::sin(oc.elevation), 
			oc.radius * std::cos(oc.elevation),
			oc.target[0], oc.target[1], oc.target[2], //target
			0.f, 0.f, 1.f //up
		);
	} else {
		for(std::string root : rtg.scene.roots){
			RTG::Node this_node = rtg.nodes[root];
			
			std::stack<mat4> node_transform_stack;
			std::stack<RTG::Node> node_stack;
			
			mat4 origin = mat4{1.f, 0.f, 0.f, 0.f, 
								0.f, 1.f, 0.f, 0.f,
								0.f, 0.f, 1.f, 0.f,
								0.f, 0.f, 0.f, 1.f};
			node_transform_stack.push(origin); //Store World from parent transform
			node_stack.push(this_node);
			while(!node_stack.empty()){
				this_node = node_stack.top();
				node_stack.pop();
				mat4 WORLD_FROM_PARENT = node_transform_stack.top();
				node_transform_stack.pop();
				mat4 WORLD_FROM_LOCAL = WORLD_FROM_PARENT * this_node.parent_from_local;

				for(std::string child : this_node.children){
					node_stack.push(rtg.nodes[child]);
					node_transform_stack.push(WORLD_FROM_LOCAL);
				}
				if(!this_node.camera.empty() && this_node.camera.compare(rtg.active_camera.name) == 0){
					RTG::Camera camera = rtg.cameras[this_node.camera];
					// float ang = float(M_PI) * 2.f * 4.f * (time / 60.f);
					vec4 eye = WORLD_FROM_LOCAL * vec4{0.f, 0.f, 0.f, 1.f};
					vec4 target = WORLD_FROM_LOCAL * vec4{0.f, 0.f, -1.f, 1.f};
					vec4 up = WORLD_FROM_LOCAL * vec4{0.f, 1.f, 0.f, 1.f};

					CLIP_FROM_WORLD = perspective(
						camera.vfov, //vfov
						camera.aspect, //aspect
						camera.near, //near
						camera.far //far
					) * look_at(
						eye[0]/eye[3], eye[1]/eye[3], eye[2]/eye[3], //eye
						target[0]/target[3], target[1]/target[3], target[2]/target[3], //target
						up[0], up[1], up[2]//up
					);
					// std::cout << "Eye " << eye[0] << " , " << eye[1] <<" , " << eye[2] <<" , " << eye[3]  << std::endl;
					// std::cout << "Target " << target[0] << " , " << target[1] <<" , " << target[2] <<" , " << target[3]  << std::endl;
				}
				
			}
		}
	}
	
	//Find that mesh
	for(std::string root : rtg.scene.roots){
		RTG::Node this_node = rtg.nodes[root];
		
		std::stack<mat4> node_transform_stack;
		std::stack<RTG::Node> node_stack;
		
		mat4 origin = mat4{1.f, 0.f, 0.f, 0.f, 
							0.f, 1.f, 0.f, 0.f,
							0.f, 0.f, 1.f, 0.f,
							0.f, 0.f, 0.f, 1.f};
		node_transform_stack.push(origin); //Store World from parent transform
		node_stack.push(this_node);
		while(!node_stack.empty()){
			this_node = node_stack.top();
			node_stack.pop();
			mat4 WORLD_FROM_PARENT = node_transform_stack.top();
			node_transform_stack.pop();
			mat4 WORLD_FROM_LOCAL = WORLD_FROM_PARENT * this_node.parent_from_local;

			for(std::string child : this_node.children){
				node_stack.push(rtg.nodes[child]);
				node_transform_stack.push(WORLD_FROM_LOCAL);
			}

			

			//Check the Mesh
			if(!this_node.mesh.empty()){
				//Check culling
				RTG::Mesh this_mesh = rtg.meshes[this_node.mesh];
				if(in_view(mesh_box[this_mesh.name], WORLD_FROM_LOCAL)){
					uint32_t tex_num = 0;
					if(!this_mesh.material.empty()){
						tex_num = rtg.materials[this_mesh.material].texture_number;
					}
					object_instances.emplace_back(ObjectInstance{
						.vertices = mesh_vertices[this_mesh.name],
						.transform{
							.CLIP_FROM_LOCAL = CLIP_FROM_WORLD * WORLD_FROM_LOCAL,
							.WORLD_FROM_LOCAL = WORLD_FROM_LOCAL,
							.WORLD_FROM_LOCAL_NORMAL = WORLD_FROM_LOCAL,
						},
						.texture = tex_num,
					});
				}
			}
			//Check the Light
			if(!this_node.light.empty()){
				
			}
			//TODO: Environment later
			// if(!this_node.mesh){
				
			// }
		}
	}


	{ //make some objects:
		
		{ //plane translated +x by one unit:
			mat4 WORLD_FROM_LOCAL{
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f,
			};

			

			object_instances.emplace_back(ObjectInstance{
				.vertices = plane_vertices,
				.transform{
					.CLIP_FROM_LOCAL = CLIP_FROM_WORLD * WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL = WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL_NORMAL = WORLD_FROM_LOCAL,
				},
				.texture = 1,
			});
			
		}

		{ //torus translated -x by one unit and rotated CCW around +y:
			float ang = time / 60.0f * 2.0f * float(M_PI) * 10.0f;
			float ca = std::cos(ang);
			float sa = std::sin(ang);
			mat4 WORLD_FROM_LOCAL{
				  ca, 0.0f,  -sa, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				  sa, 0.0f,   ca, 0.0f,
				-1.0f,0.0f, 0.0f, 1.0f,
			};

			object_instances.emplace_back(ObjectInstance{
				.vertices = torus_vertices,
				.transform{
					.CLIP_FROM_LOCAL = CLIP_FROM_WORLD * WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL = WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL_NORMAL = WORLD_FROM_LOCAL,
				},
			});
		}
		{ //sphere translated +z by and bounce:
			float z = std::sin(float(M_PI) * time / 60.f) + 0.5f;
			mat4 WORLD_FROM_LOCAL{
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, z, 1.0f,
			};

			object_instances.emplace_back(ObjectInstance{
				.vertices = sphere_vertices,
				.transform{
					.CLIP_FROM_LOCAL = CLIP_FROM_WORLD * WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL = WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL_NORMAL = WORLD_FROM_LOCAL,
				},
				.texture = 1,
			});
			
		}

		{ //sphere translated  to (0,1,.5) and ovaled:
			mat4 WORLD_FROM_LOCAL{
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.5f, 0.0f,
				0.0f, 1.0f, .5f, 1.0f,
			};

			object_instances.emplace_back(ObjectInstance{
				.vertices = sphere_vertices,
				.transform{
					.CLIP_FROM_LOCAL = CLIP_FROM_WORLD * WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL = WORLD_FROM_LOCAL,
					.WORLD_FROM_LOCAL_NORMAL = WORLD_FROM_LOCAL,
				},
				.texture = 0,
			});
			
		}

	}

}


void A1::on_input(InputEvent const &input) {
	if(input.type == InputEvent::KeyDown) {
		if(input.key.key == 85){ //U
			std::cout << "Entering user Camera mode" << std::endl;
			rtg.camera_mode = "user";
		}
		if(input.key.key == 83){ //S
			std::cout << "Entering user Scene mode" << std::endl;
			rtg.camera_mode = "scene";
		}
	}
	if(input.type == InputEvent::MouseButtonDown){
		mouse_down = true;
		x0 = input.button.x;
		y0 = input.button.y;
	}
	if(input.type == InputEvent::MouseButtonUp){
		mouse_down = false;
	}
	if(mouse_down && input.type == InputEvent::MouseMotion && rtg.camera_mode.compare("user") == 0){
		float dx = x0 - input.button.x;
		float dy = y0 - input.button.y;
		
		dx = dx / 180.f;
		dy = dy / 180.f;

		//Translate 
		rtg.user_camera.elevation = std::min(std::max(rtg.user_camera.elevation+dy, 0.f), float(M_PI));
		rtg.user_camera.azimuth += dx;
		if(rtg.user_camera.azimuth > 2.f * float(M_PI)){
			rtg.user_camera.azimuth -= 2.f * float(M_PI);
		} else if(rtg.user_camera.azimuth < 0.f){
			rtg.user_camera.azimuth += 2.f * float(M_PI);
		}
		x0 = input.button.x;
		y0 = input.button.y;
	}
	if(input.type == InputEvent::MouseWheel && rtg.camera_mode.compare("user") == 0){
		rtg.user_camera.radius = std::max(rtg.user_camera.radius + input.wheel.y, 0.01f);
	}
}

void A1::create_frustum(){
	return;
}

bool A1::in_view(AABB box, mat4 transform){
	return true;
}
