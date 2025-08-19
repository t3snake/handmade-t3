#include <windows.h>

#include<Xinput.h>
#include<mmdeviceapi.h>
#include<Audioclient.h>

#include<handmade.cpp>

#define REFTIMES_PER_SEC  10000000

global_variable bool running;
global_variable bool vibrating;

global_variable BITMAPINFO bitmap_info;

global_variable BitmapState bitmap_buffer = {};

struct AudioInterfaces {
	IAudioClient* audio_client;
	IAudioRenderClient* render_client;
	u32 buffer_size;
};

global_variable AudioInterfaces audio_intfs;

global_variable u32 buffer_write_ptr;

// Function pointers type definition for xinput funcs

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

// DIB - Device Independant Bitmap
static void Win32ResizeDIBSection(int width, int height) {
	bitmap_buffer.height = height;
	bitmap_buffer.width = width;

	// initialize bitmap_info
	bitmap_info = {};
	bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
	bitmap_info.bmiHeader.biWidth = width;
	bitmap_info.bmiHeader.biHeight = -height; // negative value: top down pitch
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biCompression = BI_RGB;
	bitmap_info.bmiHeader.biBitCount = 32;

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
		&bitmap_info,
		DIB_RGB_COLORS, // DIB_PAL_COLORS if we want to use fixed amount of colors
		SRCCOPY
	);
}

// XInput Handling

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

// temp param that modifies animation buffer
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

// WASAPI sound funtions

static void Win32InitWasapi() {
	// see https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
	HRESULT hr;

	IMMDeviceEnumerator* enumerator_ptr = 0;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**) &enumerator_ptr);

	IMMDevice *audio_device;
	hr = enumerator_ptr->GetDefaultAudioEndpoint(eRender, eConsole, &audio_device);
	if (hr != S_OK) {
		return;
	}

	/*hr = enumerator_ptr->Release();
	if (hr != S_OK) {
		return;
	}*/

	hr = audio_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**) &audio_intfs.audio_client);
	if (hr != S_OK) {
		return;
	}

	/*hr = audio_device->Release();
	if (hr != S_OK) {
		return;
	}*/

	WAVEFORMATEX wave_format = {};

	// TODO check wave format since doing it yourself doesnt seem to take any other samples/second other than 48000
	// Should probably just get the format from audio client
	// audio_client->GetMixFormat(&wave_format);

	wave_format.wFormatTag = WAVE_FORMAT_PCM;
	wave_format.nChannels = 2;
	wave_format.nSamplesPerSec = 48000;
	wave_format.wBitsPerSample = 16;
	wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
	wave_format.nAvgBytesPerSec = (wave_format.nSamplesPerSec * wave_format.nBlockAlign);

	

	hr = audio_intfs.audio_client->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		0,
		2*REFTIMES_PER_SEC,
		(AUDCLNT_STREAMFLAGS_RATEADJUST | 
		AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | 
		AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY ), // flags necessary to define own sample rate
		&wave_format,
		0
	);

	if (hr != S_OK) {
		return;
	}

	hr = audio_intfs.audio_client->Start();
	if (hr != S_OK) {
		return;
	}

	hr = audio_intfs.audio_client->GetService(__uuidof(IAudioRenderClient), (void**) &audio_intfs.render_client);
	if (hr != S_OK) {
		return;
	}

	hr = audio_intfs.audio_client->GetBufferSize(&audio_intfs.buffer_size);
	if (hr != S_OK) {
		return;
	}

}

static void Win32WriteSoundToBuffer() {
	u32 padding;
	audio_intfs.audio_client->GetCurrentPadding(&padding);
	u32 safe_bytes_to_write = audio_intfs.buffer_size - padding;

	byte* buffer_area;
	audio_intfs.render_client->GetBuffer(safe_bytes_to_write, (byte**) &buffer_area);

	for (u32 sample_index = 0; sample_index < safe_bytes_to_write; sample_index++) {

	}

	audio_intfs.render_client->ReleaseBuffer(safe_bytes_to_write, 0);
}

static void Win32CloseWasapi() {
	HRESULT hr;

	hr = audio_intfs.audio_client->Stop();
	if (hr != S_OK) {
		return;
	}

	hr = audio_intfs.render_client->Release();
	if (hr != S_OK) {
		return;
	}

	hr = audio_intfs.audio_client->Release();
	if (hr != S_OK) {
		return;
	}
}

// Windows entry point

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
		u32 vk_code = w_param;
		
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


int WinMain(
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

	// Initialize COM for WASAPI call using CoCreateInstance
	CoInitialize(0);

	if (RegisterClassA(&window_class)) {

		Win32LoadXInputLibrary();
		Win32InitWasapi();

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

			// Drawing to screen -> create buffer and blit it to screen
			RECT client_area;
			GetClientRect(window_handle, &client_area);
			HDC device_context = GetDC(window_handle);

			PlatformGameRender(bitmap_buffer, x_offset, y_offset);
			Win32DisplayBufferToWindow(device_context, &client_area);

			Win32WriteSoundToBuffer();

			ReleaseDC(window_handle, device_context);
		}
	}

	// Initialize COM for WASAPI call using CoCreateInstance
	CoUninitialize();
	Win32CloseWasapi();
}