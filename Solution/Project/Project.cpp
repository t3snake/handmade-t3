#include <windows.h>
#include<stdint.h>
#include<Xinput.h>

#define global_variable static

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

global_variable bool running;
global_variable bool vibrating;

struct bitmap_state {
	BITMAPINFO info;
	void* memory;
	int byte_per_pixel = 4;
	int width;
	int height;
};

global_variable bitmap_state bitmap_buffer = {};

// function pointers for xinput

#define InputStateGetMacro(name) DWORD name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef InputStateGetMacro(fp_x_input_get_state); 
InputStateGetMacro(stub_x_input_get_fn) {
	return ERROR_SUCCESS;
}

global_variable fp_x_input_get_state* XInputGetStatePtr = stub_x_input_get_fn; // stub initially until xinput library is loaded

#define InputStateSetMacro(name) DWORD name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef InputStateSetMacro(fp_x_input_set_state);
InputStateSetMacro(stub_x_input_set_fn) {
	return ERROR_SUCCESS;
}

global_variable fp_x_input_set_state* XInputSetStatePtr = stub_x_input_set_fn; // stub initially until xinput library is loaded


// Painting functions

static void animateCheckerPattern(int x_offset, int y_offset) {
	u8* row = (u8*)bitmap_buffer.memory;
	for (int y = 0; y < bitmap_buffer.height; y++) {
		u32* pixel =(u32*) row;
		for (int x = 0; x < bitmap_buffer.width; x++) {
			u8 green_offset = x + x_offset;
			u8 blue_offset = y + y_offset;
			*(pixel++) = (green_offset << 8) | (blue_offset);
		}
		row = row + bitmap_buffer.width * bitmap_buffer.byte_per_pixel;
	}
}

// DIB - Device Independant Bitmap
static void Win32ResizeDIBSection(int width, int height) {
	bitmap_buffer.height = height;
	bitmap_buffer.width = width;

	// initialize bitmap_info
	bitmap_buffer.info = {};
	bitmap_buffer.info.bmiHeader.biSize = sizeof(bitmap_buffer.info.bmiHeader);
	bitmap_buffer.info.bmiHeader.biWidth = width;
	bitmap_buffer.info.bmiHeader.biHeight = -height; // negative value: top down pitch
	bitmap_buffer.info.bmiHeader.biPlanes = 1;
	bitmap_buffer.info.bmiHeader.biCompression = BI_RGB;
	bitmap_buffer.info.bmiHeader.biBitCount = 32;

	// Free old memory
	if (bitmap_buffer.memory) {
		VirtualFree(bitmap_buffer.memory, 0, MEM_RELEASE); // no need to pass size for release, Os knows what to free from original VirtualAlloc call
	}

	// Allocate memory
	int bitmap_memory_size = width * height * bitmap_buffer.byte_per_pixel;
	bitmap_buffer.memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	
}

static void Win32DisplayBufferToWindow(HDC device_context, RECT *window_rect) {
	int window_width = window_rect->right - window_rect->left;
	int window_height = window_rect->bottom - window_rect->top;

	StretchDIBits(
		device_context,
		0, 0, window_width, window_height,
		0, 0, bitmap_buffer.width, bitmap_buffer.height,
		bitmap_buffer.memory,
		&bitmap_buffer.info,
		DIB_RGB_COLORS, // DIB_PAL_COLORS if we want to use fixed amount of colors
		SRCCOPY
	);
}

// XInput APIs
static void Win32LoadXInputLibrary() {
	HMODULE xinput_lib_handle;
	xinput_lib_handle = LoadLibraryA("Xinput1_4.dll");

	if (!xinput_lib_handle) {
		xinput_lib_handle = LoadLibraryA("Xinput9_1_0.dll");
	}

	if (!xinput_lib_handle) {
		return;
	}

	XInputGetStatePtr = (fp_x_input_get_state *) GetProcAddress(xinput_lib_handle, "XInputGetState");

	XInputSetStatePtr = (fp_x_input_set_state*) GetProcAddress(xinput_lib_handle, "XInputSetState");
	
}

// temp param that modifies 
static void QueryXInput(u32 *x_offset, u32 *y_offset) {
	for (DWORD controller_index = 0; controller_index < XUSER_MAX_COUNT; controller_index++) {
		XINPUT_STATE state = {};

		if (XInputGetStatePtr(controller_index, &state) == ERROR_SUCCESS) {
			// controller connected

			// see if button bits are masked through &
			bool isUpPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
			bool isDownPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			bool isLeftPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			bool isRightPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
			bool isStartPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
			bool isBackPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
			bool isL3Pressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
			bool isR3Pressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
			bool isAPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
			bool isBPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
			bool isXPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
			bool isYPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
			bool isLBPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			bool isRBPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

			short lstick_x_axis = state.Gamepad.sThumbLX;
			short lstick_y_axis = state.Gamepad.sThumbLY;
			short rstick_x_axis = state.Gamepad.sThumbRX;
			short rstick_y_axis = state.Gamepad.sThumbRY;

			byte lt_state = state.Gamepad.bLeftTrigger;
			byte rt_state = state.Gamepad.bRightTrigger;


			XINPUT_VIBRATION vibration = {};
			if (isAPressed && !vibrating) { // dont set vibration if already vibrating
				vibrating = 1;
				vibration.wLeftMotorSpeed = (1 << 15);
				vibration.wRightMotorSpeed = (1 << 15);
				XInputSetStatePtr(controller_index, &vibration);
			}
			else if (vibrating) {
				vibrating = 0;
				XInputSetStatePtr(controller_index, &vibration); // set vibration to 0
			}

			if (lstick_x_axis < -(1 << 14)) {
				(*x_offset)++;
			}
			if (lstick_x_axis > (1 << 14)) {
				(*x_offset)--;
			}
			if (lstick_y_axis > (1 << 14)) {
				(*y_offset)++;
			}
			if (lstick_y_axis < -(1 << 14)) {
				(*y_offset)--;
			}
			break;
		}
		else {
			// controller not connected
		}
	}
}

LRESULT CALLBACK Win32MainCallback(
	HWND win_handle,
	UINT message,
	WPARAM w_param,
	LPARAM l_param
) {

	LRESULT result = 0;
	switch (message) {
	case WM_CLOSE: {
		PostQuitMessage(0);
		running = 0;
	} break;
	case WM_SIZE: {
		RECT client_area;
		GetClientRect(win_handle, &client_area);

		int width = client_area.right - client_area.left;
		int height = client_area.bottom - client_area.top;
		Win32ResizeDIBSection(width, height);
	} break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC device_context = BeginPaint(win_handle, &paint);
		
		RECT client_area;
		GetClientRect(win_handle, &client_area);

		Win32DisplayBufferToWindow(device_context, &client_area);

		EndPaint(win_handle, &paint);
	} break;
	case WM_DESTROY: {
		PostQuitMessage(0);
		running = 0;
	} break;
	case WM_ACTIVATEAPP: {

	} break;
	// case handling keyboard events
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		// https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
		bool isAltPressed = (l_param & (1 << 29)) != 0; // 29th bit of lparam is context code which tells if alt was down or not 
		int vk_code = w_param;
		
		switch (vk_code) {
		case VK_UP: {

		} break;
		case VK_DOWN: {

		} break;
		case VK_LEFT: {

		} break;
		case VK_RIGHT: {

		} break;
		case VK_RETURN: { // Enter

		} break;
		case 'W': { // TODO fallthrough for VK_UP?

		} break;
		case 'A': {

		} break;
		case 'S': {

		} break;
		case 'D': {

		} break;
		case 'Q': {

		} break;
		case 'E': {

		} break;
		case VK_F4: {
			if (isAltPressed) {
				result = DefWindowProcA(win_handle, message, w_param, l_param); // handle alt f4
			}
		} break;
		}
	} break;


	default: {
		result = DefWindowProcA(win_handle, message, w_param, l_param);
	}
	}
	return result;
}


int CALLBACK WinMain(
	HINSTANCE instance,
	HINSTANCE prev_instance,
	LPSTR command_line,
	int nCmdSHow
) {
	WNDCLASSA window_class = {};
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = Win32MainCallback;
	window_class.hInstance = instance;
	//window_class.hIcon = ;
	window_class.lpszClassName = "HandmadeT3WindowClass";

	running = 1;

	if (RegisterClassA(&window_class)) {

		Win32LoadXInputLibrary();

		HWND window_handle =
			CreateWindowExA(
				0,
				window_class.lpszClassName,
				"Handmade T3",
				WS_OVERLAPPEDWINDOW | WS_VISIBLE,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				instance,
				0
			);

		u32 x_offset = 0;
		u32 y_offset = 0;

		while (running) {
			MSG message;
			// GetMessageA blocks the thread so use PeekMessageA
			while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
				TranslateMessage(&message);
				DispatchMessageA(&message);
			}

			// Gamepad handling
			QueryXInput(&x_offset, &y_offset);

			// Drawing to screen > create buffer and blit it to screen
			RECT client_area;
			GetClientRect(window_handle, &client_area);
			HDC device_context = GetDC(window_handle);

			animateCheckerPattern(x_offset, y_offset);
			Win32DisplayBufferToWindow(device_context, &client_area);

			ReleaseDC(window_handle, device_context);
		}
	}
}