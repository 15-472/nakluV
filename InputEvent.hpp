#pragma once

//Structure used for reporting input events from RTG -> Application.

// *********************************************************
// *                                                       *
// * WARNING:                                              *
// *                                                       *
// *    Editing this structure will break refsol::RTG_run  *
// *                                                       *
// *********************************************************

#include <cstdint>

union InputEvent {
	enum Type : uint32_t {
		MouseMotion,
		MouseButtonDown,
		MouseButtonUp,
		MouseWheel,
		KeyDown,
		KeyUp
	} type;
	struct MouseMotion {
		Type type;
		float x, y; //in (possibly fractional) swapchain pixels from upper left of image
		uint8_t state; //all mouse button states, bitfield of (1 << GLFW_MOUSE_BUTTON_*)
	} motion;
	struct MouseButton {
		Type type;
		float x, y; //in (possibly fractional) swapchain pixels from upper left of image
		uint8_t state; //all mouse button states, bitfield of (1 << GLFW_MOUSE_BUTTON_*)
		uint8_t button; //one of the GLFW_MOUSE_BUTTON_* values
		uint8_t mods; //bitfield of modifier keys (GLFW_MOD_* values)
	} button;
	struct MouseWheel {
		Type type;
		float x, y; //scroll offset; +x right, +y up(?); from glfw scroll callback
	} wheel;
	struct KeyInput {
		Type type;
		int key; //GLFW_KEY_* codes
		int mods; //GLFW_MOD_* bits
	} key;
};

