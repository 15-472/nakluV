#include "RTG.hpp"

#include "VK.hpp"
#include "sejp.hpp"



#include <vulkan/vulkan_core.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h> //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/vk_enum_string_helper.h> //useful for debug output
#include <GLFW/glfw3.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <set>

void RTG::Configuration::parse(int argc, char **argv) {
	for (int argi = 1; argi < argc; ++argi) {
		std::string arg = argv[argi];
		if (arg == "--debug") {
			debug = true;
		} else if (arg == "--no-debug") {
			debug = false;
		} else if (arg == "--physical-device") {
			if (argi + 1 >= argc) throw std::runtime_error("--physical-device requires a parameter (a device name).");
			argi += 1;
			physical_device_name = argv[argi];
		} else if (arg == "--drawing-size") {
			if (argi + 2 >= argc) throw std::runtime_error("--drawing-size requires two parameters (width and height).");
			auto conv = [&](std::string const &what) {
				argi += 1;
				std::string val = argv[argi];
				for (size_t i = 0; i < val.size(); ++i) {
					if (val[i] < '0' || val[i] > '9') {
						throw std::runtime_error("--drawing-size " + what + " should match [0-9]+, got '" + val + "'.");
					}
				}
				return std::stoul(val);
			};
			surface_extent.width = conv("width");
			surface_extent.height = conv("height");
		} else if(arg == "--scene"){
			if (argi + 1 >= argc) throw std::runtime_error("--scene requires a scene file.");
			argi += 1;
			scene_file = argv[argi];
		} else if(arg == "--camera"){
			if (argi + 1 >= argc) throw std::runtime_error("--camera requires a name.");
			argi += 1;
			main_camera = argv[argi];
		} else if(arg == "--culling"){
			if (argi + 1 >= argc) throw std::runtime_error("--culling requires a mode.");
			argi += 1;
			cull_mode = argv[argi];
		} else {
			throw std::runtime_error("Unrecognized argument '" + arg + "'.");
		}
	}
}

void RTG::Configuration::usage(std::function< void(const char *, const char *) > const &callback) {
	callback("--debug, --no-debug", "Turn on/off debug and validation layers.");
	callback("--physical-device <name>", "Run on the named physical device (guesses, otherwise).");
	callback("--drawing-size <w> <h>", "Set the size of the surface to draw to.");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *data,
	void *user_data
) {
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		std::cerr << "\x1b[91m" << "E: ";
	} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << "\x1b[33m" << "w: ";
	} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		std::cerr << "\x1b[90m" << "i: ";
	} else { //VERBOSE
		std::cerr << "\x1b[90m" << "v: ";
	}
	std::cerr << data->pMessage << "\x1b[0m" << std::endl;

	return VK_FALSE;
}

RTG::RTG(Configuration const &configuration_) : helpers(*this) {

	//copy input configuration:
	configuration = configuration_;

	//fill in flags/extensions/layers information:

	{//create the `instance` (main handle to Vulkan library):
		VkInstanceCreateFlags instance_flags = 0;
		std::vector< const char * > instance_extensions;
		std::vector< const char * > instance_layers;

		//add extensions for MoltenVK portability layer on macOS
		#if defined(__APPLE__)
		instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

		instance_extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
		#endif

		//add extensions and layers for debugging:
		if (configuration.debug) {
			instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		{ //add extensions needed by glfw:
			glfwInit();
			if (!glfwVulkanSupported()) {
				throw std::runtime_error("GLFW reports Vulkan is not supported.");
			}

			uint32_t count;
			const char **extensions = glfwGetRequiredInstanceExtensions(&count);
			if (extensions == nullptr) {
				throw std::runtime_error("GLFW failed to return a list of requested instance extensions. Perhaps it was not compiled with Vulkan support.");
			}
			for (uint32_t i = 0; i < count; ++i) {
				instance_extensions.emplace_back(extensions[i]);
			}
		}

		//debug messenger structure:
		VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = debug_callback,
			.pUserData = nullptr
		};

		VkInstanceCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = (configuration.debug ? &debug_messenger_create_info : nullptr),
			.flags = instance_flags,
			.pApplicationInfo = &configuration.application_info,
			.enabledLayerCount = uint32_t(instance_layers.size()),
			.ppEnabledLayerNames = instance_layers.data(),
			.enabledExtensionCount = uint32_t(instance_extensions.size()),
			.ppEnabledExtensionNames = instance_extensions.data()
		};
		VK( vkCreateInstance(&create_info, nullptr, &instance) );

		//create debug messenger
		if (configuration.debug) {
			PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (!vkCreateDebugUtilsMessengerEXT) {
				throw std::runtime_error("Failed to lookup debug utils create fn.");
			}
			VK( vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr, &debug_messenger) );
		}
	}

	{ //create the `window` and `surface` (where things get drawn):
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(configuration.surface_extent.width, configuration.surface_extent.height, configuration.application_info.pApplicationName, nullptr, nullptr);

		if (!window) {
			throw std::runtime_error("GLFW failed to create a window.");
		}

		VK( glfwCreateWindowSurface(instance, window, nullptr, &surface) );
	}

	{ //select the `physical_device` -- the gpu that will be used to draw:
		std::vector< std::string > physical_device_names; //for later error message
		{ //pick a physical device
			uint32_t count = 0;
			VK( vkEnumeratePhysicalDevices(instance, &count, nullptr) );
			std::vector< VkPhysicalDevice > physical_devices(count);
			VK( vkEnumeratePhysicalDevices(instance, &count, physical_devices.data()) );

			uint32_t best_score = 0;

			for (auto const &pd : physical_devices) {
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(pd, &properties);

				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(pd, &features);

				physical_device_names.emplace_back(properties.deviceName);

				if (!configuration.physical_device_name.empty()) {
					if (configuration.physical_device_name == properties.deviceName) {
						if (physical_device) {
							std::cerr << "WARNING: have two physical devices with the name '" << properties.deviceName << "'; using the first to be enumerated." << std::endl;
						} else {
							physical_device = pd;
						}
					}
				} else {
					uint32_t score = 1;
					if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
						score += 0x8000;
					}

					if (score > best_score) {
						best_score = score;
						physical_device = pd;
					}
				}
			}
		}

		if (physical_device == VK_NULL_HANDLE) {
			std::cerr << "Physical devices:\n";
			for (std::string const &name : physical_device_names) {
				std::cerr << "    " << name << "\n";
			}
			std::cerr.flush();

			if (!configuration.physical_device_name.empty()) {
				throw std::runtime_error("No physical device with name '" + configuration.physical_device_name + "'.");
			} else {
				throw std::runtime_error("No suitable GPU found.");
			}
		}

		{ //report device name:
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physical_device, &properties);
			std::cout << "Selected physical device '" << properties.deviceName << "'." << std::endl;
		}
	}
	

	{ //select the `surface_format` and `present_mode` which control how colors are represented on the surface and how new images are supplied to the surface:
		std::vector< VkSurfaceFormatKHR > formats;
		std::vector< VkPresentModeKHR > present_modes;
		
		{
			uint32_t count = 0;
			VK( vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, nullptr) );
			formats.resize(count);
			VK( vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, formats.data()) );
		}

		{
			uint32_t count = 0;
			VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, nullptr) );
			present_modes.resize(count);
			VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, present_modes.data()) );
		}

		//find first available surface format matching config:
		surface_format = [&](){
			for (auto const &config_format : configuration.surface_formats) {
				for (auto const &format : formats) {
					if (config_format.format == format.format && config_format.colorSpace == format.colorSpace) {
						return format;
					}
				}
			}
			throw std::runtime_error("No format matching requested format(s) found.");
		}();

		//find first available present mode matching config:
		present_mode = [&](){
			for (auto const &config_mode : configuration.present_modes) {
				for (auto const &mode : present_modes) {
					if (config_mode == mode) {
						return mode;
					}
				}
			}
			throw std::runtime_error("No present mode matching requested mode(s) found.");
		}();
	}

	{ //create the `device` (logical interface to the GPU) and the `queue`s to which we can submit commands:
		{ //look up queue indices:
			uint32_t count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
			std::vector< VkQueueFamilyProperties > queue_families(count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_families.data());

			for (auto const &queue_family : queue_families) {
				uint32_t i = uint32_t(&queue_family - &queue_families[0]);

				//if it does graphics, set the graphics queue family:
				if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					if (!graphics_queue_family) graphics_queue_family = i;
				}

				//if it has present support, set the present queue family:
				VkBool32 present_support = VK_FALSE;
				VK( vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support) );
				if (present_support == VK_TRUE) {
					if (!present_queue_family) present_queue_family = i;
				}
			}

			if (!graphics_queue_family) {
				throw std::runtime_error("No queue with graphics support.");
			}

			if (!present_queue_family) {
				throw std::runtime_error("No queue with present support.");
			}
		}

		//select device extensions:
		std::vector< const char * > device_extensions;
		#if defined(__APPLE__)
		device_extensions.emplace_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
		#endif
		//Add the swapchain extension:
		device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		{ //create the logical device:
			std::vector< VkDeviceQueueCreateInfo > queue_create_infos;
			std::set< uint32_t > unique_queue_families{
				graphics_queue_family.value(),
				present_queue_family.value()
			};

			float queue_priorities[1] = { 1.0f };
			for (uint32_t queue_family : unique_queue_families) {
				queue_create_infos.emplace_back(VkDeviceQueueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queue_family,
					.queueCount = 1,
					.pQueuePriorities = queue_priorities,
				});
			}

			VkDeviceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount = uint32_t(queue_create_infos.size()),
				.pQueueCreateInfos = queue_create_infos.data(),

				//device layers are depreciated; spec suggests passing instance_layers or nullptr:
				.enabledLayerCount = 0,
				.ppEnabledLayerNames = nullptr,

				.enabledExtensionCount = static_cast< uint32_t>(device_extensions.size()),
				.ppEnabledExtensionNames = device_extensions.data(),

				//pass a pointer to a VkPhysicalDeviceFeatures to request specific features: (e.g., thick lines)
				.pEnabledFeatures = nullptr,
			};

			VK( vkCreateDevice(physical_device, &create_info, nullptr, &device) );

			vkGetDeviceQueue(device, graphics_queue_family.value(), 0, &graphics_queue);
			vkGetDeviceQueue(device, present_queue_family.value(), 0, &present_queue);
		}
	}

	//create initial swapchain:
	recreate_swapchain();

	//create workspace resources:
	workspaces.resize(configuration.workspaces);
	//TODO: move create_info outside (creat_info_(fence/semaphore))
	for (auto &workspace : workspaces) {
		{ //create workspace fences:
			VkFenceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT, //start signaled, because all workspaces are available to start
			};

			VK( vkCreateFence(device, &create_info, nullptr, &workspace.workspace_available) );
		}

		{ //create workspace semaphores:
			VkSemaphoreCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			};

			VK( vkCreateSemaphore(device, &create_info, nullptr, &workspace.image_available) );
			VK( vkCreateSemaphore(device, &create_info, nullptr, &workspace.image_done) );
		}
	}

	{// create scene
		std::cout << "create scene from :" << configuration.scene_file << std::endl;
		if(configuration.scene_file != ""){ //load scene
			std::string scene_folder = "";
			auto pos = configuration.scene_file.find_last_of('\\');
			if(pos > 0 ){
				scene_folder = configuration.scene_file.substr(0 , pos+1);
			}
			sejp::value val = sejp::load(configuration.scene_file);
			std::cout << "loaded file!!!!" << std::endl;	
			auto header = val.as_array().value();
			if(header[0].as_string().value().compare(0, 6, "s72-v2") == 0){
				// auto s72 = header[1];
				for(int i = 1; i < header.size(); i++){
					auto object_json = header[i].as_object().value();
					std::string type = object_json.at("type").as_string().value();
					std::string name = object_json.at("name").as_string().value();
					std::cout << "Object is type : " << type << ", and name : " << name << std::endl;
					if(type.compare("SCENE") == 0){
						std::cout << "Loading scene" << std::endl;
						scene.name = name;
						auto roots = object_json.at("roots").as_array().value();
						std::cout << "Roots ";
						for(auto root : roots){
							scene.roots.push_back(root.as_string().value());
							std::cout << root.as_string().value() <<", ";
						}
						std::cout << std::endl;
					} else if(type.compare("NODE") == 0){
						std::cout << "Loading node" << std::endl;
						Node node;
						node.name = name;
						
						if(object_json.contains("translation")){
							auto translation = object_json.at("translation").as_array().value();
							if(translation.size() == 3){
								node.translation = {float(translation[0].as_number().value()),
														float(translation[1].as_number().value()), 
														float(translation[2].as_number().value()), 1.f};
							}
							// std::cout << "Translation " << node.translation[0] << " , " << node.translation[1] <<" , " << node.translation[2] << std::endl;
						}
						if(object_json.contains("rotation")){
							auto rotation = object_json.at("rotation").as_array().value();
							if(rotation.size() == 4){
								node.rotation = {float(rotation[0].as_number().value()),
												float(rotation[1].as_number().value()), 
												float(rotation[2].as_number().value()),
												float(rotation[3].as_number().value())};
							}
						}
						if(object_json.contains("scale")){
							auto scale = object_json.at("scale").as_array().value();
							if(scale.size() == 3){
								node.scale = {float(scale[0].as_number().value()),
												float(scale[1].as_number().value()), 
												float(scale[2].as_number().value()),1.f};
							}
						}
						if(object_json.contains("children")){
							for(auto children : object_json.at("children").as_array().value()){
								node.children.push_back(children.as_string().value());
							}
						}
						if(object_json.contains("camera")){
							auto camera = object_json.at("camera").as_string();
							if(camera.has_value()){
								node.camera = camera.value();
							}
						}
						if(object_json.contains("mesh")){
							auto mesh = object_json.at("mesh").as_string();
							if(mesh.has_value()){
								node.mesh = mesh.value();
							}
						}
						if(object_json.contains("environment")){
							auto environment = object_json.at("environment").as_string();
							if(environment.has_value()){
								node.environment = environment.value();
							}
						}
						if(object_json.contains("light")){
							auto light = object_json.at("light").as_string();
							if(light.has_value()){
								node.light = light.value();
							}
						}
						node.make_parent_from_local();
						nodes[name] = node;
					} else if(type.compare("MESH") == 0){
						std::cout << "Loading mesh" << std::endl;
						Mesh mesh;
						mesh.name = name;
						mesh.topology = object_json.at("topology").as_string().value();
						mesh.count =  uint32_t(object_json.at("count").as_number().value());
						mesh.indices = std::vector<uint32_t>(mesh.count);	
						
						if(object_json.contains("indices")){
							mesh.indices_info.used = true;
							auto indices_obj = object_json.at("indices").as_object().value();
							mesh.indices_info.src = indices_obj.at("src").as_string().value();
							mesh.indices_info.offset = uint32_t(indices_obj.at("offset").as_number().value());
							mesh.indices_info.format = indices_obj.at("format").as_string().value();

							//Pull the indices
							std::ifstream infile(mesh.indices_info.src.c_str() , std::ios::binary);
							//TODO indices;
							uint32_t num;
							for(uint32_t j = 0 ; j < mesh.count; j++ ){
								if(mesh.indices_info.format.compare("UINT32")){
									infile >> num;
									mesh.indices[j] = num;
								} else {
									mesh.indices[j] = j;
								}
							}
						} else {
							for(uint32_t j = 0; j < mesh.count; j++){
								mesh.indices[j] = j;
							}
						}

						//Get a long vector and then iterate over all of the floats with respect to each attribute
						//Huge while loop

						if(object_json.contains("attributes")){
							auto attributes = object_json.at("attributes").as_object().value();
							for (const auto& [key, value] : attributes) {
								std::cout << key <<" as attributes" << std::endl;
								Mesh::Attributes attribute;
								auto attr_obj = attributes.at(key).as_object().value();
								attribute.used = true;
								attribute.src = scene_folder + attr_obj.at("src").as_string().value();
								attribute.offset = uint32_t(attr_obj.at("offset").as_number().value());
								attribute.stride = uint32_t(attr_obj.at("stride").as_number().value());
								attribute.format = attr_obj.at("format").as_string().value();

								//Pull the indices

								std::ifstream infile(attribute.src.c_str() , std::ios::binary);
								if (!infile.is_open()) {
									std::cerr << "Error opening file!" << std::endl;
								}	

								std::vector<float> attr_vals;
								if(attribute.format.compare("R32G32B32_SFLOAT") == 0 || attribute.format.compare("R32G32_SFLOAT") == 0
								|| attribute.format.compare("R32G32B32A32_SFLOAT") == 0 || attribute.format.compare("R32_SFLOAT") == 0){
									// std::vector<float> floats;

									uint32_t size_attr = 0;
									//find how many
									if(attribute.format.compare("R32_SFLOAT") == 0){
										size_attr = 1;
									} else if(attribute.format.compare("R32G32_SFLOAT") == 0){
										size_attr = 2;
									} else if(attribute.format.compare("R32G32B32_SFLOAT") == 0){
										size_attr = 3;
									} else if (attribute.format.compare("R32G32B32A32_SFLOAT") == 0){
										size_attr = 4;
									}

									float num = 0.f;
									uint32_t p = attribute.offset;
									uint32_t step = attribute.stride;
									infile.seekg(p);
									uint32_t sub_iter = 0;
									while(infile.read(reinterpret_cast<char*>(&num), sizeof(float)) && p < step * mesh.count){
										attr_vals.push_back(num);
										// std::cout << num << " at " << p << "    ";
										sub_iter++;
										if(sub_iter < size_attr){
											infile.seekg(p + sizeof(float) * sub_iter);
										} else {
											sub_iter = 0;
											p += step;
											infile.seekg(p);
										}
									}


									// std::cout << std::endl;
									std::cout << "floats found from " << attribute.src.c_str() << " : "  << attr_vals.size() << std::endl;

									if (key.compare("POSITION") == 0){
										mesh.position_attr = attribute;
										mesh.position.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("NORMAL") == 0){
										mesh.normal_attr = attribute;
										mesh.normal.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("TANGENT") == 0){
										mesh.tangent_attr = attribute;
										mesh.tangent.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("TEXCOORD") == 0){
										mesh.texcoord_attr = attribute;
										mesh.texcoord.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("COLOR") == 0){
										std::cout << "No color attributes yet" << std::endl;
									}
								} else if (attribute.format.compare("R8G8B8A8_UNORM") == 0){
									
									uint32_t size_attr = 0;
									//find how many
									if(attribute.format.compare("R8_UNORM") == 0){
										size_attr = 1;
									} else if(attribute.format.compare("R8G8_UNORM") == 0){
										size_attr = 2;
									} else if(attribute.format.compare("R8G8B8_UNORM") == 0){
										size_attr = 3;
									} else if (attribute.format.compare("R8G8B8A8_UNORM") == 0){
										size_attr = 4;
									}
									
									
									uint8_t num;
									uint32_t p = attribute.offset;
									uint32_t step = attribute.stride;
									infile.seekg(p);
									uint32_t sub_iter = 0; 
									while(infile.read(reinterpret_cast<char*>(&num), sizeof(uint8_t))){
										attr_vals.push_back(float(num));	
										sub_iter++;
										if(sub_iter < size_attr){
											infile.seekg(p + sizeof(uint8_t) * sub_iter);
										} else {
											sub_iter = 0;
											p += step;	
											infile.seekg(p);
										}
									}

									if (key.compare("POSITION") == 0){
										mesh.position_attr = attribute;
										mesh.position.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("NORMAL") == 0){
										mesh.normal_attr = attribute;
										mesh.normal.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("TANGENT") == 0){
										mesh.tangent_attr = attribute;
										mesh.tangent.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("TEXCOORD") == 0){
										mesh.texcoord_attr = attribute;
										mesh.texcoord.assign(attr_vals.begin(), attr_vals.end());
									} else if(key.compare("COLOR") == 0){
										std::cout << "No color attributes yet" << std::endl;
									}
								} else {
									std::cout << "Attribute format not supported yet " << std::endl;
								}
								infile.close();
							}
						}
						if(object_json.contains("material")){
							mesh.material = object_json.at("material").as_string().value();
						}
						meshes[name] = mesh;
					} else if(type.compare("CAMERA") == 0){
						std::cout << "Loading camera" << std::endl;
						Camera camera;
						camera.name = name;
						if(object_json.contains("perspective")){
							auto perspective_obj = object_json.at("perspective").as_object().value();
							if(perspective_obj.contains("aspect")){
								camera.aspect = float(perspective_obj.at("aspect").as_number().value());
							} 
							if(perspective_obj.contains("vfov")){
								camera.vfov = float(perspective_obj.at("vfov").as_number().value());
							} 
							if(perspective_obj.contains("near")){
								camera.near = float(perspective_obj.at("near").as_number().value());
							} 
							if(perspective_obj.contains("far")){
								camera.far = float(perspective_obj.at("far").as_number().value());
							}
						}
						cameras[name] = camera;
						if(name.compare(configuration.main_camera) == 0 || active_camera.name.compare("") == 0){
							active_camera = camera;
							std::cout << "Active scene camera set as : " << camera.name <<std::endl;
						}

					} else if(type.compare("DRIVER") == 0){
						std::cout << "Loading driver" << std::endl;
						Driver driver;
						driver.name = name;
						driver.node = object_json.at("node").as_string().value();
						driver.channel = object_json.at("channel").as_string().value();
						auto times = object_json.at("times").as_array().value();
						for(auto time : times){
							driver.times.push_back(uint32_t(time.as_number().value()));
						}
						auto values = object_json.at("values").as_array().value();
						for(auto value : values){
							driver.values.push_back(uint32_t(value.as_number().value()));
						}
						if(object_json.contains("interpolation")){
							driver.interpolation = object_json.at("interpolation").as_string().value();
						}
						drivers[name] = driver;
					} else if(type.compare("MATERIAL") == 0){
						std::cout << "Loading material" << std::endl;
						Material material;
						material.name = name;
						if(object_json.contains("normalMap")){
							material.normalMap_src = object_json.at("normalMap").as_string().value();
						}
						if(object_json.contains("displacementMap")){
							material.displacementMap_src = object_json.at("displacementMap").as_string().value();
						}
						//Find the type of material
						
						if(object_json.contains("pbr")){
							material.material_type = "pbr";
							auto pbr_obj = object_json.at("pbr").as_object().value();
							if(auto albedo = pbr_obj.at("albedo").as_array()){
								auto albedo_array = albedo.value();
								material.albedo = {float(albedo_array[0].as_number().value()), 
									float(albedo_array[1].as_number().value()), float(albedo_array[2].as_number().value()), 0.f};
							} else if(auto albedo_src = pbr_obj.at("albedo").as_object()){
								material.albedo_src = scene_folder + albedo_src.value().at("src").as_string().value();
							}
						} else if (object_json.contains("lambertian")){
							material.material_type = "lambertian";
						} else if (object_json.contains("mirror")){
							material.material_type = "mirror";
						} else if (object_json.contains("environment")){
							material.material_type = "environment";
						} 
						materials[name] = material;
					} else if(type.compare("ENVIRONMENT") == 0){
						std::cout << "Loading environment" << std::endl;
						Environment environment;
						environment.name = name;
						auto radiance = object_json.at("radiance").as_object().value();
						environment.src = radiance.at("src").as_string().value();
						environment.type = radiance.at("type").as_string().value();
						environment.format = radiance.at("format").as_string().value();
						environments[name] = environment;
					} else if(type.compare("LIGHT") == 0){
						std::cout << "Loading light" << std::endl;
						Light light;
						light.name = name;
						if(object_json.contains("tint")){
							auto tint = object_json.at("tint").as_array().value();
							light.tint = {float(tint[0].as_number().value()), 
										float(tint[1].as_number().value()), 
										float(tint[2].as_number().value()), 0.f};
						}
						if(object_json.contains("shadow")){
							light.shadow = uint32_t(object_json.at("shadow").as_number().value());
						}
						if(object_json.contains("sun")){
							auto sun_obj = object_json.at("sun").as_object().value();
							light.light_type = "sun";
							light.angle = float(sun_obj.at("angle").as_number().value());
							light.strength = float(sun_obj.at("strength").as_number().value());
						} else if(object_json.contains("sphere")){
							auto sphere_obj = object_json.at("sphere").as_object().value();
							light.light_type = "sphere";
							light.angle = float(sphere_obj.at("angle").as_number().value());
							light.strength = float(sphere_obj.at("strength").as_number().value());
							if(sphere_obj.contains("limit")){
								light.limit = float(sphere_obj.at("limit").as_number().value());
							}
						} else if(object_json.contains("spot")){
							auto spot_obj = object_json.at("spot").as_object().value();
							light.light_type = "spot";
							light.angle = float(spot_obj.at("angle").as_number().value());
							light.strength = float(spot_obj.at("strength").as_number().value());
							if(spot_obj.contains("limit")){
								light.limit = float(spot_obj.at("limit").as_number().value());
							}
							light.fov = float(spot_obj.at("fov").as_number().value());
							light.blend = float(spot_obj.at("blend").as_number().value());
						}
						lights[name] = light;
					} else {
						std::cout << "no matching object type: " << type << std::endl;
					}
				}
			} else {
				std::cout << "no matching header: " << header[0].as_string().value() << std::endl;
			}
		} else {
			throw std::runtime_error("no scene was defined");
		}
	}

	{//set culling mode
		if(!configuration.cull_mode.empty()){
			cull_mode = configuration.cull_mode;
			std::cout << "Culling mode set to " << cull_mode << std::endl;
		}
	}

	{//Create user and debug cameras
		{
			user_camera.target = vec3{0.f, 0.f, 0.f};
			user_camera.radius = 2.f;
			user_camera.azimuth = 0.3f;
			user_camera.elevation = 0.3f;
		}
	}

	//run any resource creation required by Helpers structure:
	helpers.create();
}

void RTG::Node::make_parent_from_local(){
	mat4 trans_mat{ 1.f, 0.f, 0.f, 0.f, 
					0.f, 1.f, 0.f, 0.f,
					0.f, 0.f, 1.f, 0.f,
					translation[0], translation[0], translation[0], 1.f};
	mat4 rot_mat = quaternianToMatrix(rotation);
	mat4 scale_mat{ scale[0], 0.f, 0.f, 0.f,  
					0.f, scale[1], 0.f, 0.f, 
					0.f, 0.f, scale[2], 0.f,
					0.f, 0.f, 0.f, 1.f};
	parent_from_local = trans_mat * rot_mat * scale_mat;
}
RTG::~RTG() {
	//don't destroy until device is idle:
	if (device != VK_NULL_HANDLE) {
		if (VkResult result = vkDeviceWaitIdle(device); result != VK_SUCCESS) {
			std::cerr << "Failed to vkDeviceWaitIdle in RTG::~RTG [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
		}
	}

	//destroy any resource destruction required by Helpers structure:
	helpers.destroy();

	//destroy workspace resources:
	for (auto &workspace : workspaces) {
		if (workspace.workspace_available != VK_NULL_HANDLE) {
			vkDestroyFence(device, workspace.workspace_available, nullptr);
			workspace.workspace_available = VK_NULL_HANDLE;
		}
		if (workspace.image_available != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, workspace.image_available, nullptr);
			workspace.image_available = VK_NULL_HANDLE;
		}
		if (workspace.image_done != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, workspace.image_done, nullptr);
			workspace.image_done = VK_NULL_HANDLE;
		}
	}
	workspaces.clear();

	//destroy the swapchain:
	destroy_swapchain();

	//destroy the rest of the resources:
	if (device != VK_NULL_HANDLE) {
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if (surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (window != nullptr) {
		glfwDestroyWindow(window);
		window = nullptr;
	}

	if (debug_messenger != VK_NULL_HANDLE) {
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (vkDestroyDebugUtilsMessengerEXT) {
			vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			debug_messenger = VK_NULL_HANDLE;
		}
	}

	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}


void RTG::recreate_swapchain() {
	//clean up swapchain if it already exists:
	if (!swapchain_images.empty()) {
		destroy_swapchain();
	}

	//determine size, image count, and transform for swapchain:
	VkSurfaceCapabilitiesKHR capabilities;
	VK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities) );

	swapchain_extent = capabilities.currentExtent;

	uint32_t requested_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0) {
		requested_count = std::min(capabilities.maxImageCount, requested_count);
	}

	{ //create swapchain
		VkSwapchainCreateInfoKHR create_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = requested_count,
			.imageFormat = surface_format.format,
			.imageColorSpace = surface_format.colorSpace,
			.imageExtent = swapchain_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = present_mode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE //NOTE: could be more efficient by passing old swapchain handle here instead of destroying it
		};

		std::vector< uint32_t > queue_family_indices{
			graphics_queue_family.value(),
			present_queue_family.value()
		};

		if (queue_family_indices[0] != queue_family_indices[1]) {
			//if images will be presented on a different queue, make sure they are shared:
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = uint32_t(queue_family_indices.size());
			create_info.pQueueFamilyIndices = queue_family_indices.data();
		} else {
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		VK( vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) );
	}

	{ //get the swapchain images:
		uint32_t count = 0;
		VK( vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr) );
		swapchain_images.resize(count);
		VK( vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data()) );
	}

	//create views for swapchain images:
	swapchain_image_views.assign(swapchain_images.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain_images.size(); ++i) {
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surface_format.format,
			.components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};
		VK( vkCreateImageView(device, &create_info, nullptr, &swapchain_image_views[i]) );
	}

	if (configuration.debug) {
		std::cout << "Swapchain is now " << swapchain_images.size() << " images of size " << swapchain_extent.width << "x" << swapchain_extent.height << "." << std::endl;
		//TODO: print out information about the min and max image counts supported by the surface, the current present mode, or the current transformation of the surface.
	}
}


void RTG::destroy_swapchain() {
	VK( vkDeviceWaitIdle(device) ); //wait for any rendering to old swapchain to finish

	//clean up image views referencing the swapchain:
	for (auto &image_view : swapchain_image_views) {
		vkDestroyImageView(device, image_view, nullptr);
		image_view = VK_NULL_HANDLE;
	}
	swapchain_image_views.clear();

	//forget handles to swapchain images (will destroy by deallocating the swapchain itself):
	swapchain_images.clear();

	//deallocate the swapchain and (thus) its images:
	if (swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	event.type = InputEvent::MouseMotion;
	event.motion.x = float(xpos);
	event.motion.y = float(ypos);
	event.motion.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b) {
		if (glfwGetMouseButton(window, b) == GLFW_PRESS) {
			event.motion.state |= (1 << b);
		}
	}

	event_queue->emplace_back(event);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	if (action == GLFW_PRESS) {
		event.type = InputEvent::MouseButtonDown;
	} else if (action == GLFW_RELEASE) {
		event.type = InputEvent::MouseButtonUp;
	} else {
		std::cerr << "Strange: unknown mouse button action." << std::endl;
		return;
	}

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	event.button.x = float(xpos);
	event.button.y = float(ypos);
	event.button.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b) {
		if (glfwGetMouseButton(window, b) == GLFW_PRESS) {
			event.button.state |= (1 << b);
		}
	}
	event.button.button = uint8_t(button);
	event.button.mods = uint8_t(mods);

	event_queue->emplace_back(event);
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	event.type = InputEvent::MouseWheel;
	event.wheel.x = float(xoffset);
	event.wheel.y = float(yoffset);

	event_queue->emplace_back(event);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	if (action == GLFW_PRESS) {
		event.type = InputEvent::KeyDown;
	} else if (action == GLFW_RELEASE) {
		event.type = InputEvent::KeyUp;
	} else if (action == GLFW_REPEAT) {
		//ignore repeats
		return;
	} else {
		std::cerr << "Strange: got unknown keyboard action." << std::endl;
	}

	event.key.key = key;
	event.key.mods = mods;

	event_queue->emplace_back(event);
}

void RTG::run(Application &application) {
	//initial on_swapchain:
	auto on_swapchain = [&,this]() {
		application.on_swapchain(*this, SwapchainEvent{
			.extent = swapchain_extent,
			.images = swapchain_images,
			.image_views = swapchain_image_views,
		});
	};
	on_swapchain();

	//setup event handling:
	std::vector< InputEvent > event_queue;
	glfwSetWindowUserPointer(window, &event_queue);

	glfwSetCursorPosCallback(window, cursor_pos_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);

	//setup time handling:
	std::chrono::high_resolution_clock::time_point before = std::chrono::high_resolution_clock::now();

	while (!glfwWindowShouldClose(window)) {
		//event handling:
		glfwPollEvents();

		//deliver all input events to application:
		for (InputEvent const &input : event_queue) {
			application.on_input(input);
			//TODO: headlesss PLAY
			// if(headless){
			// 	Event event;
			// 	event.type = PLAY;
			// 	event.ts = before;
			// 	events.push_back(event);
			// }
		}
		event_queue.clear();

		{ //elapsed time handling:
			std::chrono::high_resolution_clock::time_point after = std::chrono::high_resolution_clock::now();
			float dt = float(std::chrono::duration< double >(after - before).count());
			before = after;

			dt = std::min(dt, 0.1f); //lag if frame rate dips too low

			application.update(dt);
			//TODO: headless AVAILABLE
		}

		uint32_t workspace_index;
		{ //acquire a workspace:
			assert(next_workspace < workspaces.size());
			workspace_index = next_workspace;
			next_workspace = (next_workspace + 1) % workspaces.size();

			//wait until the workspace is not being used:
			VK( vkWaitForFences(device, 1, &workspaces[workspace_index].workspace_available, VK_TRUE, UINT64_MAX) );

			//mark the workspace as in use:
			VK( vkResetFences(device, 1, &workspaces[workspace_index].workspace_available) );
		}

		uint32_t image_index = -1U;
		//acquire an image (resize swaphain if needed):
retry:
		//Ask the swapchain for the next image index -- note careful return handling:
		if (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, workspaces[workspace_index].image_available, VK_NULL_HANDLE, &image_index);
		    result == VK_ERROR_OUT_OF_DATE_KHR) {
			//if the swapchain is out-of-date, recreate it and run the loop again:
			std::cerr << "Recreating swapchain because vkAcquireNextImageKHR returned " << string_VkResult(result) << "." << std::endl;
			
			recreate_swapchain();
			on_swapchain();

			goto retry;
		} else if (result == VK_SUBOPTIMAL_KHR) {
			//if the swapchain is suboptimal, render to it and recreate it later:
			std::cerr << "Suboptimal swapchain format -- ignoring for the moment." << std::endl;
		} else if (result != VK_SUCCESS) {
			//other non-success results are genuine errors:
			throw std::runtime_error("Failed to acquire swapchain image (" + std::string(string_VkResult(result)) + ")!");
		}

		//call render function:
		application.render(*this, RenderParams{
			.workspace_index = workspace_index,
			.image_index = image_index,
			.image_available = workspaces[workspace_index].image_available,
			.image_done = workspaces[workspace_index].image_done,
			.workspace_available = workspaces[workspace_index].workspace_available,
		});

		{ //queue the work for presentation:
			VkPresentInfoKHR present_info{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &workspaces[workspace_index].image_done,
				.swapchainCount = 1,
				.pSwapchains = &swapchain,
				.pImageIndices = &image_index,
			};

			assert(present_queue);

			//note, again, the careful return handling:
			if (VkResult result = vkQueuePresentKHR(present_queue, &present_info);
			    result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				std::cerr << "Recreating swapchain because vkQueuePresentKHR returned " << string_VkResult(result) << "." << std::endl;
				recreate_swapchain();
				on_swapchain();
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to queue presentation of image (" + std::string(string_VkResult(result)) + ")!");
			}
		}

		//TODO: headless SAVE
		//TODO: present image (resize swapchain if needed)
	}

	//tear down event handling:
	glfwSetMouseButtonCallback(window, nullptr);
	glfwSetCursorPosCallback(window, nullptr);
	glfwSetScrollCallback(window, nullptr);
	glfwSetKeyCallback(window, nullptr);

	glfwSetWindowUserPointer(window, nullptr);

}
