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

// NOTE(ivan): POSIX standard includes.
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>

// NOTE(ivan): X11 includes.
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

// NOTE(ivan): X11 extensions includes.
#include <X11/XKBlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xrandr.h>

// NOTE(ivan): Linux includes.
#include <linux/joystick.h>

#define MAX_CONTROLLERS 8

// NOTE(ivan): Xbox controller definitions (found out by xev).
#define XBOX_CONTROLLER_DEADZONE 5000

#define XBOX_CONTROLLER_AXIS_LEFT_THUMB_X 0
#define XBOX_CONTROLLER_AXIS_LEFT_THUMB_Y 1
#define XBOX_CONTROLLER_AXIS_RIGHT_THUMB_X 2
#define XBOX_CONTROLLER_AXIS_RIGHT_THUMB_Y 3
#define XBOX_CONTROLLER_AXIS_RIGHT_TRIGGER 4
#define XBOX_CONTROLLER_AXIS_LEFT_TRIGGER 5
#define XBOX_CONTROLLER_AXIS_DPAD_HORZ 6
#define XBOX_CONTROLLER_AXIS_DPAD_VERT 7

#define XBOX_CONTROLLER_BUTTON_A 0
#define XBOX_CONTROLLER_BUTTON_B 1
#define XBOX_CONTROLLER_BUTTON_X 2
#define XBOX_CONTROLLER_BUTTON_Y 3
#define XBOX_CONTROLLER_BUTTON_LEFT_SHOULDER 4
#define XBOX_CONTROLLER_BUTTON_RIGHT_SHOULDER 5
#define XBOX_CONTROLLER_BUTTON_BACK 6
#define XBOX_CONTROLLER_BUTTON_START 7
#define XBOX_CONTROLLER_BUTTON_LEFT_THUMB 9
#define XBOX_CONTROLLER_BUTTON_RIGHT_THUMB 10

// NOTE(ivan): Linux video buffer.
struct linux_video_buffer {
	XImage *Image;
	XShmSegmentInfo SegmentInfo;
	u32 *Pixels;
	s32 Width;
	s32 Height;
	s32 BytesPerPixel;
	s32 Pitch;
};

// NOTE(ivan): Linux global variables.
static struct linux_state {
	s32 ArgC;
	char **ArgV;
	
	b32 Quitting;
	s32 QuitCode;

	char ExecutableName[2048];
	char ExecutablePath[2048];

	Display *XDisplay;
	s32 XDefScreen;
	s32 XDefDepth;
	Visual *XDefVisual;
	Window XRootWindow;
	u32 XDefBlack;
	u32 XDefWhite;
	Atom XWMDeleteWindow;
} LinuxState = {};

inline struct timespec
LinuxGetClock(void) {
	struct timespec Result;
	clock_gettime(CLOCK_MONOTONIC, &Result);
	return Result;
}

inline u64
LinuxGetRDTSC(void) {
#if defined(__i386__)
	u64 Value;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (Value));
	return Value;
#elif defined(__x86_64__) || defined(__amd64__)
	u32 High, Low;
	__asm__ volatile ("rdtsc" : "=a"(Low), "=d"(High));
	return (u64)((u64)Low | ((u64)High) << 32);
#endif
}

inline f64
LinuxGetSecondsElapsed(struct timespec Start,
					   struct timespec End) {
	return ((f64)(End.tv_sec - Start.tv_sec) + ((f64)(End.tv_nsec - Start.tv_nsec) * 1e-9f));
}

inline void
LinuxProcessKeyboardOrMouseButton(game_input_button *Button, b32 IsDown) {
	Assert(Button);

	Button->WasDown = Button->IsDown;
	Button->IsDown = IsDown;
	Button->IsNew = true;
}

inline void
LinuxProcessXboxDigitalButton(game_input_button *Button, s16 XboxButtonState) {
	Assert(Button);

	b32 IsDown = (XboxButtonState != 0);
	Button->WasDown = Button->IsDown;
	Button->IsDown = IsDown;
	Button->IsNew = true;
}

inline f32
LinuxProcessXboxStickValue(s16 Value, s16 DeadZoneThreshold) {
	f32 Result = 0.0f;

	if (Value < -DeadZoneThreshold)
		Result = (f32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
	else if (Value > DeadZoneThreshold)
		Result = (f32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));

	return Result;
}

static b32
LinuxMapXKeySymToKeyCode(KeySym XKeySym, key_code *OutCode) {
	Assert(OutCode);

	key_code KeyCode;
	b32 KeyFound = false;
#define KeyMap(MapXKeyCode, MapKeyCode)				\
	if (XKeySym == MapXKeyCode) {					\
		KeyCode = MapKeyCode;						\
		KeyFound = true;							\
	}
	KeyMap(XK_Return, KeyCode_Enter);
	KeyMap(XK_Tab, KeyCode_Tab);
	KeyMap(XK_Escape, KeyCode_Escape);
	KeyMap(XK_KP_Space, KeyCode_Space);
	KeyMap(XK_BackSpace, KeyCode_BackSpace);
	KeyMap(XK_Shift_L, KeyCode_LeftShift);
	KeyMap(XK_Shift_R, KeyCode_RightShift);
	KeyMap(XK_Alt_L, KeyCode_LeftAlt);
	KeyMap(XK_Alt_R, KeyCode_RightAlt);
	KeyMap(XK_Control_L, KeyCode_LeftControl);
	KeyMap(XK_Control_R, KeyCode_RightControl);
	KeyMap(XK_Super_L, KeyCode_LeftSuper);
	KeyMap(XK_Super_R, KeyCode_RightSuper);
  	KeyMap(XK_Home, KeyCode_Home);
	KeyMap(XK_End, KeyCode_End);
	KeyMap(XK_Prior, KeyCode_PageUp);
	KeyMap(XK_Next, KeyCode_PageDown);
	KeyMap(XK_Insert, KeyCode_Insert);
	KeyMap(XK_Delete, KeyCode_Delete);
	KeyMap(XK_Up, KeyCode_Up);
	KeyMap(XK_Down, KeyCode_Down);
	KeyMap(XK_Left, KeyCode_Left);
	KeyMap(XK_Right, KeyCode_Right);
	KeyMap(XK_F1, KeyCode_F1);
	KeyMap(XK_F2, KeyCode_F2);
	KeyMap(XK_F3, KeyCode_F3);
	KeyMap(XK_F4, KeyCode_F4);
	KeyMap(XK_F5, KeyCode_F5);
	KeyMap(XK_F6, KeyCode_F6);
	KeyMap(XK_F7, KeyCode_F7);
	KeyMap(XK_F8, KeyCode_F8);
	KeyMap(XK_F9, KeyCode_F9);
	KeyMap(XK_F10, KeyCode_F10);
	KeyMap(XK_F11, KeyCode_F11);
	KeyMap(XK_F12, KeyCode_F12);
	
	KeyMap(XK_Num_Lock, KeyCode_NumLock);
	KeyMap(XK_Caps_Lock, KeyCode_CapsLock);
	KeyMap(XK_Scroll_Lock, KeyCode_ScrollLock);
	
	KeyMap(XK_Print, KeyCode_PrintScreen);
	KeyMap(XK_Pause, KeyCode_Pause);

	KeyMap(XK_a, KeyCode_A);
	KeyMap(XK_b, KeyCode_B);
	KeyMap(XK_c, KeyCode_C);
	KeyMap(XK_d, KeyCode_D);
	KeyMap(XK_e, KeyCode_E);
	KeyMap(XK_f, KeyCode_F);
	KeyMap(XK_g, KeyCode_G);
	KeyMap(XK_h, KeyCode_H);
	KeyMap(XK_i, KeyCode_I);
	KeyMap(XK_j, KeyCode_J);
	KeyMap(XK_k, KeyCode_K);
	KeyMap(XK_l, KeyCode_L);
	KeyMap(XK_m, KeyCode_M);
	KeyMap(XK_n, KeyCode_N);
	KeyMap(XK_o, KeyCode_O);
	KeyMap(XK_p, KeyCode_P);
	KeyMap(XK_q, KeyCode_Q);
	KeyMap(XK_r, KeyCode_R);
	KeyMap(XK_s, KeyCode_S);
	KeyMap(XK_t, KeyCode_T);
	KeyMap(XK_u, KeyCode_U);
	KeyMap(XK_v, KeyCode_V);
	KeyMap(XK_w, KeyCode_W);
	KeyMap(XK_x, KeyCode_X);
	KeyMap(XK_y, KeyCode_Y);
	KeyMap(XK_z, KeyCode_Z);

	KeyMap(XK_0, KeyCode_0);
	KeyMap(XK_1, KeyCode_1);
	KeyMap(XK_2, KeyCode_2);
	KeyMap(XK_3, KeyCode_3);
	KeyMap(XK_4, KeyCode_4);
	KeyMap(XK_5, KeyCode_5);
	KeyMap(XK_6, KeyCode_6);
	KeyMap(XK_7, KeyCode_7);
	KeyMap(XK_8, KeyCode_8);
	KeyMap(XK_9, KeyCode_9);

	KeyMap(XK_bracketleft, KeyCode_OpenBracket);
	KeyMap(XK_bracketright, KeyCode_CloseBracket);
	KeyMap(XK_semicolon, KeyCode_Semicolon);
	KeyMap(XK_quotedbl, KeyCode_Quote);
	KeyMap(XK_comma, KeyCode_Comma);
	KeyMap(XK_period, KeyCode_Period);
	KeyMap(XK_slash, KeyCode_Slash);
	KeyMap(XK_backslash, KeyCode_BackSlash);
	KeyMap(XK_asciitilde, KeyCode_Tilde);
	KeyMap(XK_plus, KeyCode_Plus);
	KeyMap(XK_minus, KeyCode_Minus);

	KeyMap(XK_KP_Up, KeyCode_NumUp);
	KeyMap(XK_KP_Down, KeyCode_NumDown);
	KeyMap(XK_KP_Left, KeyCode_NumLeft);
	KeyMap(XK_KP_Right, KeyCode_NumRight);
	KeyMap(XK_KP_Home, KeyCode_NumHome);
	KeyMap(XK_KP_End, KeyCode_NumEnd);
	KeyMap(XK_KP_Prior, KeyCode_NumPageUp);
	KeyMap(XK_KP_Next, KeyCode_NumPageDown);

	KeyMap(XK_KP_0, KeyCode_Num0);
	KeyMap(XK_KP_1, KeyCode_Num1);
	KeyMap(XK_KP_2, KeyCode_Num2);
	KeyMap(XK_KP_3, KeyCode_Num3);
	KeyMap(XK_KP_4, KeyCode_Num4);
	KeyMap(XK_KP_5, KeyCode_Num5);
	KeyMap(XK_KP_6, KeyCode_Num6);
	KeyMap(XK_KP_7, KeyCode_Num7);
	KeyMap(XK_KP_8, KeyCode_Num8);
	KeyMap(XK_KP_9, KeyCode_Num9);
	
	KeyMap(XK_KP_Multiply, KeyCode_NumMultiply);
	KeyMap(XK_KP_Divide, KeyCode_NumDivide);
	KeyMap(XK_KP_Add, KeyCode_NumPlus);
	KeyMap(XK_KP_Subtract, KeyCode_NumMinus);
	KeyMap(XK_KP_Separator, KeyCode_NumClear);
#undef KeyMap	

	if (!KeyFound)
		return false;
	*OutCode = KeyCode;
	return true;
}

inline b32
LinuxDirectoryExists(const char *DirName) {
	Assert(DirName);

	DIR *Dir = opendir(DirName);
	if (!Dir)
		return false;

	closedir(Dir);
	return true;
}

inline rectangle
LinuxGetWindowClientDimension(Window W) {
	rectangle Result;
	
	Window RootWindow;
	int WindowX, WindowY;
	unsigned int WindowWidth, WindowHeight;
	unsigned int WindowBorder, WindowDepth;
	XGetGeometry(LinuxState.XDisplay, W,
				 &RootWindow,
				 &WindowX, &WindowY,
				 &WindowWidth, &WindowHeight,
				 &WindowBorder, &WindowDepth);

	Result.Width = (s32)WindowWidth;
	Result.Height = (s32)WindowHeight;

	return Result;
}

static void
LinuxResizeVideoBuffer(linux_video_buffer *Buffer, s32 NewWidth, s32 NewHeight) {
	Assert(Buffer);

	if (Buffer->Image) {
		XShmDetach(LinuxState.XDisplay, &Buffer->SegmentInfo);
		XDestroyImage(Buffer->Image);
		shmdt(Buffer->SegmentInfo.shmaddr);
		shmctl(Buffer->SegmentInfo.shmid, IPC_RMID, 0);

		munmap(Buffer->Pixels, Buffer->Width * Buffer->Height * Buffer->BytesPerPixel);

		Buffer->Image = 0;
		Buffer->Pixels = 0;
	}

	static s32 NewBytesPerPixel = 4;

	if ((NewWidth * NewHeight) != 0) {
		Buffer->Image = XShmCreateImage(LinuxState.XDisplay,
										LinuxState.XDefVisual,
										LinuxState.XDefDepth,
										ZPixmap,
										0,
										&Buffer->SegmentInfo,
										NewWidth, NewHeight);
		Buffer->SegmentInfo.shmid = shmget(IPC_PRIVATE,
										   Buffer->Image->bytes_per_line * Buffer->Image->height,
										   IPC_CREAT | 0777);
		Buffer->SegmentInfo.shmaddr = Buffer->Image->data = (char *)shmat(Buffer->SegmentInfo.shmid, 0, 0);
		Buffer->SegmentInfo.readOnly = False;

		XShmAttach(LinuxState.XDisplay, &Buffer->SegmentInfo);

		Buffer->Pixels = (u32 *)mmap(0, NewWidth * NewHeight * NewBytesPerPixel, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		Buffer->Width = NewWidth;
		Buffer->Height = NewHeight;
		Buffer->BytesPerPixel = NewBytesPerPixel;
		Buffer->Pitch = NewWidth * NewBytesPerPixel;
		
		// TODO(ivan): Diagnostics.
	}
}

static Cursor
LinuxCreateNullCursor(void) {
	Pixmap CursorMask = XCreatePixmap(LinuxState.XDisplay,
									  LinuxState.XRootWindow,
									  1, 1, 1);

	XGCValues XGC;
	XGC.function = GXclear;
	
	GC CursorGC = XCreateGC(LinuxState.XDisplay,
							CursorMask,
							GCFunction,
							&XGC);
	XFillRectangle(LinuxState.XDisplay,
				   CursorMask,
				   CursorGC,
				   0, 0, 1, 1);

	XColor DummyColor = {};
	Cursor Result = XCreatePixmapCursor(LinuxState.XDisplay,
										CursorMask, CursorMask,
										&DummyColor, &DummyColor,
										0, 0);

	XFreePixmap(LinuxState.XDisplay, CursorMask);
	XFreeGC(LinuxState.XDisplay, CursorGC);

	return Result;
}

void
PlatformQuit(s32 QuitCode) {
	LinuxState.Quitting = true;
	LinuxState.QuitCode = QuitCode;
}

s32
PlatformCheckParam(const char *Param) {
	Assert(Param);

	for (s32 Index = 0; Index < LinuxState.ArgC; Index++) {
		if (AreStringsEqual(LinuxState.ArgV[Index], Param))
			return Index;
	}

	return PARAM_MISSING;
}

const char *
PlatformCheckParamValue(const char *Param) {
	Assert(Param);

	s32 Index = PlatformCheckParam(Param);
	if (Index == PARAM_MISSING)
		return 0;
	if ((Index + 1) >= LinuxState.ArgC)
		return 0;

	return LinuxState.ArgV[Index + 1];
}

piece
PlatformReadEntireFile(const char *FileName) {
	Assert(FileName);

	piece Result = {};

	s32 File = open(FileName, O_RDONLY);
	if (File != -1) {
		off_t FileSize64 = lseek(File, 0, SEEK_END);
		lseek(File, 0, SEEK_SET);

		u32 FileSize = SafeTruncateU64(FileSize64);
		if (FileSize) {
			Result.Base = (u8 *)mmap(0, FileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (Result.Base) {
				Result.Size = FileSize;

				if (read(File, Result.Base, FileSize) == FileSize) {
					// NOTE(ivan): Success.
				} else {
					munmap(Result.Base, FileSize);
					Result.Base = 0;
				}
			}
		}

		close(File);
	}

	return Result;
}

b32
PlatformWriteEntireFile(const char *FileName, void *Base, uptr Size) {
	Assert(FileName);
	Assert(Base);
	Assert(Size);

	b32 Result = false;

	s32 File = open(FileName, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (File != -1) {
		ssize_t Written = write(File, Base, Size);
		if (fsync(File) >= 0)
			Result = (Written == (ssize_t)Size);

		close(File);
	}

	return Result;
}

void
PlatformFreeEntireFilePiece(piece *Piece) {
	Assert(Piece);
	Assert(Piece->Base);

	munmap(Piece->Base, Piece->Size);
	Piece->Size = 0;
}

#if INTERNAL
#if SLOWCODE
void
DEBUGPlatformOutf(const char *Format, ...) {
	Assert(Format);
	
	// TODO(ivan): Get rid of using CRT's va routines.
	va_list ArgsList;
	va_start(ArgsList, Format);
	
	printf("## ");
	vprintf(Format, ArgsList);
	printf("\n");
	
	va_end(ArgsList);
}

void
DEBUGPlatformOutFailedAssertion(const char *SrcFile,
								u32 SrcLine,
								const char *SrcExpr) {
	Assert(SrcFile);
	Assert(SrcExpr);

	DEBUGPlatformOutf("*** ASSERTION FAILED ***");
	DEBUGPlatformOutf("[SrcFile] %s", SrcFile);
	DEBUGPlatformOutf("[SrcLine] %d", SrcLine);
	DEBUGPlatformOutf("[SrcExpr] %s", SrcExpr);
}
#endif // #if SLOWCODE
#endif // #if INTERNAL

int
main(int ArgC, char **ArgV) {
	DEBUGPlatformOutf("Starting " GAMENAME "...");

	LinuxState.ArgC = ArgC;
	LinuxState.ArgV = ArgV;
	
	// NOTE(ivan): Obtain executable's file name and path.
	char ModuleName[2048] = {};
	readlink("/proc/self/exe", ModuleName, CountOf(ModuleName) - 1);
	char *PastLastSlash = ModuleName, *Ptr = ModuleName;
	while (*Ptr) {
		if (*Ptr == '/')
			PastLastSlash = Ptr + 1;
		Ptr++;
	}
	CopyString(LinuxState.ExecutableName, PastLastSlash);
	CopyStringN(LinuxState.ExecutablePath, ModuleName, PastLastSlash - ModuleName);

	DEBUGPlatformOutf("Executable name: %s", LinuxState.ExecutableName);
	DEBUGPlatformOutf("Executable path: %s", LinuxState.ExecutablePath);

	// NOTE(ivan): Establish connect with X.
	LinuxState.XDisplay = XOpenDisplay(getenv("DISPLAY"));
	if (LinuxState.XDisplay) {
		LinuxState.XDefScreen = DefaultScreen(LinuxState.XDisplay);
		LinuxState.XDefDepth = DefaultDepth(LinuxState.XDisplay, LinuxState.XDefScreen);
		LinuxState.XDefVisual = DefaultVisual(LinuxState.XDisplay, LinuxState.XDefScreen);
		LinuxState.XDefBlack = BlackPixel(LinuxState.XDisplay, LinuxState.XDefScreen);
		LinuxState.XDefWhite = WhitePixel(LinuxState.XDisplay, LinuxState.XDefScreen);
		LinuxState.XRootWindow = RootWindow(LinuxState.XDisplay, LinuxState.XDefScreen);
		LinuxState.XWMDeleteWindow = XInternAtom(LinuxState.XDisplay, "WM_DELETE_WINDOW", False);

		// NOTE(ivan): Check X11 MIT-SHM extension support.
		int ShmMajor, ShmMinor;
		Bool ShmPixmaps;
		if (XShmQueryVersion(LinuxState.XDisplay, &ShmMajor, &ShmMinor, &ShmPixmaps)) {
			DEBUGPlatformOutf("X MIT-SHM extension found, version %d.%d", ShmMajor, ShmMinor);
			
			// NOTE(ivan): Check X11 XKB extension support.
			int XkbMajor = XkbMajorVersion, XkbMinor = XkbMinorVersion;
			if (XkbLibraryVersion(&XkbMajor, &XkbMinor)) {
				DEBUGPlatformOutf("X XKB extension found, version compile-time(%d.%d), run-time(%d.%d)", XkbMajorVersion, XkbMinorVersion, XkbMajor, XkbMinor);
				
				// NOTE(ivan): Check X11 XRandR extension support.
				int XRRMajor, XRRMinor;
				if (XRRQueryVersion(LinuxState.XDisplay, &XRRMajor, &XRRMinor)) {
					DEBUGPlatformOutf("X XRR extension found, version %d.%d", XRRMajor, XRRMinor);
					
					// NOTE(ivan): Game memory preparation (hunk).
					uptr HunkSize = 0;
					u32 MemAvailable = sysconf(_SC_PAGE_SIZE) * sysconf(_SC_AVPHYS_PAGES);

					const char *ParamHunk = PlatformCheckParamValue("-hunk");
					if (ParamHunk) {
						sscanf(ParamHunk, "%zu", &HunkSize); // TODO(ivan): Replace CRT's sscanf() with our own function.
					} else if (MemAvailable) {
						HunkSize = (u32)(0.9f * (f32)MemAvailable);
					} else {
#if defined(__i386__)					
						HunkSize = Gigabytes(2);
#elif defined(__x86_64__) || defined(__amd64__)
						HunkSize = Gigabytes(4);
#endif
					}

					DEBUGPlatformOutf("Memory available: %dKb", MemAvailable / 1024);
					DEBUGPlatformOutf("Hunk size: %zuKb", HunkSize / 1024);
					
					GameState.Hunk.Size = HunkSize;
					GameState.Hunk.Base = (u8 *)mmap(0, HunkSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
					if (GameState.Hunk.Base) {
						// NOTE(ivan): Create main window and its graphics device.
						XSetWindowAttributes WindowAttr = {};
						WindowAttr.background_pixel = LinuxState.XDefBlack;
						WindowAttr.border_pixel = LinuxState.XDefBlack;
						WindowAttr.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;

						point WindowPos = {20, 20};
						rectangle WindowDim = {800, 600};
						Window W = XCreateWindow(LinuxState.XDisplay,
												 LinuxState.XRootWindow,
												 WindowPos.X, WindowPos.Y,
												 WindowDim.Width, WindowDim.Height, 5,
												 LinuxState.XDefDepth,
												 InputOutput,
												 LinuxState.XDefVisual,
												 CWBackPixel | CWBorderPixel | CWEventMask,
												 &WindowAttr);

						XSizeHints SizeHints;
						SizeHints.x = WindowPos.X;
						SizeHints.y = WindowPos.Y;
						SizeHints.width = WindowDim.Width;
						SizeHints.height = WindowDim.Height;
						SizeHints.flags = PPosition | PSize;
						XSetStandardProperties(LinuxState.XDisplay,
											   W,
											   GAMENAME,
											   0, None,
											   ArgV, ArgC,
											   &SizeHints);

						XWMHints WMHints;
						WMHints.input = True;
						WMHints.flags = InputHint;
						XSetWMHints(LinuxState.XDisplay,
									W, &WMHints);

						XSetWMProtocols(LinuxState.XDisplay,
										W,
										&LinuxState.XWMDeleteWindow,
										1);

						GC WindowGC = XCreateGC(LinuxState.XDisplay,
												W, 0, 0);
						XSetBackground(LinuxState.XDisplay, WindowGC, LinuxState.XDefBlack);
						XSetForeground(LinuxState.XDisplay, WindowGC, LinuxState.XDefWhite);

						// NOTE(ivan): General initial video buffer.
						WindowDim = LinuxGetWindowClientDimension(W);

						linux_video_buffer SecondaryVideoBuffer = {};
						LinuxResizeVideoBuffer(&SecondaryVideoBuffer, WindowDim.Width, WindowDim.Height);

						Bool DetectableAutoRepeat;
						XkbSetDetectableAutoRepeat(LinuxState.XDisplay, False, &DetectableAutoRepeat);
						if (DetectableAutoRepeat) {
#if INTERNAL
							b32 DebugCursor = true;
#else
							b32 DebugCursor = false;
#endif

							// NOTE(ivan): Detect display refresh rate.
							XRRScreenConfiguration *ScreenInfo = XRRGetScreenInfo(LinuxState.XDisplay,
																				  LinuxState.XRootWindow);
							u32 DisplayFrequency = (u32)XRRConfigCurrentRate(ScreenInfo);

							// NOTE(ivan): Detect controllers.
							u32 NumControllers = 0;
							u8 ControllerIDs[MAX_CONTROLLERS] = {};
							s32 ControllerFDs[MAX_CONTROLLERS] = {};
							s32 ControllerEDs[MAX_CONTROLLERS] = {};
							struct ff_effect ControllerEffect = {};
							ControllerEffect.type == FF_RUMBLE;
							ControllerEffect.u.rumble.strong_magnitude = 60000;
							ControllerEffect.u.rumble.weak_magnitude = 0;
							ControllerEffect.replay.length = 200;
							ControllerEffect.replay.delay = 0;
							ControllerEffect.id = -1; // NOTE(ivan): ID must be set -1 for every new effect.
							for (u8 Index = 0; Index < MAX_CONTROLLERS; Index++) {
								if (ControllerIDs[Index] == 0) {
									u8 ControllerID = ControllerIDs[NumControllers];
									if (ControllerFDs[ControllerID] && ControllerEDs[ControllerID]) {
										NumControllers++;
										continue;
									} else if (!ControllerFDs[ControllerID]) {
										char ControllerFileName[] = "/dev/input/jsX";
										ControllerFileName[13] = (char)(Index + 0x30);
										int File = open(ControllerFileName, O_RDONLY | O_NONBLOCK);
										if (File == -1)
											continue;
										ControllerIDs[ControllerID] = File;
									}

									if (!ControllerEDs[ControllerID]) {
										// TODO(ivan): Match the joystick file ids with the event ids.
										for (u8 TempIndex; TempIndex <= MAX_CONTROLLERS; TempIndex++) {
											char TempBuffer[] = "/sys/class/input/jsX/device/event/event/X";
											TempBuffer[19] = (char)(Index + 0x30);
											TempBuffer[40] = (char)(TempIndex + 0x30);
											if (LinuxDirectoryExists(TempBuffer)) {
												char AnotherBuffer[] = "/dev/input/eventX";
												AnotherBuffer[16] = (char)(Index + 0x30);
												int File = open(AnotherBuffer, O_RDWR);
												if (File != -1) {
													ControllerEDs[Index] = File;
													// NOTE(ivan): Send the effect to the driver.
													if (ioctl(File, EVIOCSFF, &ControllerEffect) == -1) {
														DEBUGPlatformOutf("Failed sending the effect to the game controller driver.");
														DEBUGPlatformOutf("%s: %s", AnotherBuffer, TempBuffer);
													}

													break;
												}
											}
										}
									}

									ControllerIDs[NumControllers++] = Index;
								} else {
									NumControllers++;
								}
							}
							
							GameUpdate(GameUpdateType_Prepare);
					
							// NOTE(ivan): Present main window after all initialization is done.
							XMapRaised(LinuxState.XDisplay, W);
							XFlush(LinuxState.XDisplay);

							struct timespec LastCounter = LinuxGetClock();
							u64 LastCycleCounter = LinuxGetRDTSC();
							
							// NOTE(ivan): Primary loop.
							while (!LinuxState.Quitting) {							
								// NOTE(ivan): Process X11 messages.
								static XEvent Ev;
								while (XPending(LinuxState.XDisplay)) {
									XNextEvent(LinuxState.XDisplay, &Ev);
									if (Ev.xany.window == W) {
										switch (Ev.type) {
										case ClientMessage: {
											if (Ev.xclient.data.l[0] == LinuxState.XWMDeleteWindow)
												PlatformQuit(0);
										} break;

										case ConfigureNotify: {
											if (Ev.xconfigure.width != SecondaryVideoBuffer.Width &&
												Ev.xconfigure.height != SecondaryVideoBuffer.Height) {
												WindowDim = LinuxGetWindowClientDimension(W);

												if (SecondaryVideoBuffer.Pixels) {
													CopyBytes(SecondaryVideoBuffer.Image->data,
															  SecondaryVideoBuffer.Pixels,
															  SecondaryVideoBuffer.Width * SecondaryVideoBuffer.Height * SecondaryVideoBuffer.BytesPerPixel);
													XShmPutImage(LinuxState.XDisplay,
																 W, WindowGC,
																 SecondaryVideoBuffer.Image,
																 0, 0, 0, 0,
																 SecondaryVideoBuffer.Width, SecondaryVideoBuffer.Height,
																 False);
												}
											}
										} break;

										case KeyPress:
										case KeyRelease: {
											KeySym XKeySym = XLookupKeysym(&Ev.xkey, 0);

											key_code KeyCode;
											if (LinuxMapXKeySymToKeyCode(XKeySym, &KeyCode))
												LinuxProcessKeyboardOrMouseButton(&GameState.KeyboardButtons[KeyCode], Ev.type == KeyPress);
										} break;

										case ButtonPress:
										case ButtonRelease: {
											b32 IsKeyPress = (Ev.type == ButtonPress);
											switch (Ev.xbutton.button) {
											case Button1: {
												LinuxProcessKeyboardOrMouseButton(&GameState.MouseButtons[0], IsKeyPress);
											} break;

											case Button2: {
												LinuxProcessKeyboardOrMouseButton(&GameState.MouseButtons[1], IsKeyPress);
											} break;

											case Button3: {
												LinuxProcessKeyboardOrMouseButton(&GameState.MouseButtons[2], IsKeyPress);
											} break;

											case Button4: {
												GameState.MouseWheel++;
											} break;

											case Button5: {
												GameState.MouseWheel--;
											} break;
											}
										} break;
										}
									}

									// NOTE(ivan): Capture mouse pointer position.
									Window RootIgnore, ChildIgnore;
									s32 RootXIgnore, RootYIgnore;
									s32 WinX, WinY;
									u32 MaskIgnore;
									XQueryPointer(LinuxState.XDisplay, W,
												  &RootIgnore, &ChildIgnore,
												  &RootXIgnore, &RootYIgnore,
												  &WinX, &WinY,
												  &MaskIgnore);
									GameState.MousePos.X = WinX;
									GameState.MousePos.Y = WinY;

									// Process Xbox controllers input.
									if (NumControllers) {
										if (NumControllers > CountOf(GameState.XboxControllers))
											NumControllers = CountOf(GameState.XboxControllers);

										for (u8 Index = 0; Index < NumControllers; Index++) {
											u8 ControllerID = ControllerIDs[Index];
											game_input_xbox_controller *Controller = &GameState.XboxControllers[Index];

											Controller->IsConnected = true;

											static struct js_event ControllerEvent;
											while (read(ControllerFDs[Index], &ControllerEvent, sizeof(ControllerEvent)) > 0) {
												if (ControllerEvent.type >= JS_EVENT_INIT)
													ControllerEvent.type -= JS_EVENT_INIT;

												if (ControllerEvent.type == JS_EVENT_BUTTON) {
													if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_A) {
														LinuxProcessXboxDigitalButton(&Controller->A, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_B) {
														LinuxProcessXboxDigitalButton(&Controller->B, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_X) {
														LinuxProcessXboxDigitalButton(&Controller->X, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_Y) {
														LinuxProcessXboxDigitalButton(&Controller->Y, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_START) {
														LinuxProcessXboxDigitalButton(&Controller->Start, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_BACK) {
														LinuxProcessXboxDigitalButton(&Controller->Back, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_LEFT_SHOULDER) {
														LinuxProcessXboxDigitalButton(&Controller->LeftBumper, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_RIGHT_SHOULDER) {
														LinuxProcessXboxDigitalButton(&Controller->RightBumper, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_LEFT_THUMB) {
														LinuxProcessXboxDigitalButton(&Controller->LeftStick, ControllerEvent.value);
													} else if (ControllerEvent.number = XBOX_CONTROLLER_BUTTON_RIGHT_THUMB) {
														LinuxProcessXboxDigitalButton(&Controller->RightStick, ControllerEvent.value);
													}
												} else if (ControllerEvent.type == JS_EVENT_AXIS) {
													if (ControllerEvent.number == XBOX_CONTROLLER_AXIS_LEFT_THUMB_X) {
														Controller->LeftStickPos.X = LinuxProcessXboxStickValue(ControllerEvent.value, XBOX_CONTROLLER_DEADZONE);
													} else if (ControllerEvent.number == XBOX_CONTROLLER_AXIS_LEFT_THUMB_Y) {
														Controller->LeftStickPos.Y = LinuxProcessXboxStickValue(ControllerEvent.value, XBOX_CONTROLLER_DEADZONE);
													} if (ControllerEvent.number == XBOX_CONTROLLER_AXIS_RIGHT_THUMB_X) {
														Controller->RightStickPos.X = LinuxProcessXboxStickValue(ControllerEvent.value, XBOX_CONTROLLER_DEADZONE);
													} else if (ControllerEvent.number == XBOX_CONTROLLER_AXIS_RIGHT_THUMB_Y) {
														Controller->RightStickPos.Y = LinuxProcessXboxStickValue(ControllerEvent.value, XBOX_CONTROLLER_DEADZONE);
													} else if (ControllerEvent.number == XBOX_CONTROLLER_AXIS_DPAD_HORZ) {
														if (ControllerEvent.value == -32767) {
															LinuxProcessXboxDigitalButton(&Controller->Left, 1);
															LinuxProcessXboxDigitalButton(&Controller->Right, 0);
														} else if (ControllerEvent.value == 32768) {
															LinuxProcessXboxDigitalButton(&Controller->Left, 0);
															LinuxProcessXboxDigitalButton(&Controller->Right, 1);
														} else {
															LinuxProcessXboxDigitalButton(&Controller->Left, 0);
															LinuxProcessXboxDigitalButton(&Controller->Right, 0);
														}
													} else if (ControllerEvent.number = XBOX_CONTROLLER_AXIS_DPAD_VERT) {
														if (ControllerEvent.value == -32767) {
															LinuxProcessXboxDigitalButton(&Controller->Up, 1);
															LinuxProcessXboxDigitalButton(&Controller->Down, 0);
														} else if (ControllerEvent.value == 32768) {
															LinuxProcessXboxDigitalButton(&Controller->Up, 0);
															LinuxProcessXboxDigitalButton(&Controller->Down, 1);
														} else {
															LinuxProcessXboxDigitalButton(&Controller->Up, 0);
															LinuxProcessXboxDigitalButton(&Controller->Down, 0);
														}
													}
												}
											}
										}
									}

									// NOTE(ivan): Process linux-side input events.
									if (GameState.KeyboardButtons[KeyCode_F4].IsDown &&
										(GameState.KeyboardButtons[KeyCode_LeftAlt].IsDown || GameState.KeyboardButtons[KeyCode_RightAlt].IsDown))
										PlatformQuit(0);
#if INTERNAL
									if (IsNewlyPressed(&GameState.KeyboardButtons[KeyCode_F2]))
										DebugCursor = !DebugCursor;
#endif

									// NOTE(ivan): Set debug cursor.
									if (DebugCursor)
										XUndefineCursor(LinuxState.XDisplay, W);
									else
										XDefineCursor(LinuxState.XDisplay, W, LinuxCreateNullCursor());

									// NOTE(ivan): Prepare game video buffer.
									GameState.VideoBuffer.Pixels = SecondaryVideoBuffer.Pixels;
									GameState.VideoBuffer.Width = SecondaryVideoBuffer.Width;
									GameState.VideoBuffer.Height = SecondaryVideoBuffer.Height;
									GameState.VideoBuffer.BytesPerPixel = SecondaryVideoBuffer.BytesPerPixel;
									GameState.VideoBuffer.Pitch = SecondaryVideoBuffer.Pitch;

									GameUpdate(GameUpdateType_Frame);

									// NOTE(ivan): Before the next frame, reset the mouse wheel.
									GameState.MouseWheel = 0;
						
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

									// NOTE(ivan): Finish timings.
									struct timespec WorkCounter = LinuxGetClock();

									f64 TargetSecondsPerFrame = (f64)(1.0 / DisplayFrequency);
									f64 SecondsElapsedForWork = LinuxGetSecondsElapsed(LastCounter, WorkCounter);
									GameState.SecondsPerFrame = SecondsElapsedForWork;
									GameState.FramesPerSecond = 0; // TODO(ivan): Implement me!
									
									if (SecondsElapsedForWork < TargetSecondsPerFrame) {
										f64 SecondsToSleep = TargetSecondsPerFrame - SecondsElapsedForWork;
										u32 SleepMS = (u32)(1000 * SecondsToSleep);
										if (SleepMS > 0) {
											usleep(SleepMS);
											SecondsElapsedForWork += SecondsToSleep;
										}
									} else {
										// NOTE(ivan): Missing framerate!
									}

									struct timespec EndCounter = LinuxGetClock();
									u64 EndCycleCounter = LinuxGetRDTSC();

									GameState.CyclesPerFrame = ((f64)(EndCycleCounter - LastCycleCounter) / (1000.0 * 1000.0));
		
									LastCounter = EndCounter;
									LastCycleCounter = EndCycleCounter;
								}

								GameUpdate(GameUpdateType_Release);
							}
						} else {
							DEBUGPlatformOutf("XkbSetDetectanbleAutoRepeat() failed!");
						}

						munmap(GameState.Hunk.Base, GameState.Hunk.Size);
					} else {
						DEBUGPlatformOutf("Could not allocate enough hunk memory!");
					}
				} else {
					DEBUGPlatformOutf("XRR is not available!");
				}
			} else {
				DEBUGPlatformOutf("XKB is not available!");
			}
		} else {
			DEBUGPlatformOutf("MIT-SHM is not available!");
		}

		XCloseDisplay(LinuxState.XDisplay);
	} else {
		DEBUGPlatformOutf("X not responding!");
	}
	
	return LinuxState.QuitCode;
}
