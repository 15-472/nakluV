#pragma once

#include "Helpers.hpp"
#include "InputEvent.hpp"

#include <vulkan/vulkan_core.h>

#include "mat4.hpp"

#include <array>
#include <optional>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <map>

struct GLFWwindow;

/*
 * Real-time Graphics support framework.
 * Takes care of creating a Vulkan device and running a main loop.
 *
 *  RTG::Configuration configuration;
 *  configuration.application_info = VkApplicationInfo{ ... }; //set properly for your app
 *  config.parse(argc, argv); //manage command line arguments
 *
 *  RTG rtg(configuration);
 *  MyApplication app(rtg); //note: derives from RTG::Application
 *
 *  rtg.run(app);
 *
 */

struct RTG {
	//----------------------------------------
	// Creating an RTG wrapper:

	struct Configuration;

	RTG(Configuration const &); //creates/acquires resources
	~RTG(); //destroys/deallocates resources
	RTG(RTG const &) = delete; //don't copy this structure!

	//Configuration passed to RTG constructor:
	struct Configuration {
		//application info passed to Vulkan:
		VkApplicationInfo application_info{
			.pApplicationName = "Unknown",
			.applicationVersion = VK_MAKE_VERSION(0,0,0),
			.pEngineName = "Unknown",
			.engineVersion = VK_MAKE_VERSION(0,0,0),
			.apiVersion = VK_API_VERSION_1_3
		};

		//if true, add debug and validation layers and print more debug output:
		//  `--debug` and `--no-debug` command-line flags
		bool debug = true;

		//if set, use a specific device for rendering:
		// `--physical-device <name>` command-line flag
		std::string physical_device_name = "";

		//requested (priority-ranked) formats for output surface: (will use first available)
		std::vector< VkSurfaceFormatKHR > surface_formats{
			VkSurfaceFormatKHR{ .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
		};
		//requested (priority-ranked) presentation modes for output surface: (will use first available)
		std::vector< VkPresentModeKHR > present_modes{
			VK_PRESENT_MODE_FIFO_KHR
		};

		//requested size of the output surface:
		// `--drawing-size <w> <h>` command-line flag
		VkExtent2D surface_extent{ .width = 800, .height=540 };

		//how many "workspaces" (frames that can currently be being worked on by the CPU or GPU) to use:
		uint32_t workspaces = 2;

		//Where to find the scene
		std::string scene_file = "";

		//The main camera
		std::string main_camera = "";

		std::string cull_mode = "";

		//for configuration construction + management:
		Configuration() = default;
		void parse(int argc, char **argv); //parse command-line options; throws on error
		static void usage(std::function< void(const char *, const char *) > const &callback); //reports command line usage by passing flag and description to callback.
	};

	Configuration configuration; //configuration, as used (might have extra extensions, layers, or flags added)

	//------------------------------------------------
	//Helper functions, split off into their own little package:
	// see Helpers.hpp
	Helpers helpers;

	//------------------------------------------------
	//Basic vulkan handles:

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	//queue for graphics and transfer operations:
	std::optional< uint32_t > graphics_queue_family;
	VkQueue graphics_queue = VK_NULL_HANDLE;

	//queue for present operations:
	std::optional< uint32_t > present_queue_family;
	VkQueue present_queue = VK_NULL_HANDLE;

	//-------------------------------------------------
	//Handles for the window and surface:

	GLFWwindow *window = nullptr;

	//The surface is where rendered images are shown:
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSurfaceFormatKHR surface_format{};
	VkPresentModeKHR present_mode{};

	//-------------------------------------------------
	//Stuff used by 'run' to run the main loop (swapchain and workspaces):

	//The swapchain is the list of images that get rendered to and shown on the surface:
	VkSwapchainKHR swapchain = VK_NULL_HANDLE; //in non-headless mode, swapchain images are managed by this object; in headless mode this will be null

	VkExtent2D swapchain_extent = {.width = 0, .height = 0}; //current size of the swapchain
	std::vector< VkImage > swapchain_images; //images in the swapchain
	std::vector< VkImageView > swapchain_image_views; //image views of the images in the swapchain

	//swapchain management: (used from RTG::RTG(), RTG::~RTG(), and RTG::run() [on resize])
	void recreate_swapchain();
	void destroy_swapchain(); //NOTE: swapchain must exist
	
	//Workspaces hold dynamic state that must be kept separate between frames.
	// RTG stores some synchronization primitives per workspace.
	// (The bulk of per-workspace data will be managed by the Application.)
	struct PerWorkspace {
		VkFence workspace_available = VK_NULL_HANDLE; //workspace is ready for a new render
		VkSemaphore image_available = VK_NULL_HANDLE; //the image is ready to write to
		VkSemaphore image_done = VK_NULL_HANDLE; //the image is done being written to
	};
	std::vector< PerWorkspace > workspaces;
	//^^ this size could probably be hardcoded (it will almost always be 2 unless you want bottlenecks!), but I'm leaving it variable at the moment.
	uint32_t next_workspace = 0;

	//------------------------------
	//Main loop stuff:

	struct Application;

	//run an application (calls 'update', 'resize', 'handle_event', and 'render' functions on application):
	void run(Application &);

	struct SwapchainEvent;
	struct RenderParams;

	//inherit from application to make something to pass to run:
	struct Application {
		//handle user input: (called when user interacts)
		virtual void on_input(InputEvent const &) = 0;

		//[re]create resources when swapchain is recreated: (called at start of run() and when window is resized)
		virtual void on_swapchain(RTG &, SwapchainEvent const &) = 0;

		//advance time for dt seconds: (called every frame)
		virtual void update(float dt) = 0;

		//queue commands to render a frame: (called every frame)
		virtual void render(RTG &, RenderParams const &) = 0;
	};

	//------------------------------
	//Structure definitions:

	//event structure (well, union) used to pass events from RTG -> App:
	// See InputEvent.hpp for `union InputEvent`

	//parameters passed to Application::on_swapchain() when swapchain is [re]created:
	// (these can also be accessed on the rtg directly but the package puts them in a convenient spot)
	struct SwapchainEvent {
		VkExtent2D const &extent; //swapchain extent
		std::vector< VkImage > const &images; //swapchain images
		std::vector< VkImageView > const &image_views; //swapchain image views
	};

	//parameters passed to Application::draw() when asking application to render a frame:
	struct RenderParams {
		uint32_t workspace_index; //which per-render workspace to use (e.g., you probably want a command buffer per workspace)
		uint32_t image_index; //which swapchain image to render into
		VkSemaphore image_available = VK_NULL_HANDLE; //nothing should use the swapchain image until this is signal'd
		VkSemaphore image_done = VK_NULL_HANDLE; //this should be signal'd when the image is done being written to
		VkFence workspace_available = VK_NULL_HANDLE; //this should be signal'd when *all* work is done for the frame
	};

	//-----------------------
	//Object parameters

	struct Scene {
		std::string name;
		std::vector<std::string> roots;
	};

	struct Node{
		std::string name;
		vec4 translation = {0.0f, 0.0f, 0.0f, 1.f};
		vec4 rotation = {0.f, 0.f, 0.f, 1.f};
		vec4 scale = {1.0f, 1.0f, 1.0f, 1.f};
		mat4 parent_from_local = mat4{1.f, 0.f, 0.f, 0.f, 
									  0.f, 1.f, 0.f, 0.f,
									  0.f, 0.f, 1.f, 0.f,
									  0.f, 0.f, 0.f, 1.f};
		void make_parent_from_local();
		// std::string type;
		std::vector<std::string> children;
		std::string camera;
		std::string mesh;
		std::string environment;
		std::string light;
	};

	struct Mesh{
		std::string name;
		std::string topology;
		uint32_t count;
		//Do we need to store this information
		struct Indices {
			bool used = false;
			std::string src;
			uint32_t offset;
			std::string format;
		}indices_info;
		std::vector<uint32_t> indices;

		struct Attributes{
			bool used = false;
			std::string src;
			uint32_t offset;
			uint32_t stride;
			std::string format;
		};
		Attributes position_attr;
		Attributes normal_attr;
		Attributes tangent_attr;
		Attributes texcoord_attr;
		std::vector<float> position;
		std::vector<float> normal;
		std::vector<float> tangent;
		std::vector<float> texcoord;
		std::string material;

		uint32_t vertices_start;
		uint32_t vertices_count;
	};

	struct Camera{
		std::string name;
		float aspect;
		float vfov;
		float near;
		float far;
	};

	struct OrbitCamera{
		vec3 target;
		float radius = 1.f;
		float azimuth;
		float elevation;
	};

	struct Driver {
		std::string name;
		std::string node;
		std::string channel;
		std::vector<uint32_t> times;
		std::vector<uint32_t> values;
		std::string interpolation = "LINEAR";
	};

	struct Material {
		std::string name;
		std::string normalMap_src;
		std::string displacementMap_src;
		std::string material_type = "lambertian";

		vec4 albedo = {0.8f, 0.8f, 0.8f, 0.f};
		std::string albedo_src;	
		float roughness = 1.f;
		std::string roughness_src;
		float metalness = 0.f;
		std::string metalness_src;

		uint32_t texture_number = 0;
	};

	struct Environment {
		std::string name;
		std::string src;
		std::string type;
		std::string format;
	};

	struct Light {
		std::string name;
		vec4 tint = {1.f, 1.f, 1.f, 0.f};
		uint32_t shadow = 0;
		std::string light_type;

		float angle;
		float strength;
		float radius;
		float power;
		float limit;
		float fov;
		float blend;

		// struct Sun{
		// 	float angle;
		// 	float strength;
		// } sun;
		// struct Sphere{
		// 	float radius;
		// 	float power;
		// 	float limit;
		// } sphere;
		// struct Spot{
		// 	float radius;
		// 	float power;
		// 	float fov;
		// 	float blend;
		// 	float limit;
		// } spot;
		
	};


	Scene scene;
	std::map<std::string, Node> nodes;
	std::map<std::string, Mesh> meshes;
	std::map<std::string, Camera> cameras;
	std::map<std::string, Driver> drivers;
	std::map<std::string, Material> materials;
	std::map<std::string, Environment> environments;
	std::map<std::string, Light> lights;

	std::string camera_mode = "scene";
	Camera active_camera;
	// Camera scene_camera;
	OrbitCamera user_camera;
	// Camera debug_camera;


	//------------------------------
	std::string cull_mode = "none";


	//----------------------------------
	//Headless event structure

	bool headless = false;

	struct Play{
		float t;
		float rate;
	};

	struct Event{
		enum{NONE, AVAILABLE, PLAY, SAVE, MARK} type;

		float ts;

		union{
			bool available;
			Play play;
			std::string saveFile;
			std::string markDescription;
		};

		Event() : type(NONE) , ts(0.0) {};

		~Event() {};

	};


	std::vector<Event> events;
};
