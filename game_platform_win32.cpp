/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#include "game.h"
#include "game_misc.h"
#include "game_math.h"

// NOTE(ivan): Win32 API versions definitions.
#include <sdkddkver.h>
#undef _WIN32_WINNT
#undef _WIN32_IE
#undef NTDDI_VERSION

// NOTE(ivan): Win32 API target version is: Windows 7.
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define _WIN32_IE _WIN32_IE_WIN7
#define NTDDI_VERSION NTDDI_VERSION_FROM_WIN32_WINNT(_WIN32_WINNT)

// NOTE(ivan): Win32 API strict mode enable.
#define STRICT

// NOTE(ivan): Win32 API rarely-used routines exclusion.
//
// WIN32_LEAN_AND_MEAN:          keep the api header being a minimal set of mostly-required declarations and includes
// OEMRESOURCE:                  exclude OEM resource values (dunno wtf is that, but never needed them...)
// NOATOM:                       exclude atoms and their api (barely used today obsolete technique of pooling strings)
// NODRAWTEXT:                   exclude DrawText() and DT_* definitions
// NOMETAFILE:                   exclude METAFILEPICT (yet another windows weirdo we don't need)
// NOMINMAX:                     exclude min() & max() macros (we have our own)
// NOOPENFILE:                   exclude OpenFile(), OemToAnsi(), AnsiToOem(), and OF_* definitions (useless for us)
// NOSCROLL:                     exclude SB_* definitions and scrolling routines
// NOSERVICE:                    exclude Service Controller routines, SERVICE_* equates, etc...
// NOSOUND:                      exclude sound driver routines (we'd rather use OpenAL or another thirdparty API)
// NOTEXTMETRIC:                 exclude TEXTMETRIC and associated routines
// NOWH:                         exclude SetWindowsHook() and WH_* defnitions
// NOCOMM:                       exclude COMM driver routines
// NOKANJI:                      exclude Kanji support stuff
// NOHELP:                       exclude help engine interface
// NOPROFILER:                   exclude profiler interface
// NODEFERWINDOWPOS:             exclude DeferWindowPos() routines
// NOMCX:                        exclude Modem Configuration Extensions (modems in 2018, really?)
#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE
#define NOATOM
#define NODRAWTEXT
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

// NOTE(ivan): Win32 API includes.
#include <windows.h>
#include <objbase.h>
#include <versionhelpers.h>
#include <mmsystem.h>
#include <xinput.h>

// NOTE(ivan): XInput prototypes.
#define X_INPUT_GET_STATE(Name) DWORD WINAPI Name(DWORD UserIndex, XINPUT_STATE *State)
typedef X_INPUT_GET_STATE(x_input_get_state);

#define X_INPUT_SET_STATE(Name) DWORD WINAPI Name(DWORD UserIndex, XINPUT_VIBRATION *Vibration)
typedef X_INPUT_SET_STATE(x_input_set_state);

// NOTE(ivan): Win32 system structure for settings thread name by Win32SetThreadName().
struct win32_thread_name {
	DWORD Type; // NOTE(ivan): Must be 0x1000.
	LPCSTR Name;
	DWORD ThreadId;
	DWORD Flags;
};

// NOTE(ivan): Win32 video buffer.
struct win32_video_buffer {
	BITMAPINFO Info;
	u32 *Pixels;

	s32 Width;
	s32 Height;
	s32 BytesPerPixel;
	s32 Pitch;
};

// NOTE(ivan): Win32 global variables.
static struct {
	HINSTANCE Instance;
	UINT QueryCancelAutoplay;

	b32 Quitting;
	s32 QuitCode;

	u64 PerformanceFrequency;

	char ExecutableName[2048];
	char ExecutablePath[2048];

	win32_video_buffer SecondaryVideoBuffer;
} Win32State;

static void
Win32SetThreadName(DWORD Id, LPCSTR Name) {
	Assert(Name);

	win32_thread_name ThreadName = {};
	ThreadName.Type = 0x1000;
	ThreadName.Name = Name;
	ThreadName.ThreadId = Id;

#pragma warning(push)
#pragma warning(disable: 6320 6322)
	__try {
		RaiseException(0x406d1388, 0, sizeof(ThreadName) / sizeof(ULONG_PTR), (ULONG_PTR *)&ThreadName);
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
#pragma warning(pop)
}

inline u64
Win32GetClock(void) {
	LARGE_INTEGER PerformanceCounter;
	Verify(QueryPerformanceCounter(&PerformanceCounter));

	return PerformanceCounter.QuadPart;
}

inline f64
Win32GetSecondsElapsed(u64 Start, u64 End) {
	u64 Diff = End - Start;
	return (f64)(Diff / (f64)Win32State.PerformanceFrequency);
}

X_INPUT_GET_STATE(Win32XInputGetStateStub) {
	UnusedParam(UserIndex);
	UnusedParam(State);
	return ERROR_DEVICE_NOT_CONNECTED;
}

X_INPUT_SET_STATE(Win32XInputSetStateStub) {
	UnusedParam(UserIndex);
	UnusedParam(Vibration);
	return ERROR_DEVICE_NOT_CONNECTED;
}

inline void
Win32ProcessKeyboardOrMouseButton(game_input_button *Button, b32 IsDown) {
	Assert(Button);

	Button->WasDown = Button->IsDown;
	Button->IsDown = IsDown;
	Button->IsNew = true;
}

inline void
Win32ProcessXInputDigitalButton(game_input_button *Button, DWORD XInputButtonState, DWORD ButtonBit) {
	Assert(Button);

	b32 IsDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	Button->WasDown = Button->IsDown;
	Button->IsDown = IsDown;
	Button->IsNew = true;
}

inline f32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold) {
	f32 Result = 0.0f;

	if (Value < -DeadZoneThreshold)
		Result = (f32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
	else if (Value > DeadZoneThreshold)
		Result = (f32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));

	return Result;
}

static b32
Win32MapVKToKeyCode(u32 VKCode, u32 ScanCode, b32 IsE0, b32 IsE1, key_code *OutCode) {
	Assert(OutCode);

	key_code KeyCode = {}; // NOTE(ivan): Result of Windows VK -> our keycode conversion.
	b32 KeyFound = false;

	if (VKCode == 255) {
		// NOTE(ivan): Discard "fake keys" which are part of en escaped sequence.
		return false;
	} else if (VKCode == VK_SHIFT) {
		// NOTE(ivan): Correct left-hand / right-hand SHIFT.
		VKCode = MapVirtualKey(ScanCode, MAPVK_VSC_TO_VK_EX);
	} else if (VKCode == VK_NUMLOCK) {
		// NOTE(ivan): Correct PAUSE/BREAK and NUMLOCK silliness, and set the extended bit.
		ScanCode = (MapVirtualKey(VKCode, MAPVK_VK_TO_VSC) | 0x100);
	}

	// NOTE(ivan): E0 and E1 are escape sequences used for certain special keys, such as PRINTSCREEN or PAUSE/BREAK.
	// See: http://www.win.tue.nl/~aeb/linux/kbd/scancodes-1.html

	if (IsE1) {
		// NOTE(ivan): For escaped sequences, turn the virtual key into the correct scan code using MapVirtualKey.
		// However, MapVirtualKey is unable to map VK_PAUSE (this is a known bug), hence we map that by hand.
		if (VKCode == VK_PAUSE)
			ScanCode = 0x45;
		else
			ScanCode = MapVirtualKey(VKCode, MAPVK_VK_TO_VSC);
	}

	switch (VKCode) {
		// NOTE(ivan): Right-hand CONTROL and ALT have their E0 bit set.
	case VK_CONTROL: {
		if (IsE0)
			KeyCode = KeyCode_RightControl;
		else
			KeyCode = KeyCode_LeftControl;
		KeyFound = true;
	} break;

	case VK_MENU: {
		if (IsE0)
			KeyCode = KeyCode_RightAlt;
		else
			KeyCode = KeyCode_LeftAlt;
		KeyFound = true;
	} break;

		// NOTE(ivan): NUM ENTER has its E0 bit set
	case VK_RETURN: {
		if (IsE0)
			KeyCode = KeyCode_NumEnter;
		else
			KeyCode = KeyCode_Enter;
		KeyFound = true;
	} break;

		// NOTE(ivan): The standard INSERT, DELETE, HOME, END, PRIOR and NEXT keys will always have their E0 bit set,
		// but the corresponding NUM keys will not.
	case VK_INSERT: {
		if (!IsE0)
			KeyCode = KeyCode_NumInsert;
		else
			KeyCode = KeyCode_Insert;
		KeyFound = true;
	} break;
	case VK_DELETE: {
		if (!IsE0)
			KeyCode = KeyCode_NumDelete;
		else
			KeyCode = KeyCode_Delete;
		KeyFound = true;
	} break;
	case VK_HOME: {
		if (!IsE0)
			KeyCode = KeyCode_NumHome;
		else
			KeyCode = KeyCode_Home;
		KeyFound = true;
	} break;
	case VK_END: {
		if (!IsE0)
			KeyCode = KeyCode_NumEnd;
		else
			KeyCode = KeyCode_End;
		KeyFound = true;
	} break;
	case VK_PRIOR: {
		if (!IsE0)
			KeyCode = KeyCode_NumPageUp;
		else
			KeyCode = KeyCode_PageUp;
		KeyFound = true;
	} break;
	case VK_NEXT: {
		if (!IsE0)
			KeyCode = KeyCode_NumPageDown;
		else
			KeyCode = KeyCode_PageDown;
		KeyFound = true;
	} break;

		// NOTE(ivan): The standard arrow keys will awlays have their E0 bit set,
		// but the corresponding NUM keys will not.
	case VK_UP: {
		if (!IsE0)
			KeyCode = KeyCode_NumUp;
		else
			KeyCode = KeyCode_Up;
		KeyFound = true;
	} break;
	case VK_DOWN: {
		if (!IsE0)
			KeyCode = KeyCode_NumDown;
		else
			KeyCode = KeyCode_Down;
		KeyFound = true;
	} break;
	case VK_LEFT: {
		if (!IsE0)
			KeyCode = KeyCode_NumLeft;
		else
			KeyCode = KeyCode_Left;
		KeyFound = true;
	} break;
	case VK_RIGHT: {
		if (!IsE0)
			KeyCode = KeyCode_NumRight;
		else
			KeyCode = KeyCode_Right;
		KeyFound = true;
	} break;

		// NOTE(ivan): NUM 5 doesn't have its E0 bit set.
	case VK_CLEAR: {
		if (!IsE0) {
			KeyCode = KeyCode_NumClear;
			KeyFound = true;
		} else {
			return false;
		}
	} break;
	}

#define KeyMap(MapVK, MapKeyCode)				\
	if (VKCode == MapVK) {						\
		KeyCode = MapKeyCode;					\
		KeyFound = true;						\
	}
	KeyMap(VK_TAB, KeyCode_Tab);
	KeyMap(VK_ESCAPE, KeyCode_Escape);
	KeyMap(VK_SPACE, KeyCode_Space);
	KeyMap(VK_BACK, KeyCode_BackSpace);
	KeyMap(VK_LSHIFT, KeyCode_LeftShift);
	KeyMap(VK_RSHIFT, KeyCode_RightShift);
	KeyMap(VK_LMENU, KeyCode_LeftAlt);
	KeyMap(VK_RMENU, KeyCode_RightAlt);
	KeyMap(VK_LCONTROL, KeyCode_LeftControl);
	KeyMap(VK_RCONTROL, KeyCode_RightControl);
	KeyMap(VK_LWIN, KeyCode_LeftSuper);
	KeyMap(VK_RWIN, KeyCode_RightSuper);

	KeyMap(VK_F1, KeyCode_F1);
	KeyMap(VK_F2, KeyCode_F2);
	KeyMap(VK_F3, KeyCode_F3);
	KeyMap(VK_F4, KeyCode_F4);
	KeyMap(VK_F5, KeyCode_F5);
	KeyMap(VK_F6, KeyCode_F6);
	KeyMap(VK_F7, KeyCode_F7);
	KeyMap(VK_F8, KeyCode_F8);
	KeyMap(VK_F9, KeyCode_F9);
	KeyMap(VK_F10, KeyCode_F10);
	KeyMap(VK_F11, KeyCode_F11);
	KeyMap(VK_F12, KeyCode_F12);
	
	KeyMap(VK_NUMLOCK, KeyCode_NumLock);
	KeyMap(VK_CAPITAL, KeyCode_CapsLock);
	KeyMap(VK_SCROLL, KeyCode_ScrollLock);
	
	KeyMap(VK_PRINT, KeyCode_PrintScreen);
	KeyMap(VK_PAUSE, KeyCode_Pause);

	KeyMap(0x41, KeyCode_A);
	KeyMap(0x42, KeyCode_B);
	KeyMap(0x43, KeyCode_C);
	KeyMap(0x44, KeyCode_D);
	KeyMap(0x45, KeyCode_E);
	KeyMap(0x46, KeyCode_F);
	KeyMap(0x47, KeyCode_G);
	KeyMap(0x48, KeyCode_H);
	KeyMap(0x49, KeyCode_I);
	KeyMap(0x4A, KeyCode_J);
	KeyMap(0x4B, KeyCode_K);
	KeyMap(0x4C, KeyCode_L);
	KeyMap(0x4D, KeyCode_M);
	KeyMap(0x4E, KeyCode_N);
	KeyMap(0x4F, KeyCode_O);
	KeyMap(0x50, KeyCode_P);
	KeyMap(0x51, KeyCode_Q);
	KeyMap(0x52, KeyCode_R);
	KeyMap(0x53, KeyCode_S);
	KeyMap(0x54, KeyCode_T);
	KeyMap(0x55, KeyCode_U);
	KeyMap(0x56, KeyCode_V);
	KeyMap(0x57, KeyCode_W);
	KeyMap(0x58, KeyCode_X);
	KeyMap(0x59, KeyCode_Y);
	KeyMap(0x5A, KeyCode_Z);

	KeyMap(0x30, KeyCode_0);
	KeyMap(0x31, KeyCode_1);
	KeyMap(0x32, KeyCode_2);
	KeyMap(0x33, KeyCode_3);
	KeyMap(0x34, KeyCode_4);
	KeyMap(0x35, KeyCode_5);
	KeyMap(0x36, KeyCode_6);
	KeyMap(0x37, KeyCode_7);
	KeyMap(0x38, KeyCode_8);
	KeyMap(0x39, KeyCode_9);

	KeyMap(VK_OEM_4, KeyCode_OpenBracket);
	KeyMap(VK_OEM_6, KeyCode_CloseBracket);
	KeyMap(VK_OEM_1, KeyCode_Semicolon);
	KeyMap(VK_OEM_7, KeyCode_Quote);
	KeyMap(VK_OEM_COMMA, KeyCode_Comma);
	KeyMap(VK_OEM_PERIOD, KeyCode_Period);
	KeyMap(VK_OEM_2, KeyCode_Slash);
	KeyMap(VK_OEM_5, KeyCode_BackSlash);
	KeyMap(VK_OEM_3, KeyCode_Tilde);
	KeyMap(VK_OEM_PLUS, KeyCode_Plus);
	KeyMap(VK_OEM_MINUS, KeyCode_Minus);

	KeyMap(VK_NUMPAD8, KeyCode_Num8);
	KeyMap(VK_NUMPAD2, KeyCode_Num2);
	KeyMap(VK_NUMPAD4, KeyCode_Num4);
	KeyMap(VK_NUMPAD6, KeyCode_Num6);
	KeyMap(VK_NUMPAD7, KeyCode_Num7);
	KeyMap(VK_NUMPAD1, KeyCode_Num1);
	KeyMap(VK_NUMPAD9, KeyCode_Num9);
	KeyMap(VK_NUMPAD3, KeyCode_Num3);
	KeyMap(VK_NUMPAD0, KeyCode_Num0);
	KeyMap(VK_SEPARATOR, KeyCode_NumSeparator);
	KeyMap(VK_MULTIPLY, KeyCode_NumMultiply);
	KeyMap(VK_DIVIDE, KeyCode_NumDivide);
	KeyMap(VK_ADD, KeyCode_NumPlus);
	KeyMap(VK_SUBTRACT, KeyCode_NumMinus);
#undef KeyMap
	if (!KeyFound)
		return false;

	*OutCode = KeyCode;
	return true;
}

inline rectangle
Win32GetWindowClientDimension(HWND Window) {
	rectangle Result = {};

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);

	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

static void
Win32ResizeVideoBuffer(win32_video_buffer *Buffer, s32 NewWidth, s32 NewHeight) {
	Assert(Buffer);

	if (Buffer->Pixels)
		VirtualFree(Buffer->Pixels, 0, MEM_RELEASE);

	u16 NewBytesPerPixel = 4;
	
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = NewWidth;
	Buffer->Info.bmiHeader.biHeight = -NewHeight; // NOTE(ivan): Minus for top-left.
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = NewBytesPerPixel * 8;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	u32 NewSize = NewWidth * NewHeight * NewBytesPerPixel;
	if (NewSize) {
		Buffer->Pixels = (u32 *)VirtualAlloc(0, NewSize, MEM_COMMIT, PAGE_READWRITE);
		if (!Buffer->Pixels) {
			NewWidth = NewHeight = NewBytesPerPixel = 0;
			// TODO(ivan): Diagnostics.
		}
	}

	Buffer->Width = NewWidth;
	Buffer->Height = NewHeight;
	Buffer->BytesPerPixel = NewBytesPerPixel;

	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

	// TODO(ivan): Diagnostics.
}

static LRESULT CALLBACK
Win32WindowProc(HWND Window, UINT Msg, WPARAM W, LPARAM L) {
	switch (Msg) {
	case WM_DESTROY: {
		PostQuitMessage(0);
	} break;

	case WM_PAINT: {
		rectangle ClientDim = Win32GetWindowClientDimension(Window);

		PAINTSTRUCT WindowPS;
		HDC WindowDC = BeginPaint(Window, &WindowPS);
		PatBlt(WindowDC, 0, 0, ClientDim.Width, ClientDim.Height, BLACKNESS);
		EndPaint(Window, &WindowPS);
	} break;

	case WM_SIZE: {
		rectangle ClientDim = Win32GetWindowClientDimension(Window);
		Win32ResizeVideoBuffer(&Win32State.SecondaryVideoBuffer,
							   ClientDim.Width,
							   ClientDim.Height);
	} break;

	case WM_INPUT: {
		u8 Buffer[sizeof(RAWINPUT)] = {};
		u32 BufferSize = sizeof(RAWINPUT);
		GetRawInputData((HRAWINPUT)L, RID_INPUT, Buffer, &BufferSize, sizeof(RAWINPUTHEADER));

		RAWINPUT *RawInput = (RAWINPUT *)Buffer;
		if (RawInput->header.dwType == RIM_TYPEKEYBOARD) {
			RAWKEYBOARD *RawKeyboard = &RawInput->data.keyboard;

			key_code KeyCode;
			if (Win32MapVKToKeyCode(RawKeyboard->VKey, RawKeyboard->MakeCode,
									(RawKeyboard->Flags & RI_KEY_E0) != 0,
									(RawKeyboard->Flags & RI_KEY_E1) != 0,
									&KeyCode))
				Win32ProcessKeyboardOrMouseButton(&GameState.KeyboardButtons[KeyCode], (RawKeyboard->Flags & RI_KEY_BREAK) == 0);
		} else if (RawInput->header.dwType == RIM_TYPEMOUSE) {
			RAWMOUSE *RawMouse = &RawInput->data.mouse;

			if (RawMouse->usFlags == MOUSE_MOVE_ABSOLUTE) {
				GameState.MousePos.X = RawMouse->lLastX;
				GameState.MousePos.Y = RawMouse->lLastY;
			}

			switch (RawMouse->usButtonFlags) {
			case RI_MOUSE_BUTTON_1_DOWN: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[0], true);
			} break;
			case RI_MOUSE_BUTTON_1_UP: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[0], false);
			} break;

			case RI_MOUSE_BUTTON_2_DOWN: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[1], true);
			} break;
			case RI_MOUSE_BUTTON_2_UP: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[1], false);
			} break;

			case RI_MOUSE_BUTTON_3_DOWN: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[2], true);
			} break;
			case RI_MOUSE_BUTTON_3_UP: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[2], false);
			} break;

			case RI_MOUSE_BUTTON_4_DOWN: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[3], true);
			} break;
			case RI_MOUSE_BUTTON_4_UP: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[3], false);
			} break;

			case RI_MOUSE_BUTTON_5_DOWN: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[4], true);
			} break;
			case RI_MOUSE_BUTTON_5_UP: {
				Win32ProcessKeyboardOrMouseButton(&GameState.MouseButtons[4], false);
			} break;

			case RI_MOUSE_WHEEL: {
				GameState.MouseWheel = RawMouse->usButtonData;
			} break;
			}
		}
	} break;

	default: {
		if (Msg == Win32State.QueryCancelAutoplay)
			return TRUE; // NOTE(ivan): Cancel CD-ROM autoplay.
		
		return DefWindowProcA(Window, Msg, W, L);
	} break;
	}
	
	return FALSE;
}

void
PlatformQuit(s32 QuitCode) {
	Win32State.Quitting = true;
	Win32State.QuitCode = QuitCode;
}

int CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR CommandLine,
		int ShowCommand) {
	UnusedParam(PrevInstance);
	UnusedParam(CommandLine);
	UnusedParam(ShowCommand);
	
	Win32State.Instance = Instance;
	Win32SetThreadName(GetCurrentThreadId(), GAMENAME " Primary Thread");

	SetCursor(LoadCursorA(0, MAKEINTRESOURCEA(32514))); // NOTE(ivan): IDC_WAIT.

	if (IsWindows7OrGreater()) {
		// NOTE(ivan): Strange but the only way to set system's scheduler granularity
		// so our Sleep() calls will be way more accurate.
		b32 IsSleepGranular = timeBeginPeriod(1);

		// NOTE(ivan): Query CD-ROM autoplay feature disable.
		Win32State.QueryCancelAutoplay = RegisterWindowMessageA("QueryCancelAutoplay");

		LARGE_INTEGER PerformanceFrequency;
		if (QueryPerformanceFrequency(&PerformanceFrequency)) {
			Win32State.PerformanceFrequency = PerformanceFrequency.QuadPart;

			// NOTE(ivan): Obtain executable's file and path name.
			char ModuleName[2048] = {};
			char *PastLastSlash = ModuleName, *Ptr = ModuleName;
			Verify(GetModuleFileNameA(Win32State.Instance, ModuleName, CountOf(ModuleName) - 1));
			while (*Ptr) {
				if (*Ptr == '\\')
					PastLastSlash = Ptr + 1;
				Ptr++;
			}
			CopyString(Win32State.ExecutableName, PastLastSlash);
			CopyStringN(Win32State.ExecutablePath, ModuleName, PastLastSlash - ModuleName);

			// NOTE(ivan): Prepare game main memory (hunk).
			uptr HunkSize = 0;

#ifdef _M_IX86
			BOOL IsWow64 = FALSE;
			BOOL (APIENTRY *IsWow64ProcessFunc)(HANDLE, PBOOL) = (BOOL (APIENTRY *)(HANDLE, PBOOL))GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process");
			if (IsWow64ProcessFunc) {
				if (!IsWow64ProcessFunc(GetCurrentProcess(), &IsWow64))
					IsWow64 = FALSE;
			}
#endif

			MEMORYSTATUSEX MemStat;
			MemStat.dwLength = sizeof(MemStat);
			if (GlobalMemoryStatusEx(&MemStat)) {
				HunkSize = (uptr)(0.8f * (f32)MemStat.ullAvailPhys);
			} else {
#if defined(_M_IX86)
				HunkSize = IsWow64 ? Megabytes(3584) : Gigabytes(3);
#elif defined(_M_X64)
				HunkSize = Gigabytes(4);
#endif				
			}

			SYSTEM_INFO SysInfo;
			GetNativeSystemInfo(&SysInfo);
			HunkSize = AlignPow2(HunkSize, (uptr)SysInfo.dwPageSize);

			GameState.Hunk.Size = HunkSize;
			GameState.Hunk.Base = (u8 *)VirtualAlloc(0, HunkSize, MEM_COMMIT, PAGE_READWRITE);
			if (GameState.Hunk.Base) {
				// NOTE(ivan): Create main window and its device context.
				WNDCLASSA WindowClass = {};
				WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
				WindowClass.lpszClassName = GAMENAME " Window";
				WindowClass.lpfnWndProc = Win32WindowProc;
				WindowClass.hInstance = Win32State.Instance;

				if (RegisterClassA(&WindowClass)) {
					HWND Window = CreateWindowExA(WS_EX_APPWINDOW,
												  WindowClass.lpszClassName, GAMENAME,
												  WS_OVERLAPPEDWINDOW,
												  CW_USEDEFAULT, CW_USEDEFAULT,
												  CW_USEDEFAULT, CW_USEDEFAULT,
												  0, 0, Win32State.Instance, 0);
					if (Window) {
						// NOTE(ivan): Since we specified CS_OWNDC, we can just get one device context
						// and use it forever because we are not sharing it with anyone.
						HDC WindowDC = GetDC(Window);
						if (WindowDC) {
#if INTERNAL							
							b32 DebugCursor = true;
#else
							b32 DebugCursor = false;
#endif
							
							// NOTE(ivan): Initialize raw keyboard and mouse input.
							RAWINPUTDEVICE RawDevices[2];
	
							RawDevices[0].usUsagePage = 0x01;
							RawDevices[0].usUsage = 0x06;
							RawDevices[0].dwFlags = RIDEV_NOLEGACY;
							RawDevices[0].hwndTarget = Window;
							
							RawDevices[1].usUsagePage = 0x01;
							RawDevices[1].usUsage = 0x02;
							RawDevices[1].dwFlags = 0;
							RawDevices[1].hwndTarget = Window;

							RegisterRawInputDevices(RawDevices, 2, sizeof(RAWINPUTDEVICE));

							x_input_get_state *XIGetState;
							x_input_set_state *XISetState;
							HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
							if (!XInputLibrary)
								XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
							if (!XInputLibrary)
								XInputLibrary = LoadLibraryA("xinput1_3.dll");
							if (!XInputLibrary) {
								XIGetState = Win32XInputGetStateStub;
								XISetState = Win32XInputSetStateStub;
								// TODO(ivan): Diagnostics.
							} else {
								XIGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
								XISetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");

								if (XIGetState && XISetState) {
									// NOTE(ivan): Success.
								} else {
									XIGetState = Win32XInputGetStateStub;
									XISetState = Win32XInputSetStateStub;
									// TODO(ivan): Diagnostics.
								}
							}

							GameUpdate(GameUpdateType_Prepare);
 
							// NOTE(ivan): After all initialization is complete, show main window.
							ShowWindow(Window, ShowCommand);
							SetCursor(LoadCursorA(0, MAKEINTRESOURCE(32512))); // NOTE(ivan): IDC_ARROW.

							u64 LastCounter = Win32GetClock();
							u64 LastCycleCounter = __rdtsc();

							// NOTE(ivan): Primary loop.
							while (!Win32State.Quitting) {
								// NOTE(ivan): Process Win32 messages.
								static MSG Msg;
								while (PeekMessageA(&Msg, 0, 0, 0, PM_REMOVE)) {
									if (Msg.message == WM_QUIT)
										PlatformQuit((s32)Msg.wParam);

									TranslateMessage(&Msg);
									DispatchMessageA(&Msg);
								}

								// NOTE(ivan): Process Xbox controller state.
								// TODO(ivan): Monitor Xbox controllers for plugged in after the fact!
								b32 XboxControllerPresent[XUSER_MAX_COUNT];
								for (u32 Index = 0; Index < CountOf(XboxControllerPresent); Index++)
									XboxControllerPresent[Index] = true;

								// TODO(ivan): Need to not poll disconnected controllers to avoid XInput frame rate hit on older libraries...
								// TODO(ivan): Should we poll this more frequently?
								DWORD MaxXboxControllerCount = CountOf(XboxControllerPresent);
								if (MaxXboxControllerCount > CountOf(GameState.XboxControllers))
									MaxXboxControllerCount = CountOf(GameState.XboxControllers);
								for (u32 Index = 0; Index < MaxXboxControllerCount; Index++) {
									game_input_xbox_controller *XboxController = &GameState.XboxControllers[Index];
									XINPUT_STATE XboxControllerState;
									if (XboxControllerPresent[Index] && XIGetState(Index, &XboxControllerState) == ERROR_SUCCESS) {
										XboxController->IsConnected = true;

										// TODO(ivan): See if ControllerState.dwPacketNumber increments too rapidly.
										XINPUT_GAMEPAD *XboxGamepad = &XboxControllerState.Gamepad;

										Win32ProcessXInputDigitalButton(&XboxController->Start,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_START);
										Win32ProcessXInputDigitalButton(&XboxController->Back,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_BACK);

										Win32ProcessXInputDigitalButton(&XboxController->A,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_A);
										Win32ProcessXInputDigitalButton(&XboxController->B,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_B);
										Win32ProcessXInputDigitalButton(&XboxController->X,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_X);
										Win32ProcessXInputDigitalButton(&XboxController->Y,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_Y);

										Win32ProcessXInputDigitalButton(&XboxController->Up,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_DPAD_UP);
										Win32ProcessXInputDigitalButton(&XboxController->Down,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_DPAD_DOWN);
										Win32ProcessXInputDigitalButton(&XboxController->Left,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_DPAD_LEFT);
										Win32ProcessXInputDigitalButton(&XboxController->Right,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_DPAD_RIGHT);

										Win32ProcessXInputDigitalButton(&XboxController->LeftBumper,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER);
										Win32ProcessXInputDigitalButton(&XboxController->RightBumper,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER);

										Win32ProcessXInputDigitalButton(&XboxController->LeftStick,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_LEFT_THUMB);
										Win32ProcessXInputDigitalButton(&XboxController->RightStick,
																		XboxGamepad->wButtons, XINPUT_GAMEPAD_RIGHT_THUMB);

										XboxController->LeftTrigger = XboxGamepad->bLeftTrigger;
										XboxController->RightTrigger = XboxGamepad->bRightTrigger;

										XboxController->LeftStickPos.X = Win32ProcessXInputStickValue(XboxGamepad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
										XboxController->LeftStickPos.Y = Win32ProcessXInputStickValue(XboxGamepad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
										XboxController->RightStickPos.X = Win32ProcessXInputStickValue(XboxGamepad->sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
										XboxController->RightStickPos.Y = Win32ProcessXInputStickValue(XboxGamepad->sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
									} else {
										XboxController->IsConnected = false;
									}
								}

								// NOTE(ivan): Process Win32-side input events.
								//
								if (GameState.KeyboardButtons[KeyCode_F4].IsDown &&
									(GameState.KeyboardButtons[KeyCode_LeftAlt].IsDown || GameState.KeyboardButtons[KeyCode_RightAlt].IsDown))
									PlatformQuit(0);
#if INTERNAL
								if (IsNewlyPressed(&GameState.KeyboardButtons[KeyCode_F2]))
									DebugCursor = !DebugCursor;
#endif

								// NOTE(ivan): Set cursor.
								if (DebugCursor)
									SetCursor(LoadCursorA(0, MAKEINTRESOURCEA(32515))); // NOTE(ivan): IDC_CROSS.
								else
									SetCursor(0);

								// NOTE(ivan): Prepare game video buffer.
								GameState.VideoBuffer.Pixels = Win32State.SecondaryVideoBuffer.Pixels;
								GameState.VideoBuffer.Width = Win32State.SecondaryVideoBuffer.Width;
								GameState.VideoBuffer.Height = Win32State.SecondaryVideoBuffer.Height;
								GameState.VideoBuffer.BytesPerPixel = Win32State.SecondaryVideoBuffer.BytesPerPixel;
								GameState.VideoBuffer.Pitch = Win32State.SecondaryVideoBuffer.Pitch;

								GameUpdate(GameUpdateType_Frame);

								// NOTE(ivan): Output game video buffer.
								static rectangle ClientDim = Win32GetWindowClientDimension(Window);
								StretchDIBits(WindowDC,
											  0, 0, ClientDim.Width, ClientDim.Height,
											  0, 0, Win32State.SecondaryVideoBuffer.Width, Win32State.SecondaryVideoBuffer.Height,
											  Win32State.SecondaryVideoBuffer.Pixels, &Win32State.SecondaryVideoBuffer.Info, DIB_RGB_COLORS, SRCCOPY);

								// NOTE(ivan): Before the next frame, make all input states obsolete.
								for (u32 Index = 0; Index < CountOf(GameState.KeyboardButtons); Index++)
									GameState.KeyboardButtons[Index].IsNew = false;

								for (u32 Index = 0; Index < CountOf(GameState.MouseButtons); Index++)
									GameState.MouseButtons[Index].IsNew = false;

								for (u32 Index = 0; Index < CountOf(GameState.XboxControllers); Index++) {
									game_input_xbox_controller *XboxController = &GameState.XboxControllers[Index];

									XboxController->Start.IsNew = false;
									XboxController->Back.IsNew = false;

									XboxController->A.IsNew = false;
									XboxController->B.IsNew = false;
									XboxController->X.IsNew = false;
									XboxController->Y.IsNew = false;

									XboxController->Up.IsNew = false;
									XboxController->Down.IsNew = false;
									XboxController->Left.IsNew = false;
									XboxController->Right.IsNew = false;

									XboxController->LeftBumper.IsNew = false;
									XboxController->RightBumper.IsNew = false;

									XboxController->LeftStick.IsNew = false;
									XboxController->RightStick.IsNew = false;
								}

								static u32 DisplayFrequency = GetDeviceCaps(WindowDC, VREFRESH);
								if (DisplayFrequency <= 1)
									DisplayFrequency = 60;

								u64 WorkCounter = Win32GetClock();

								f64 TargetSecondsPerFrame = (f64)(1.0 / DisplayFrequency);
								f64 SecondsElapsedForWork = Win32GetSecondsElapsed(LastCounter, WorkCounter);
								GameState.SecondsPerFrame = SecondsElapsedForWork;
								GameState.FramesPerSecond = Win32State.PerformanceFrequency / (f64)(WorkCounter - LastCounter);

								if (SecondsElapsedForWork < TargetSecondsPerFrame) {
									if (IsSleepGranular) {
										f64 SecondsToSleep = TargetSecondsPerFrame - SecondsElapsedForWork;
										DWORD SleepMS = (DWORD)(1000 * SecondsToSleep);
										if (SleepMS > 0) {
											Sleep(SleepMS);
											SecondsElapsedForWork += SecondsToSleep;
										}
									} else {
										while (SecondsElapsedForWork < TargetSecondsPerFrame)
											SecondsElapsedForWork = Win32GetSecondsElapsed(LastCounter, Win32GetClock());
									}
								} else {
									// NOTE(ivan): Missing framerate!
									// TODO(ivan): Diagnostics.
								}

								u64 EndCounter = Win32GetClock();
								u64 EndCycleCounter = __rdtsc();

								GameState.CyclesPerFrame = ((f64)(EndCycleCounter - LastCycleCounter) / (1000.0 * 1000.0));
		
								LastCounter = EndCounter;
								LastCycleCounter = EndCycleCounter;
							}

							GameUpdate(GameUpdateType_Release);

							if (XInputLibrary)
								FreeLibrary(XInputLibrary);
							
							RawDevices[0].dwFlags = RIDEV_REMOVE;
							RawDevices[1].dwFlags = RIDEV_REMOVE;
							RegisterRawInputDevices(RawDevices, 2, sizeof(RAWINPUTDEVICE));
							
							ReleaseDC(Window, WindowDC);
						} else {
							// TODO(ivan): Diagnostics (GetDC).
						}
					
						DestroyWindow(Window);
					} else {
						// TODO(ivan): Diagnostics (CreateWindowExA).
					}
				
					UnregisterClassA(WindowClass.lpszClassName, Win32State.Instance);
				} else {
					// TODO(ivan): Diagnostics (RegisterClassA).
				}
			} else {
				// TODO(ivan): Diagnostics (VirtualAlloc).
			}

			if (IsSleepGranular)
				timeEndPeriod(1);
		} else {
			// TODO(ivan): Diagnostics (QueryPerformanceFrequency).
		}
	} else {
		// TODO(ivan): Diagnostics (IsWindows7OrGreater).
	}
	
	return Win32State.QuitCode;
}
