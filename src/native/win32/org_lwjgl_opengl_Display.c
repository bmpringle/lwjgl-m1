/* 
 * Copyright (c) 2002-2004 LWJGL Project
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * * Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of 'LWJGL' nor the names of 
 *   its contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/**
 * $Id$
 *
 * Base Win32 display
 *
 * @author cix_foo <cix_foo@users.sourceforge.net>
 * @version $Revision$
 */

#define _PRIVATE_WINDOW_H_
#include "Window.h"
#include <windowsx.h>
#include "extgl_wgl.h"
#include "common_tools.h"
#include "extgl_wgl.h"
#include "display.h"
#include "org_lwjgl_opengl_Win32Display.h"
#include <malloc.h>

static bool				oneShotInitialised = false;     // Registers the LWJGL window class

static HWND       display_hwnd = NULL;            // Handle to the window
static HDC        display_hdc = NULL;             // Device context
static HGLRC      display_hglrc = NULL;           // OpenGL context
static bool				isFullScreen = false;           // Whether we're fullscreen or not
static bool				isMinimized = false;            // Whether we're minimized or not
static bool       isFocused = false;              // whether we're focused or not
static bool       isDirty = false;                // Whether we're dirty or not
static bool       isUndecorated = false;          // Whether we're undecorated or not
extern HINSTANCE	dll_handle;                     // Handle to the LWJGL dll
RECT clientSize;                                  // client size rect used when creating and positioning window

static bool closerequested;
static int pixel_format_index;

#define WINDOWCLASSNAME "LWJGL"

bool applyPixelFormat(HDC hdc, int iPixelFormat) {
	PIXELFORMATDESCRIPTOR desc;
	if (DescribePixelFormat(hdc, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &desc) == 0) {
		return false;
	}

	// make that the pixel format of the device context 
	if (SetPixelFormat(hdc, iPixelFormat, &desc) == FALSE) {
		return false;
	}
	return true;
}

HWND getCurrentHWND() {
	return display_hwnd;
}

HGLRC getCurrentContext() {
	return display_hglrc;
}

static int findPixelFormatARBFromBPP(JNIEnv *env, HDC hdc, jobject pixel_format, jobject pixelFormatCaps, int bpp, bool window, bool pbuffer, bool double_buffer) {
        jclass cls_pixel_format = (*env)->GetObjectClass(env, pixel_format);
        int alpha = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "alpha", "I"));
        int depth = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "depth", "I"));
        int stencil = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "stencil", "I"));
        int samples = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "samples", "I"));
        int num_aux_buffers = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "num_aux_buffers", "I"));
        int accum_bpp = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "accum_bpp", "I"));
        int accum_alpha = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "accum_alpha", "I"));
        jboolean stereo = (*env)->GetBooleanField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "stereo", "Z"));
	int iPixelFormat;
	unsigned int num_formats_returned;
	attrib_list_t attrib_list;
        GLuint *pixelFormatCaps_ptr;
        jlong pixelFormatCapsSize;
        BOOL result;
        jlong i;

	initAttribList(&attrib_list);
	if (window) {
		putAttrib(&attrib_list, WGL_DRAW_TO_WINDOW_ARB); putAttrib(&attrib_list, TRUE);
	} 
	if (pbuffer) {
		putAttrib(&attrib_list, WGL_DRAW_TO_PBUFFER_ARB); putAttrib(&attrib_list, TRUE);
	}
	putAttrib(&attrib_list, WGL_ACCELERATION_ARB); putAttrib(&attrib_list, WGL_FULL_ACCELERATION_ARB);
	putAttrib(&attrib_list, WGL_PIXEL_TYPE_ARB); putAttrib(&attrib_list, WGL_TYPE_RGBA_ARB);
	putAttrib(&attrib_list, WGL_DOUBLE_BUFFER_ARB); putAttrib(&attrib_list, double_buffer ? TRUE : FALSE);
	putAttrib(&attrib_list, WGL_SUPPORT_OPENGL_ARB); putAttrib(&attrib_list, TRUE);
	putAttrib(&attrib_list, WGL_COLOR_BITS_ARB); putAttrib(&attrib_list, bpp);
	putAttrib(&attrib_list, WGL_ALPHA_BITS_ARB); putAttrib(&attrib_list, alpha);
	putAttrib(&attrib_list, WGL_DEPTH_BITS_ARB); putAttrib(&attrib_list, depth);
	putAttrib(&attrib_list, WGL_STENCIL_BITS_ARB); putAttrib(&attrib_list, stencil);
	if (samples > 0 && extgl_Extensions.WGL_ARB_multisample) {
		putAttrib(&attrib_list, WGL_SAMPLE_BUFFERS_ARB); putAttrib(&attrib_list, 1);
		putAttrib(&attrib_list, WGL_SAMPLES_ARB); putAttrib(&attrib_list, samples);
	}
	putAttrib(&attrib_list, WGL_ACCUM_BITS_ARB); putAttrib(&attrib_list, accum_bpp);
	putAttrib(&attrib_list, WGL_ACCUM_ALPHA_BITS_ARB); putAttrib(&attrib_list, accum_alpha);
	putAttrib(&attrib_list, WGL_STEREO_ARB); putAttrib(&attrib_list, stereo ? TRUE : FALSE);
	putAttrib(&attrib_list, WGL_AUX_BUFFERS_ARB); putAttrib(&attrib_list, num_aux_buffers);
	if ( pixelFormatCaps != NULL ) {
		if ( !extgl_Extensions.WGL_ARB_render_texture ) {
			return -1;
		}

                pixelFormatCaps_ptr = (GLuint *)(*env)->GetDirectBufferAddress(env, pixelFormatCaps);
                pixelFormatCapsSize = (*env)->GetDirectBufferCapacity(env, pixelFormatCaps);

                for (i = 0; i < pixelFormatCapsSize; i++)
			putAttrib(&attrib_list, pixelFormatCaps_ptr[i]);
	}
	putAttrib(&attrib_list, 0); putAttrib(&attrib_list, 0);
        result = wglChoosePixelFormatARB(hdc, attrib_list.attribs, NULL, 1, &iPixelFormat, &num_formats_returned);

	if (result == FALSE || num_formats_returned < 1) {
		return -1;
	}
	return iPixelFormat;
}

int findPixelFormatARB(JNIEnv *env, HDC hdc, jobject pixel_format, jobject pixelFormatCaps, bool use_hdc_bpp, bool window, bool pbuffer, bool double_buffer) {
	int bpp;
        int iPixelFormat;
        jclass cls_pixel_format = (*env)->GetObjectClass(env, pixel_format);
	if (use_hdc_bpp) {
		bpp = GetDeviceCaps(hdc, BITSPIXEL);
                iPixelFormat = findPixelFormatARBFromBPP(env, hdc, pixel_format, pixelFormatCaps, bpp, window, pbuffer, double_buffer);
		if (iPixelFormat == -1)
			bpp = 16;
		else
			return iPixelFormat;
	} else
                bpp = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "bpp", "I"));
	return findPixelFormatARBFromBPP(env, hdc, pixel_format, pixelFormatCaps, bpp, window, pbuffer, double_buffer);
}

/*
 * Find an appropriate pixel format
 */
static int findPixelFormatFromBPP(JNIEnv *env, HDC hdc, jobject pixel_format, int bpp)
{
        jclass cls_pixel_format = (*env)->GetObjectClass(env, pixel_format);
        int alpha = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "alpha", "I"));
        int depth = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "depth", "I"));
        int stencil = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "stencil", "I"));
        int samples = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "samples", "I"));
        int num_aux_buffers = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "num_aux_buffers", "I"));
        int accum_bpp = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "accum_bpp", "I"));
        int accum_alpha = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "accum_alpha", "I"));
        jboolean stereo = (*env)->GetBooleanField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "stereo", "Z"));
	unsigned int flags = PFD_DRAW_TO_WINDOW |   // support window 
                PFD_SUPPORT_OPENGL |
                PFD_DOUBLEBUFFER |
                (stereo ? PFD_STEREO : 0);
	PIXELFORMATDESCRIPTOR desc;
        int iPixelFormat;
        PIXELFORMATDESCRIPTOR pfd = { 
		sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd 
		1,                     // version number 
		flags,         // RGBA type 
		PFD_TYPE_RGBA,
		(BYTE)bpp,       
		0, 0, 0, 0, 0, 0,      // color bits ignored 
		(BYTE)alpha,       
		0,                     // shift bit ignored 
		accum_bpp + accum_alpha,                     // no accumulation buffer 
		0, 0, 0, 0,            // accum bits ignored 
		(BYTE)depth,       
		(BYTE)stencil,     
		num_aux_buffers, 
		PFD_MAIN_PLANE,        // main layer
		0,                     // reserved 
		0, 0, 0                // layer masks ignored
	};

	// get the best available match of pixel format for the device context  
        iPixelFormat = ChoosePixelFormat(hdc, &pfd);
	if (iPixelFormat == 0) {
		throwException(env, "Failed to choose pixel format");
		return -1;
	}

	if (DescribePixelFormat(hdc, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &desc) == 0) {
		throwException(env, "Could not describe pixel format");
		return -1;
	}

	if (desc.cColorBits < bpp) {
		throwException(env, "This application requires a greater colour depth");
		return -1;
	}

	if (desc.cAlphaBits < alpha) {
		throwException(env, "This application requires a greater alpha depth");
		return -1;
	}

	if (desc.cStencilBits < stencil) {
		throwException(env, "This application requires a greater stencil depth");
		return -1;
	}

	if (desc.cDepthBits < depth) {
		throwException(env, "This application requires a greater depth buffer depth");
		return -1;
	}

	if ((desc.dwFlags & PFD_GENERIC_FORMAT) != 0 || (desc.dwFlags & PFD_GENERIC_ACCELERATED) != 0) {
		jboolean allowSoftwareOpenGL = getBooleanProperty(env, "org.lwjgl.opengl.Window.allowSoftwareOpenGL");
		// secondary check for software override
		if(!allowSoftwareOpenGL) {
			throwException(env, "Mode not supported by hardware");
			return -1;
		}
	}

	if ((desc.dwFlags & flags) != flags) {
		throwException(env, "Capabilities not supported");
		return -1;
	}

	return iPixelFormat;
}

int findPixelFormat(JNIEnv *env, HDC hdc, jobject pixel_format) {
        int bpp = GetDeviceCaps(hdc, BITSPIXEL);
        jclass cls_pixel_format = (*env)->GetObjectClass(env, pixel_format);
	int iPixelFormat = findPixelFormatFromBPP(env, hdc, pixel_format, bpp);
	if (iPixelFormat == -1) {
		return findPixelFormatFromBPP(env, hdc, pixel_format, 16);
	} else
		return iPixelFormat;
}

/*
 * Close the window
 */
void closeWindow(HWND hwnd, HDC hdc)
{
	// Release device context
	if (hdc != NULL && hwnd != NULL) {
		printfDebug("Releasing DC\n");
		ReleaseDC(hwnd, hdc);
		hdc = NULL;
	}

	// Close the window
	if (hwnd != NULL) {
		ShowWindow(hwnd, SW_HIDE);
		printfDebug("Destroy window\n");
		DestroyWindow(hwnd);
		hwnd = NULL;
	}
}

/*
 * Called when the application is alt-tabbed to or from
 */
static void appActivate(bool active)
{
	static bool inAppActivate = false;

	if (inAppActivate) {
		return;
	}
	inAppActivate = true;
	if (active) {
		if (isFullScreen) {
			restoreDisplayMode();
		}
		ShowWindow(display_hwnd, SW_RESTORE);
		SetForegroundWindow(display_hwnd);
	} else if (isFullScreen) {
		ShowWindow(display_hwnd, SW_MINIMIZE);
		resetDisplayMode(NULL);
	}
	inAppActivate = false;
}

/*
 *	WindowProc for the GL window.
 */
LRESULT CALLBACK lwjglWindowProc(HWND hWnd,
							     UINT msg,
							     WPARAM wParam,
							     LPARAM lParam)
{
        int xPos; 
        int yPos;
        int dwheel;
        bool oldIsMinimized;
        bool oldIsFocused;
	switch (msg) {
		// disable screen saver and monitor power down messages which wreak havoc
		case WM_SYSCOMMAND:
		{
			switch (wParam) {
			case SC_SCREENSAVE:
			case SC_MONITORPOWER:
				return 0L;
				break;
			case SC_CLOSE:
				closerequested = true;
				//don't continue processing this command since this 
				//would shutdown the window, which the application might not want to
				return 0L;
			}
		}
		break;
		case WM_MOUSEMOVE:
		{
                        xPos = GET_X_LPARAM(lParam); 
                        yPos = GET_Y_LPARAM(lParam);
			handleMouseMoved(xPos, yPos);
			return 0;
		}
		case WM_MOUSEWHEEL:
		{
                        dwheel = GET_WHEEL_DELTA_WPARAM(wParam);
			handleMouseScrolled(dwheel);
			return 0;
		}
		case WM_LBUTTONDOWN:
		{
			handleMouseButton(0, 1);
			return 0;
		}
		case WM_LBUTTONUP:
		{
			handleMouseButton(0, 0);
			return 0;
		}
		case WM_RBUTTONDOWN:
		{
			handleMouseButton(1, 1);
			return 0;
		}
		case WM_RBUTTONUP:
		{
			handleMouseButton(1, 0);
			return 0;
		}
		case WM_MBUTTONDOWN:
		{
			handleMouseButton(2, 1);
			return 0;
		}
		case WM_MBUTTONUP:
		{
			handleMouseButton(2, 0);
			return 0;
		}
		break;
		case WM_QUIT:
		{
			closerequested = true;
			return 0L;
		}
		case WM_PAINT:
		{
			isDirty = true;
		}
    /*case WM_MOVE:  {
      // get fields of display
            jclass cls_display = (*env)->FindClass(env, "org/lwjgl/opengl/Display");
            jfieldID fid_x = (*env)->GetStaticFieldID(env, cls_display, "x", "I");
            jfieldID fid_y = (*env)->GetStaticFieldID(env, cls_display, "y", "I");
	    
	    // set fields
            (*env)->SetStaticIntField(env, cls_display, fid_x, (int)(short) LOWORD(lParam));
            (*env)->SetStaticIntField(env, cls_display, fid_y, (int)(short) HIWORD(lParam));
	    
    }*/
	}

	// Update window state directly having processed window messages
        oldIsMinimized = isMinimized;
        oldIsFocused = isFocused;
	isMinimized = IsIconic(display_hwnd); 
	isFocused = GetForegroundWindow() == display_hwnd;
	if (oldIsMinimized != isMinimized || oldIsFocused != isFocused) {
		appActivate(isFocused && !isMinimized);
	}

	// default action
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/*
 * Register the LWJGL window class.
 * Returns true for success, or false for failure
 */
static bool registerWindow()
{
        WNDCLASS windowClass;
	if (!oneShotInitialised) {
		windowClass.style = CS_GLOBALCLASS | CS_OWNDC;
		windowClass.lpfnWndProc = lwjglWindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = dll_handle;
		windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		windowClass.lpszMenuName = NULL;
		windowClass.lpszClassName = WINDOWCLASSNAME;

		if (RegisterClass(&windowClass) == 0) {
			printfDebug("Failed to register window class\n");
			return false;
		}
		printfDebug("Window registered\n");
		oneShotInitialised = true;
	}

	return true;
}

/*
 * Handle native Win32 messages
 */
void handleMessages(void)
{
	/*
	 * Now's our chance to deal with Windows messages that are
	 * otherwise just piling up and causing everything not to
	 * work properly
	 */
	MSG msg;
	while (PeekMessage(
		&msg,         // message information
		display_hwnd,           // handle to window
		0,  // first message
		0,  // last message
		PM_REMOVE      // removal options
		))
	{
      	DispatchMessage(&msg);
	};
}

/*
 * Create a window with the specified title, position, size, and
 * fullscreen attribute. The window will have DirectInput associated
 * with it.
 * 
 * Returns true for success, or false for failure
 */
HWND createWindow(int x, int y, int width, int height, bool fullscreen, bool undecorated)
{
	int exstyle, windowflags;
        HWND new_hwnd;
	// 1. Register window class if necessary
	if (!registerWindow()) {
		return NULL;
	}

	if (fullscreen) {
		exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
		windowflags = WS_POPUP;
	} else if (undecorated) {
		exstyle = WS_EX_APPWINDOW;
		windowflags = WS_POPUP;
	} else {
		exstyle = WS_EX_APPWINDOW;
		windowflags = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_MINIMIZEBOX | WS_SYSMENU;
	}

	// If we're not a fullscreen window, adjust the height to account for the
	// height of the title bar (unless undecorated)
	clientSize.bottom = height;
	clientSize.left = 0;
	clientSize.right = width;
	clientSize.top = 0;
	
	AdjustWindowRectEx(
	  &clientSize,    // client-rectangle structure
	  windowflags,    // window styles
	  FALSE,       // menu-present option
	  exstyle   // extended window style
	);

	// Create the window now, using that class:
        new_hwnd = CreateWindowEx (
		 exstyle, 
		 WINDOWCLASSNAME,
		 "",
		 windowflags,
		 x, y, clientSize.right - clientSize.left, clientSize.bottom - clientSize.top,
		 NULL,
		 NULL,
		 dll_handle,
		 NULL);

	return new_hwnd;
}

/*
 * Class:     org_lwjgl_Window
 * Method:    nSetTitle
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_setTitle
  (JNIEnv * env, jobject self, jstring title_obj)
{
	char * title = GetStringNativeChars(env, title_obj);
	SetWindowText(display_hwnd, title);
	free(title);
}

/*
 * Class:     org_lwjgl_Window
 * Method:    update
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_update
  (JNIEnv * env, jobject self)
{
	handleMessages();
}


/*
 * Class:     org_lwjgl_Window
 * Method:    swapBuffers
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_swapBuffers
  (JNIEnv * env, jobject self)
{
	isDirty = false;
	SwapBuffers(display_hdc);
}

/*
 * Class:     org_lwjgl_opengl_Window
 * Method:    nIsDirty
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_lwjgl_opengl_Win32Display_isDirty
  (JNIEnv *env, jobject self) {
	bool result = isDirty;
	isDirty = false;
	return result ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_lwjgl_opengl_Window
 * Method:    nIsVisible
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_lwjgl_opengl_Win32Display_isVisible
  (JNIEnv *env, jobject self) {
	return isMinimized ? JNI_FALSE : JNI_TRUE;
}

/*
 * Class:     org_lwjgl_opengl_Window
 * Method:    nIsCloseRequested
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_lwjgl_opengl_Win32Display_isCloseRequested
  (JNIEnv *env, jobject self) {
	bool saved = closerequested;
	closerequested = false;
	return saved;
}

/*
 * Class:     org_lwjgl_opengl_Window
 * Method:    nIsActive
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_lwjgl_opengl_Win32Display_isActive
  (JNIEnv *env, jobject self) {
	return isFocused;
}

/*
 * Class:     org_lwjgl_opengl_Window
 * Method:    nSetVSyncEnabled
 * Signature: (Z)Z
 */
JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_setVSyncEnabled
  (JNIEnv * env, jobject self, jboolean sync)
{
	if (extgl_Extensions.WGL_EXT_swap_control) {
		if (sync == JNI_TRUE) {
			wglSwapIntervalEXT(1);
		} else {
			wglSwapIntervalEXT(0);
		}
	}
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_makeCurrent
  (JNIEnv *env, jobject self)
{
	BOOL result = wglMakeCurrent(display_hdc, display_hglrc);
	if (!result)
		throwException(env, "Could not make display context current");
}

JNIEXPORT jobjectArray JNICALL Java_org_lwjgl_opengl_Win32Display_getAvailableDisplayModes(JNIEnv *env, jobject self) {
	return getAvailableDisplayModes(env);
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_createWindow(JNIEnv *env, jobject self, jobject mode, jboolean fullscreen, jint x, jint y) {
        jclass cls_displayMode = (*env)->GetObjectClass(env, mode);
        jfieldID fid_width = (*env)->GetFieldID(env, cls_displayMode, "width", "I");
        jfieldID fid_height = (*env)->GetFieldID(env, cls_displayMode, "height", "I");
        int width = (*env)->GetIntField(env, mode, fid_width);
        int height = (*env)->GetIntField(env, mode, fid_height);
        BOOL result;
	closerequested = false;
	isMinimized = false;
	isFocused = false;
	isDirty = true;
	isFullScreen = fullscreen == JNI_TRUE;
	isUndecorated = getBooleanProperty(env, "org.lwjgl.opengl.Window.undecorated");

	display_hwnd = createWindow(x, y, width, height, isFullScreen, isUndecorated);
	if (display_hwnd == NULL) {
		throwException(env, "Failed to create the window.");
		return;
	}
	display_hdc = GetDC(display_hwnd);
	if (!applyPixelFormat(display_hdc, pixel_format_index)) {
		closeWindow(display_hwnd, display_hdc);
		throwException(env, "Could not apply pixel format to window");
		return;
	}

        result = wglMakeCurrent(display_hdc, display_hglrc);
	if (!result) {
		throwException(env, "Could not bind context to window");
		closeWindow(display_hwnd, display_hdc);
		return;
	}
	extgl_InitWGL(env);
	ShowWindow(display_hwnd, SW_SHOW);
	UpdateWindow(display_hwnd);
	SetForegroundWindow(display_hwnd);
	SetFocus(display_hwnd);
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_destroyWindow(JNIEnv *env, jobject self) {
	closeWindow(display_hwnd, display_hdc);
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_switchDisplayMode(JNIEnv *env, jobject self, jobject mode) {
	switchDisplayMode(env, mode);
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_resetDisplayMode(JNIEnv *env, jobject self) {
	resetDisplayMode(env);
}

JNIEXPORT jint JNICALL Java_org_lwjgl_opengl_Win32Display_getGammaRampLength(JNIEnv *env, jobject self) {
	return getGammaRampLength();
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_setGammaRamp(JNIEnv *env, jobject self, jobject gamma_buffer) {
	setGammaRamp(env, gamma_buffer);
}

JNIEXPORT jstring JNICALL Java_org_lwjgl_opengl_Win32Display_getAdapter(JNIEnv *env, jobject self) {
	return getAdapter(env);
}

JNIEXPORT jstring JNICALL Java_org_lwjgl_opengl_Win32Display_getVersion(JNIEnv *env, jobject self) {
	return getVersion(env);
}

JNIEXPORT jobject JNICALL Java_org_lwjgl_opengl_Win32Display_init(JNIEnv *env, jobject self) {
	return initDisplay(env);
}

static bool createARBContextAndPixelFormat(JNIEnv *env, HDC hdc, jobject pixel_format, int *pixel_format_index_return, HGLRC *context_return) {
        int pixel_format_index;
        HWND arb_hwnd;
        HDC arb_hdc;
        HGLRC arb_context;
	// Some crazy strangeness here so we can use ARB_pixel_format to specify the number
	// of multisamples we want. If the extension is present we'll delete the existing
	// rendering context and start over, using the ARB extension instead to pick the context.
	if (!extgl_Extensions.WGL_ARB_pixel_format)
		return false;
        pixel_format_index = findPixelFormatARB(env, hdc, pixel_format, NULL, true, true, true, true);
	if (pixel_format_index == -1) {
		pixel_format_index = findPixelFormatARB(env, hdc, pixel_format, NULL, true, true, false, true);		
		if (pixel_format_index == -1)
			return false;
	}
        arb_hwnd = createWindow(0, 0, 1, 1, false, false);
	if (arb_hwnd == NULL)
		return false;
        arb_hdc = GetDC(arb_hwnd);
	if (!applyPixelFormat(arb_hdc, pixel_format_index)) {
		closeWindow(arb_hwnd, arb_hdc);
		return false;
	}	
        arb_context = wglCreateContext(arb_hdc);
	closeWindow(arb_hwnd, arb_hdc);
	*pixel_format_index_return = pixel_format_index;
	*context_return = arb_context;
	return true;
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_createContext(JNIEnv *env, jobject self, jobject pixel_format) {
	HWND dummy_hwnd = createWindow(0, 0, 1, 1, false, false);
        HDC dummy_hdc;
        BOOL result;
        jclass cls_pixel_format;
        int samples;
        int pixel_format_index_arb;
        HGLRC context_arb;
        bool arb_success;
	if (dummy_hwnd == NULL) {
		throwException(env, "Failed to create the window.");
		return;
	}
        dummy_hdc = GetDC(dummy_hwnd);
	pixel_format_index = findPixelFormat(env, dummy_hdc, pixel_format);
	if (pixel_format_index == -1) {
		return;
	}
	if (!applyPixelFormat(dummy_hdc, pixel_format_index)) {
		closeWindow(dummy_hwnd, dummy_hdc);
		throwException(env, "Could not apply pixel format to window");
		return;
	}
	display_hglrc = wglCreateContext(dummy_hdc);
	if (display_hglrc == NULL) {
		closeWindow(dummy_hwnd, dummy_hdc);
		throwException(env, "Failed to create OpenGL rendering context");
		return;
	}
        result = wglMakeCurrent(dummy_hdc, display_hglrc);
	if (!result) {
		throwException(env, "Could not bind context to dummy window");
		wglDeleteContext(display_hglrc);
		closeWindow(dummy_hwnd, dummy_hdc);
		return;
	}
	extgl_InitWGL(env);
        cls_pixel_format = (*env)->GetObjectClass(env, pixel_format);
        samples = (int)(*env)->GetIntField(env, pixel_format, (*env)->GetFieldID(env, cls_pixel_format, "samples", "I"));
	if (samples > 0) {
                arb_success = createARBContextAndPixelFormat(env, dummy_hdc, pixel_format, &pixel_format_index_arb, &context_arb);
		closeWindow(dummy_hwnd, dummy_hdc);
		wglDeleteContext(display_hglrc);
		if (!arb_success) {
			throwException(env, "Samples > 0 but could not find a suitable ARB pixel format");
			return;
		}
		display_hglrc = context_arb;
		pixel_format_index = pixel_format_index_arb;
	} else
		closeWindow(dummy_hwnd, dummy_hdc);
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_destroyContext(JNIEnv *env, jobject self) {
	wglMakeCurrent(NULL, NULL);

	// Delete the rendering context
	if (display_hglrc != NULL) {
		printfDebug("Deleting GL context\n");
		wglDeleteContext(display_hglrc);
		display_hglrc = NULL;
	}
}

JNIEXPORT void JNICALL Java_org_lwjgl_opengl_Win32Display_reshape(JNIEnv *env, jobject self, jint x, jint y, jint width, jint height) {
	int exstyle, windowflags;

	if (isFullScreen) {
		return;
	}

	if (isFullScreen) {
		exstyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;
		windowflags = WS_POPUP;
	} else if (getBooleanProperty(env, "org.lwjgl.opengl.Window.undecorated")) {
		exstyle = WS_EX_APPWINDOW;
		windowflags = WS_POPUP;
	} else {
		exstyle = WS_EX_APPWINDOW;
		windowflags = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_MINIMIZEBOX | WS_SYSMENU;
	}
	
	// If we're not a fullscreen window, adjust the height to account for the
	// height of the title bar:
	clientSize.bottom = height;
	clientSize.left = 0;
	clientSize.right = width;
	clientSize.top = 0;
	
	AdjustWindowRectEx(
	  &clientSize,    // client-rectangle structure
	  windowflags,    // window styles
	  FALSE,          // menu-present option
	  exstyle         // extended window style
	);

	SetWindowPos(display_hwnd, HWND_TOP, x, y, clientSize.right - clientSize.left, clientSize.bottom - clientSize.top, SWP_NOZORDER);
}