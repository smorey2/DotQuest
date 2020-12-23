#include <android/native_window_jni.h>	// for native window JNI
#include <android/input.h>
#include "lib/argtable3.h"

#include <jni.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )

//#include <EGL/egl.h>
//#include <EGL/eglext.h>
#include <GLES3/gl3.h>
//#include <GLES3/gl3ext.h>
#include <GLES/gl2ext.h>

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"
#include "VrApi_Types.h"

#include <../src/gl/loader.h>

#include "VrCompositor.h"

// Must use EGLSyncKHR because the VrApi still supports OpenGL ES 2.0
#define EGL_SYNC
#if defined EGL_SYNC
// EGL_KHR_reusable_sync
PFNEGLCREATESYNCKHRPROC			eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC		eglDestroySyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC		eglClientWaitSyncKHR;
PFNEGLSIGNALSYNCKHRPROC			eglSignalSyncKHR;
PFNEGLGETSYNCATTRIBKHRPROC		eglGetSyncAttribKHR;
#endif

// Let's go to the maximum!
int CPU_LEVEL = 4;
int GPU_LEVEL = 4;
int NUM_MULTI_SAMPLES = 1;
float SS_MULTIPLIER = 1.25f;
float maximumSupportedFramerate = 60.0; //The lowest default framerate

/*
================================================================================
OpenGL - ES Utility Functions
================================================================================
*/

typedef struct {
	bool multi_view;						// GL_OVR_multiview, GL_OVR_multiview2
	bool EXT_texture_border_clamp;			// GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
} OpenGLExtensions_t;
OpenGLExtensions_t glExtensions;

static void EglInitExtensions() {
#if defined EGL_SYNC
	eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
	eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
	eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
	eglSignalSyncKHR = (PFNEGLSIGNALSYNCKHRPROC)eglGetProcAddress("eglSignalSyncKHR");
	eglGetSyncAttribKHR = (PFNEGLGETSYNCATTRIBKHRPROC)eglGetProcAddress("eglGetSyncAttribKHR");
#endif

	const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
	if (allExtensions != NULL) {
		glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") && strstr(allExtensions, "GL_OVR_multiview_multisampled_render_to_texture");
		glExtensions.EXT_texture_border_clamp = false; //strstr(allExtensions, "GL_EXT_texture_border_clamp") || strstr(allExtensions, "GL_OES_texture_border_clamp");
	}
}

static const char* EglErrorString(const EGLint error) {
	switch (error) {
	case EGL_SUCCESS:				return "EGL_SUCCESS";
	case EGL_NOT_INITIALIZED:		return "EGL_NOT_INITIALIZED";
	case EGL_BAD_ACCESS:			return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:				return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:			return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:			return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:			return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE:	return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:			return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:			return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:				return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:			return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:		return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:		return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:			return "EGL_CONTEXT_LOST";
	default:						return "unknown";
	}
}

static const char* GlFrameBufferStatusString(GLenum status) {
	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED:						return "GL_FRAMEBUFFER_UNDEFINED";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	case GL_FRAMEBUFFER_UNSUPPORTED:					return "GL_FRAMEBUFFER_UNSUPPORTED";
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:			return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
	default:											return "unknown";
	}
}

/*
================================================================================
ovrEgl
================================================================================
*/

typedef struct {
	EGLint		MajorVersion;
	EGLint		MinorVersion;
	EGLDisplay	Display;
	EGLConfig	Config;
	EGLSurface	TinySurface;
	EGLSurface	MainSurface;
	EGLContext	Context;
} ovrEgl;

static void ovrEgl_Clear(ovrEgl* egl) {
	egl->MajorVersion = 0;
	egl->MinorVersion = 0;
	egl->Display = 0;
	egl->Config = 0;
	egl->TinySurface = EGL_NO_SURFACE;
	egl->MainSurface = EGL_NO_SURFACE;
	egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl) {
	if (egl->Display != 0)
		return;
	egl->Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	ALOGV("eglInitialize(Display, &MajorVersion, &MinorVersion)");
	eglInitialize(egl->Display, &egl->MajorVersion, &egl->MinorVersion);
	// Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
	// flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
	// settings, and that is completely wasted for our warp target.
	const int MAX_CONFIGS = 1024;
	EGLConfig configs[MAX_CONFIGS];
	EGLint numConfigs = 0;
	if (eglGetConfigs(egl->Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
		ALOGE("eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
		return;
	}
	const EGLint configAttribs[] = {
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8, // need alpha for the multi-pass timewarp compositor
		EGL_DEPTH_SIZE,		0,
		EGL_STENCIL_SIZE,	0,
		EGL_SAMPLES,		0,
		EGL_NONE
	};
	egl->Config = 0;
	for (int i = 0; i < numConfigs; i++) {
		EGLint value = 0;

		eglGetConfigAttrib(egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value);
		if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR)
			continue;

		// The pbuffer config also needs to be compatible with normal window rendering
		// so it can share textures with the window context.
		eglGetConfigAttrib(egl->Display, configs[i], EGL_SURFACE_TYPE, &value);
		if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT))
			continue;

		int	j = 0;
		for (; configAttribs[j] != EGL_NONE; j += 2) {
			eglGetConfigAttrib(egl->Display, configs[i], configAttribs[j], &value);
			if (value != configAttribs[j + 1])
				break;
		}
		if (configAttribs[j] == EGL_NONE) {
			egl->Config = configs[i];
			break;
		}
	}
	if (egl->Config == 0) {
		ALOGE("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
		return;
	}
	EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	ALOGV("Context = eglCreateContext(Display, Config, EGL_NO_CONTEXT, contextAttribs)");
	egl->Context = eglCreateContext(egl->Display, egl->Config, (shareEgl != NULL) ? shareEgl->Context : EGL_NO_CONTEXT, contextAttribs);
	if (egl->Context == EGL_NO_CONTEXT) {
		ALOGE("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
		return;
	}
	const EGLint surfaceAttribs[] = {
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
		EGL_NONE
	};
	ALOGV("TinySurface = eglCreatePbufferSurface(Display, Config, surfaceAttribs)");
	egl->TinySurface = eglCreatePbufferSurface(egl->Display, egl->Config, surfaceAttribs);
	if (egl->TinySurface == EGL_NO_SURFACE) {
		ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
		eglDestroyContext(egl->Display, egl->Context);
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
	ALOGV("        eglMakeCurrent(Display, TinySurface, TinySurface, Context)");
	if (eglMakeCurrent(egl->Display, egl->TinySurface, egl->TinySurface, egl->Context) == EGL_FALSE) {
		ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
		eglDestroySurface(egl->Display, egl->TinySurface);
		eglDestroyContext(egl->Display, egl->Context);
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
}

static void ovrEgl_DestroyContext(ovrEgl* egl) {
	if (egl->Display != 0) {
		ALOGE("        eglMakeCurrent(Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)");
		if (eglMakeCurrent(egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE)
			ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
	}
	if (egl->Context != EGL_NO_CONTEXT) {
		ALOGE("eglDestroyContext(Display, Context)");
		if (eglDestroyContext(egl->Display, egl->Context) == EGL_FALSE)
			ALOGE("eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
		egl->Context = EGL_NO_CONTEXT;
	}
	if (egl->TinySurface != EGL_NO_SURFACE) {
		ALOGE("eglDestroySurface( Display, TinySurface )");
		if (eglDestroySurface(egl->Display, egl->TinySurface) == EGL_FALSE)
			ALOGE("eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
		egl->TinySurface = EGL_NO_SURFACE;
	}
	if (egl->Display != 0) {
		ALOGE("eglTerminate( Display )");
		if (eglTerminate(egl->Display) == EGL_FALSE)
			ALOGE("eglTerminate() failed: %s", EglErrorString(eglGetError()));
		egl->Display = 0;
	}
}

/*
================================================================================
ovrFramebuffer
================================================================================
*/

static void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer) {
	frameBuffer->Width = 0;
	frameBuffer->Height = 0;
	frameBuffer->Multisamples = 0;
	frameBuffer->TextureSwapChainLength = 0;
	frameBuffer->ProcessingTextureSwapChainIndex = 0;
	frameBuffer->ReadyTextureSwapChainIndex = 0;
	frameBuffer->ColorTextureSwapChain = NULL;
	frameBuffer->DepthBuffers = NULL;
	frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create(ovrFramebuffer* frameBuffer, const GLenum colorFormat, const int width, const int height, const int multisamples) {
	LOAD_GLES2(glBindTexture);
	LOAD_GLES2(glTexParameteri);
	LOAD_GLES2(glGenRenderbuffers);
	LOAD_GLES2(glBindRenderbuffer);
	LOAD_GLES2(glRenderbufferStorage);
	LOAD_GLES2(glGenFramebuffers);
	LOAD_GLES2(glBindFramebuffer);
	LOAD_GLES2(glFramebufferRenderbuffer);
	LOAD_GLES2(glFramebufferTexture2D);
	LOAD_GLES2(glCheckFramebufferStatus);

	frameBuffer->Width = width;
	frameBuffer->Height = height;
	frameBuffer->Multisamples = multisamples;

	frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D, colorFormat, frameBuffer->Width, frameBuffer->Height, 1, 3);
	frameBuffer->TextureSwapChainLength = vrapi_GetTextureSwapChainLength(frameBuffer->ColorTextureSwapChain);
	frameBuffer->DepthBuffers = (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
	frameBuffer->FrameBuffers = (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");

	for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
		// Create the color buffer texture.
		const GLuint colorTexture = vrapi_GetTextureSwapChainHandle(frameBuffer->ColorTextureSwapChain, i);
		GLenum colorTextureTarget = GL_TEXTURE_2D;
		GL(gles_glBindTexture(colorTextureTarget, colorTexture));
		// Just clamp to edge. However, this requires manually clearing the border
		// around the layer to clear the edge texels.
		GL(gles_glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL(gles_glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

		GL(gles_glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL(gles_glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL(gles_glBindTexture(colorTextureTarget, 0));

		if (multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL && glFramebufferTexture2DMultisampleEXT != NULL) {
			// Create multisampled depth buffer.
			GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
			GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
			GL(glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height));
			GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

			// Create the frame buffer.
			GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
			GL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
			GL(glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0, multisamples));
			GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
			GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER));
			GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
			if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
				ALOGE("OVRHelper::Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
				return false;
			}
		}
		else {
			// Create depth buffer.
			GL(gles_glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
			GL(gles_glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
			GL(gles_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, frameBuffer->Width, frameBuffer->Height));
			GL(gles_glBindRenderbuffer(GL_RENDERBUFFER, 0));

			// Create the frame buffer.
			GL(gles_glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
			GL(gles_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
			GL(gles_glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
			GL(gles_glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0));
			GL(GLenum renderFramebufferStatus = gles_glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
			GL(gles_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
			if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
				ALOGE("Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
				return false;
			}
		}
	}

	return true;
}

void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer) {
	LOAD_GLES2(glDeleteFramebuffers);
	LOAD_GLES2(glDeleteRenderbuffers);

	GL(gles_glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers));
	GL(gles_glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));

	vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

	free(frameBuffer->DepthBuffers);
	free(frameBuffer->FrameBuffers);

	ovrFramebuffer_Clear(frameBuffer);
}

void GPUWaitSync() {
	GLsync syncBuff = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	GLenum status = glClientWaitSync(syncBuff, GL_SYNC_FLUSH_COMMANDS_BIT, 1000 * 1000 * 50); // Wait for a max of 50ms...
	if (status != GL_CONDITION_SATISFIED)
		ALOGE("Error on glClientWaitSync: %d\n", status);
	glDeleteSync(syncBuff);
}

void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer) {
	LOAD_GLES2(glBindFramebuffer);
	GL(gles_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->ProcessingTextureSwapChainIndex]));
}

void ovrFramebuffer_SetNone() {
	LOAD_GLES2(glBindFramebuffer);
	GL(gles_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer) {
	// Discard the depth buffer, so the tiler won't need to write it back out to memory.
//	const GLenum depthAttachment[1] = { GL_DEPTH_ATTACHMENT };
//	glInvalidateFramebuffer( GL_DRAW_FRAMEBUFFER, 1, depthAttachment );

	// Flush this frame worth of commands.
	glFlush();
}

void ovrFramebuffer_Advance(ovrFramebuffer* frameBuffer) {
	// Advance to the next texture from the set.
	frameBuffer->ReadyTextureSwapChainIndex = frameBuffer->ProcessingTextureSwapChainIndex;
	frameBuffer->ProcessingTextureSwapChainIndex = (frameBuffer->ProcessingTextureSwapChainIndex + 1) % frameBuffer->TextureSwapChainLength;
}

void ovrFramebuffer_ClearEdgeTexels(ovrFramebuffer* frameBuffer) {
	LOAD_GLES2(glEnable);
	LOAD_GLES2(glDisable);
	LOAD_GLES2(glViewport);
	LOAD_GLES2(glScissor);
	LOAD_GLES2(glClearColor);
	LOAD_GLES2(glClear);

	GL(gles_glEnable(GL_SCISSOR_TEST));
	GL(gles_glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));

	// Explicitly clear the border texels to black because OpenGL-ES does not support GL_CLAMP_TO_BORDER.
	// Clear to fully opaque black.
	GL(gles_glClearColor(0.0f, 0.0f, 0.0f, 1.0f));

	// bottom
	GL(gles_glScissor(0, 0, frameBuffer->Width, 1));
	GL(gles_glClear(GL_COLOR_BUFFER_BIT));
	// top
	GL(gles_glScissor(0, frameBuffer->Height - 1, frameBuffer->Width, 1));
	GL(gles_glClear(GL_COLOR_BUFFER_BIT));
	// left
	GL(gles_glScissor(0, 0, 1, frameBuffer->Height));
	GL(gles_glClear(GL_COLOR_BUFFER_BIT));
	// right
	GL(gles_glScissor(frameBuffer->Width - 1, 0, 1, frameBuffer->Height));
	GL(gles_glClear(GL_COLOR_BUFFER_BIT));

	GL(gles_glScissor(0, 0, 0, 0));
	GL(gles_glDisable(GL_SCISSOR_TEST));
}


/*
================================================================================
ovrRenderer
================================================================================
*/

void ovrRenderer_Clear(ovrRenderer* renderer) {
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
		ovrFramebuffer_Clear(&renderer->FrameBuffer[eye]);
	renderer->ProjectionMatrix = ovrMatrix4f_CreateIdentity();
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
}

void ovrRenderer_Create(int width, int height, ovrRenderer* renderer, const ovrJava* java) {
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;

	// now using a symmetrical render target, based on the horizontal FOV
	vr.fov = vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y);

	// create the render Textures.
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
		ovrFramebuffer_Create(&renderer->FrameBuffer[eye], GL_RGBA8, width, height, NUM_MULTI_SAMPLES);

	// setup the projection matrix.
	renderer->ProjectionMatrix = ovrMatrix4f_CreateProjectionFov(vr.fov, vr.fov, 0.0f, 0.0f, 1.0f, 0.0f);
}

void ovrRenderer_Destroy(ovrRenderer* renderer) {
	for (int eye = 0; eye < renderer->NumBuffers; eye++)
		ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
	renderer->ProjectionMatrix = ovrMatrix4f_CreateIdentity();
}


/*
================================================================================
ovrApp
================================================================================
*/

typedef struct {
	ovrJava				Java;
	ovrEgl				Egl;
	ANativeWindow* NativeWindow;
	bool				Resumed;
	ovrMobile* Ovr;
	ovrScene			Scene;
	long long			FrameIndex;
	double 				DisplayTime;
	int					SwapInterval;
	int					CpuLevel;
	int					GpuLevel;
	int					MainThreadTid;
	int					RenderThreadTid;
	ovrLayer_Union2		Layers[ovrMaxLayerCount];
	int					LayerCount;
	ovrRenderer			Renderer;
} ovrApp;

static void ovrApp_Clear(ovrApp* app) {
	app->Java.Vm = NULL;
	app->Java.Env = NULL;
	app->Java.ActivityObject = NULL;
	app->Ovr = NULL;
	app->FrameIndex = 1;
	app->DisplayTime = 0;
	app->SwapInterval = 1;
	app->CpuLevel = 3;
	app->GpuLevel = 3;
	app->MainThreadTid = 0;
	app->RenderThreadTid = 0;

	ovrEgl_Clear(&app->Egl);

	ovrScene_Clear(&app->Scene);
	ovrRenderer_Clear(&app->Renderer);
}

static void ovrApp_PushBlackFinal(ovrApp* app) {
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH | VRAPI_FRAME_FLAG_FINAL;

	ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

	const ovrLayerHeader2* layers[] = {
		&layer.Header
	};

	ovrSubmitFrameDescription2 frameDesc = {};
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = 1;
	frameDesc.FrameIndex = app->FrameIndex;
	frameDesc.DisplayTime = app->DisplayTime;
	frameDesc.LayerCount = 1;
	frameDesc.Layers = layers;

	vrapi_SubmitFrame2(app->Ovr, &frameDesc);
}

static void ovrApp_HandleVrModeChanges(ovrApp* app) {
	if (app->Resumed && app->NativeWindow) {
		if (app->Ovr)
			return;
		ovrModeParms parms = vrapi_DefaultModeParms(&app->Java);
		// Must reset the FLAG_FULLSCREEN window flag when using a SurfaceView
		parms.Flags |= VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

		parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
		parms.Display = (size_t)app->Egl.Display;
		parms.WindowSurface = (size_t)app->NativeWindow;
		parms.ShareContext = (size_t)app->Egl.Context;

		ALOGV("eglGetCurrentSurface(EGL_DRAW) = %p", eglGetCurrentSurface(EGL_DRAW));
		ALOGV("vrapi_EnterVrMode()");
		app->Ovr = vrapi_EnterVrMode(&parms);
		ALOGV("eglGetCurrentSurface(EGL_DRAW) = %p", eglGetCurrentSurface(EGL_DRAW));
		// If entering VR mode failed then the ANativeWindow was not valid.
		if (!app->Ovr) {
			ALOGE("Invalid ANativeWindow!");
			app->NativeWindow = NULL;
			return;
		}

		// Set performance parameters once we have entered VR mode and have a valid ovrMobile.
		ovrResult result = vrapi_SetDisplayRefreshRate(app->Ovr, maximumSupportedFramerate);
		if (result == ovrSuccess) ALOGV("Changed refresh rate. %f Hz", maximumSupportedFramerate);
		else ALOGV("Failed to change refresh rate to 90Hz Result = %d", result);
		vrapi_SetClockLevels(app->Ovr, app->CpuLevel, app->GpuLevel);
		ALOGV("vrapi_SetClockLevels(%d, %d)", app->CpuLevel, app->GpuLevel);
		vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid);
		ALOGV("vrapi_SetPerfThread(MAIN, %d)", app->MainThreadTid);
		vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid);
		ALOGV("vrapi_SetPerfThread(RENDERER, %d)", app->RenderThreadTid);
	}
	else {
		if (!app->Ovr)
			return;
		ALOGV("eglGetCurrentSurface(EGL_DRAW) = %p", eglGetCurrentSurface(EGL_DRAW));
		ALOGV("vrapi_LeaveVrMode()");
		vrapi_LeaveVrMode(app->Ovr); app->Ovr = NULL;
		ALOGV("eglGetCurrentSurface(EGL_DRAW) = %p", eglGetCurrentSurface(EGL_DRAW));
	}
}


/*
================================================================================
ovrMessageQueue
================================================================================
*/

typedef enum {
	MQ_WAIT_NONE,		// don't wait
	MQ_WAIT_RECEIVED,	// wait until the consumer thread has received the message
	MQ_WAIT_PROCESSED	// wait until the consumer thread has processed the message
} ovrMQWait;

#define MAX_MESSAGE_PARMS	8
#define MAX_MESSAGES		1024

typedef struct {
	int			Id;
	ovrMQWait	Wait;
	long long	Parms[MAX_MESSAGE_PARMS];
} ovrMessage;

static void ovrMessage_Init(ovrMessage* message, const int id, const int wait) {
	message->Id = id;
	message->Wait = (ovrMQWait)wait;
	memset(message->Parms, 0, sizeof(message->Parms));
}

static void ovrMessage_SetPointerParm(ovrMessage* message, int index, void* ptr) { *(void**)&message->Parms[index] = ptr; }
static void* ovrMessage_GetPointerParm(ovrMessage* message, int index) { return *(void**)&message->Parms[index]; }
static void ovrMessage_SetIntegerParm(ovrMessage* message, int index, int value) { message->Parms[index] = value; }
static int ovrMessage_GetIntegerParm(ovrMessage* message, int index) { return (int)message->Parms[index]; }
static void ovrMessage_SetFloatParm(ovrMessage* message, int index, float value) { *(float*)&message->Parms[index] = value; }
static float ovrMessage_GetFloatParm(ovrMessage* message, int index) { return *(float*)&message->Parms[index]; }

// cyclic queue with messages.
typedef struct {
	ovrMessage	 		Messages[MAX_MESSAGES];
	volatile int		Head;	// dequeue at the head
	volatile int		Tail;	// enqueue at the tail
	ovrMQWait			Wait;
	volatile bool		EnabledFlag;
	volatile bool		PostedFlag;
	volatile bool		ReceivedFlag;
	volatile bool		ProcessedFlag;
	pthread_mutex_t		Mutex;
	pthread_cond_t		PostedCondition;
	pthread_cond_t		ReceivedCondition;
	pthread_cond_t		ProcessedCondition;
} ovrMessageQueue;

static void ovrMessageQueue_Create(ovrMessageQueue* messageQueue) {
	messageQueue->Head = 0;
	messageQueue->Tail = 0;
	messageQueue->Wait = MQ_WAIT_NONE;
	messageQueue->EnabledFlag = false;
	messageQueue->PostedFlag = false;
	messageQueue->ReceivedFlag = false;
	messageQueue->ProcessedFlag = false;

	pthread_mutexattr_t	attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&messageQueue->Mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	pthread_cond_init(&messageQueue->PostedCondition, NULL);
	pthread_cond_init(&messageQueue->ReceivedCondition, NULL);
	pthread_cond_init(&messageQueue->ProcessedCondition, NULL);
}

static void ovrMessageQueue_Destroy(ovrMessageQueue* messageQueue) {
	pthread_mutex_destroy(&messageQueue->Mutex);
	pthread_cond_destroy(&messageQueue->PostedCondition);
	pthread_cond_destroy(&messageQueue->ReceivedCondition);
	pthread_cond_destroy(&messageQueue->ProcessedCondition);
}

static void ovrMessageQueue_Enable(ovrMessageQueue* messageQueue, const bool set) {
	messageQueue->EnabledFlag = set;
}

static void ovrMessageQueue_PostMessage(ovrMessageQueue* messageQueue, const ovrMessage* message) {
	if (!messageQueue->EnabledFlag)
		return;
	while (messageQueue->Tail - messageQueue->Head >= MAX_MESSAGES)
		usleep(1000);
	pthread_mutex_lock(&messageQueue->Mutex);
	messageQueue->Messages[messageQueue->Tail & (MAX_MESSAGES - 1)] = *message;
	messageQueue->Tail++;
	messageQueue->PostedFlag = true;
	pthread_cond_broadcast(&messageQueue->PostedCondition);
	if (message->Wait == MQ_WAIT_RECEIVED) {
		while (!messageQueue->ReceivedFlag)
			pthread_cond_wait(&messageQueue->ReceivedCondition, &messageQueue->Mutex);
		messageQueue->ReceivedFlag = false;
	}
	else if (message->Wait == MQ_WAIT_PROCESSED) {
		while (!messageQueue->ProcessedFlag)
			pthread_cond_wait(&messageQueue->ProcessedCondition, &messageQueue->Mutex);
		messageQueue->ProcessedFlag = false;
	}
	pthread_mutex_unlock(&messageQueue->Mutex);
}

static void ovrMessageQueue_SleepUntilMessage(ovrMessageQueue* messageQueue) {
	if (messageQueue->Wait == MQ_WAIT_PROCESSED) {
		messageQueue->ProcessedFlag = true;
		pthread_cond_broadcast(&messageQueue->ProcessedCondition);
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	pthread_mutex_lock(&messageQueue->Mutex);
	if (messageQueue->Tail > messageQueue->Head) {
		pthread_mutex_unlock(&messageQueue->Mutex);
		return;
	}
	while (!messageQueue->PostedFlag)
		pthread_cond_wait(&messageQueue->PostedCondition, &messageQueue->Mutex);
	messageQueue->PostedFlag = false;
	pthread_mutex_unlock(&messageQueue->Mutex);
}

static bool ovrMessageQueue_GetNextMessage(ovrMessageQueue* messageQueue, ovrMessage* message, bool waitForMessages) {
	if (messageQueue->Wait == MQ_WAIT_PROCESSED) {
		messageQueue->ProcessedFlag = true;
		pthread_cond_broadcast(&messageQueue->ProcessedCondition);
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	if (waitForMessages)
		ovrMessageQueue_SleepUntilMessage(messageQueue);
	pthread_mutex_lock(&messageQueue->Mutex);
	if (messageQueue->Tail <= messageQueue->Head) {
		pthread_mutex_unlock(&messageQueue->Mutex);
		return false;
	}
	*message = messageQueue->Messages[messageQueue->Head & (MAX_MESSAGES - 1)];
	messageQueue->Head++;
	pthread_mutex_unlock(&messageQueue->Mutex);
	if (message->Wait == MQ_WAIT_RECEIVED) {
		messageQueue->ReceivedFlag = true;
		pthread_cond_broadcast(&messageQueue->ReceivedCondition);
	}
	else if (message->Wait == MQ_WAIT_PROCESSED)
		messageQueue->Wait = MQ_WAIT_PROCESSED;
	return true;
}

/*
================================================================================
ovrAppThread
================================================================================
*/

enum {
	MESSAGE_ON_CREATE,
	MESSAGE_ON_START,
	MESSAGE_ON_RESUME,
	MESSAGE_ON_PAUSE,
	MESSAGE_ON_STOP,
	MESSAGE_ON_DESTROY,
	MESSAGE_ON_SURFACE_CREATED,
	MESSAGE_ON_SURFACE_DESTROYED
};

typedef struct {
	JavaVM* JavaVm;
	jobject			ActivityObject;
	jclass          ActivityClass;
	pthread_t		Thread;
	ovrMessageQueue	MessageQueue;
	ANativeWindow* NativeWindow;
} ovrAppThread;

void* AppThreadFunction(void* parm);

static void ovrAppThread_Create(ovrAppThread* appThread, JNIEnv* env, jobject activityObject, jclass activityClass) {
	env->GetJavaVM(&appThread->JavaVm);
	appThread->ActivityObject = env->NewGlobalRef(activityObject);
	appThread->ActivityClass = (jclass)env->NewGlobalRef(activityClass);
	appThread->Thread = 0;
	appThread->NativeWindow = NULL;
	ovrMessageQueue_Create(&appThread->MessageQueue);

	const int createError = pthread_create(&appThread->Thread, NULL, AppThreadFunction, appThread);
	if (createError)
		ALOGE("pthread_create returned %i", createError);
}

static void ovrAppThread_Destroy(ovrAppThread* appThread, JNIEnv* env) {
	pthread_join(appThread->Thread, NULL);
	env->DeleteGlobalRef(appThread->ActivityObject);
	env->DeleteGlobalRef(appThread->ActivityClass);
	ovrMessageQueue_Destroy(&appThread->MessageQueue);
}

/*
================================================================================
Activity lifecycle
================================================================================
*/

jmethodID _android_shutdown;
static JavaVM* _jVM;
static jobject _shutdownCallbackObj = 0;

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	ALOGV("JNI_OnLoad");
	JNIEnv* env;
	_jVM = vm;
	if (_jVM->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
		ALOGE("Failed JNI_OnLoad");
		return -1;
	}
	return JNI_VERSION_1_4;
}

void JNI_Shutdown() {
	ALOGV("JNI_Shutdown");
	JNIEnv* env;
	if (_jVM->GetEnv((void**)&env, JNI_VERSION_1_4) < 0) {
		_jVM->AttachCurrentThread(&env, NULL);
	}
	return env->CallVoidMethod(_shutdownCallbackObj, _android_shutdown);
}

// global arg_xxx structs
struct arg_dbl* ss;
struct arg_int* cpu;
struct arg_int* gpu;
struct arg_int* msaa;
struct arg_end* end;
char** argv;
int argc = 0;

extern "C" void initialize_gl4es();

static int ParseCommandLine(char* cmdline, char** argv);

extern "C" JNIEXPORT jlong JNICALL Java_com_dotquest_quest_MainActivityJNI_onCreate(JNIEnv * env, jclass activityClass, jobject activity, jstring commandLineParams) {
	ALOGV("MainActivityJNI::onCreate()");

	// the global arg_xxx structs are initialised within the argtable
	void* argtable[] = {
		ss = arg_dbl0("s", "supersampling", "<double>", "super sampling value (e.g. 1.0)"),
		cpu = arg_int0("c", "cpu", "<int>", "CPU perf index 1-4 (default: 2)"),
		gpu = arg_int0("g", "gpu", "<int>", "GPU perf index 1-4 (default: 3)"),
		msaa = arg_int0("m", "msaa", "<int>", "MSAA (default: 1)"),
		end = arg_end(20)
	};

	jboolean iscopy;
	const char* arg = env->GetStringUTFChars(commandLineParams, &iscopy);
	char* cmdLine = arg && strlen(arg) ? strdup(arg) : NULL;
	env->ReleaseStringUTFChars(commandLineParams, arg);
	ALOGV("Command line %s", cmdLine);
	argv = (char**)malloc(sizeof(char*) * 255);
	argc = ParseCommandLine(strdup(cmdLine), argv);

	// verify the argtable[] entries were allocated sucessfully
	if (arg_nullcheck(argtable) == 0) {
		// Parse the command line as defined by argtable[]
		arg_parse(argc, argv, argtable);
		if (ss->count > 0 && ss->dval[0] > 0.0)
			SS_MULTIPLIER = ss->dval[0];
		if (cpu->count > 0 && cpu->ival[0] > 0 && cpu->ival[0] < 10)
			CPU_LEVEL = cpu->ival[0];
		if (gpu->count > 0 && gpu->ival[0] > 0 && gpu->ival[0] < 10)
			GPU_LEVEL = gpu->ival[0];
		if (msaa->count > 0 && msaa->ival[0] > 0 && msaa->ival[0] < 10)
			NUM_MULTI_SAMPLES = msaa->ival[0];
	}

	initialize_gl4es();

	ovrAppThread* appThread = (ovrAppThread*)malloc(sizeof(ovrAppThread));
	ovrAppThread_Create(appThread, env, activity, activityClass);
	ovrMessageQueue_Enable(&appThread->MessageQueue, true);
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	return (jlong)((size_t)appThread);
}

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onStart(JNIEnv * env, jobject obj, jlong handle, jobject obj1) {
	ALOGV("MainActivityJNI::onStart()");
	_shutdownCallbackObj = (jobject)env->NewGlobalRef(obj1);
	jclass callbackClass = env->GetObjectClass(_shutdownCallbackObj);
	_android_shutdown = env->GetMethodID(callbackClass, "shutdown", "()V");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_START, MQ_WAIT_PROCESSED);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onResume(JNIEnv * env, jobject obj, jlong handle) {
	ALOGV("MainActivityJNI::onResume()");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onPause(JNIEnv * env, jobject obj, jlong handle) {
	ALOGV("MainActivityJNI::onPause()");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onStop(JNIEnv * env, jobject obj, jlong handle) {
	ALOGV("MainActivityJNI::onStop()");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onDestroy(JNIEnv * env, jobject obj, jlong handle) {
	ALOGV("MainActivityJNI::onDestroy()");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	ovrMessageQueue_Enable(&appThread->MessageQueue, false);
	ovrAppThread_Destroy(appThread, env);
	free(appThread);
}

/*
================================================================================
Surface lifecycle
================================================================================
*/

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onSurfaceCreated(JNIEnv * env, jobject obj, jlong handle, jobject surface) {
	ALOGV("MainActivityJNI::onSurfaceCreated()");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	// An app that is relaunched after pressing the home button gets an initial surface with the wrong orientation even though android:screenOrientation="landscape" is set in the
	// manifest. The choreographer callback will also never be called for this surface because the surface is immediately replaced with a new surface with the correct orientation.
	ANativeWindow* newNativeWindow = ANativeWindow_fromSurface(env, surface);
	if (ANativeWindow_getWidth(newNativeWindow) < ANativeWindow_getHeight(newNativeWindow))
		ALOGE("Surface not in landscape mode!");
	ALOGV("NativeWindow = ANativeWindow_fromSurface(env, surface)");
	appThread->NativeWindow = newNativeWindow;
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
	ovrMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onSurfaceChanged(JNIEnv * env, jobject obj, jlong handle, jobject surface) {
	ALOGV("MainActivityJNI::onSurfaceChanged()");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	// An app that is relaunched after pressing the home button gets an initial surface with the wrong orientation even though android:screenOrientation="landscape" is set in the
	// manifest. The choreographer callback will also never be called for this surface because the surface is immediately replaced with a new surface with the correct orientation.
	ANativeWindow* newNativeWindow = ANativeWindow_fromSurface(env, surface);
	if (ANativeWindow_getWidth(newNativeWindow) < ANativeWindow_getHeight(newNativeWindow))
		ALOGE("Surface not in landscape mode!");
	if (newNativeWindow != appThread->NativeWindow) {
		if (appThread->NativeWindow != NULL) {
			ovrMessage message;
			ovrMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
			ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
			ALOGV("ANativeWindow_release(NativeWindow)");
			ANativeWindow_release(appThread->NativeWindow);
			appThread->NativeWindow = NULL;
		}
		if (newNativeWindow != NULL) {
			ALOGV("NativeWindow = ANativeWindow_fromSurface(env, surface)");
			appThread->NativeWindow = newNativeWindow;
			ovrMessage message;
			ovrMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
			ovrMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
			ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
		}
	}
	else if (newNativeWindow != NULL) {
		ANativeWindow_release(newNativeWindow);
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_dotquest_quest_MainActivityJNI_onSurfaceDestroyed(JNIEnv * env, jobject obj, jlong handle) {
	ALOGV("MainActivityJNI::onSurfaceDestroyed()");
	ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
	ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	ALOGV("ANativeWindow_release(NativeWindow)");
	ANativeWindow_release(appThread->NativeWindow);
	appThread->NativeWindow = NULL;
}

/*
================================================================================
AppThread
================================================================================
*/

static ovrAppThread* _appThread = NULL;
static ovrApp _appState;
static ovrJava _java;
static bool _destroyed = false;

void AppProcessMessageQueue() {
	for (;;) {
		ovrMessage message;
		const bool waitForMessages = !_appState.Ovr && !_destroyed;
		if (!ovrMessageQueue_GetNextMessage(&_appThread->MessageQueue, &message, waitForMessages))
			return;
		switch (message.Id) {
		case MESSAGE_ON_CREATE: break;
		case MESSAGE_ON_START:
			if (vr.initialized) break;
			ALOGV("Initializing DotQuest Engine");
			if (argc != 0) {} // set command line arguments here
			else { int argc = 1; char* argv[] = { "quest" }; }
			vr.initialized = true;
			break;
		case MESSAGE_ON_RESUME: _appState.Resumed = true; break;
		case MESSAGE_ON_PAUSE: _appState.Resumed = false; break;
		case MESSAGE_ON_STOP: break;
		case MESSAGE_ON_DESTROY: _appState.NativeWindow = NULL; _destroyed = true; break;
		case MESSAGE_ON_SURFACE_CREATED: _appState.NativeWindow = (ANativeWindow*)ovrMessage_GetPointerParm(&message, 0); break;
		case MESSAGE_ON_SURFACE_DESTROYED: _appState.NativeWindow = NULL; break;
		}
		ovrApp_HandleVrModeChanges(&_appState);
	}
}

void AppIncrementFrameIndex() {
	// This is the only place the frame index is incremented, right before calling vrapi_GetPredictedDisplayTime().
	_appState.FrameIndex++;
	_appState.DisplayTime = vrapi_GetPredictedDisplayTime(_appState.Ovr, _appState.FrameIndex);
}

void AppShowLoadingIcon() {
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH;
	ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
	blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
	ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
	iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
	const ovrLayerHeader2* layers[] = { &blackLayer.Header, &iconLayer.Header };
	ovrSubmitFrameDescription2 frameDesc = {};
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = 1;
	frameDesc.FrameIndex = _appState.FrameIndex;
	frameDesc.DisplayTime = _appState.DisplayTime;
	frameDesc.LayerCount = 2;
	frameDesc.Layers = layers;
	vrapi_SubmitFrame2(_appState.Ovr, &frameDesc);
}

void AppShutdownVR() {
	ovrRenderer_Destroy(&_appState.Renderer);
	ovrEgl_DestroyContext(&_appState.Egl);
	_java.Vm->DetachCurrentThread();
	vrapi_Shutdown();
}

int AppMain(int argc, char* argv[]) {
	for (int i = 0; i < 10000; i++) {
		ALOGV("APPMAIN: %d", i);
		usleep(1000);
	}
}

void* AppThreadFunction(void* parm) {
	_appThread = (ovrAppThread*)parm;

	_java.Vm = _appThread->JavaVm;
	_java.Vm->AttachCurrentThread(&_java.Env, NULL);
	_java.ActivityObject = _appThread->ActivityObject;
	jclass cls = _java.Env->GetObjectClass(_java.ActivityObject);
	// Note that AttachCurrentThread will reset the thread name.
	prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

	vr.initialized = false;
	vr.screen_dist = NULL;

	const ovrInitParms initParms = vrapi_DefaultInitParms(&_java);
	int32_t initResult = vrapi_Initialize(&initParms);
	if (initResult != VRAPI_INITIALIZE_SUCCESS)
		exit(0); // If intialization failed, vrapi_* function calls will not be available.

	ovrApp_Clear(&_appState);
	_appState.Java = _java;

	// this app will handle android gamepad events itself.
	vrapi_SetPropertyInt(&_appState.Java, VRAPI_EAT_NATIVE_GAMEPAD_EVENTS, 0);

	// using a symmetrical render target
	vr.height = vr.width = (int)(vrapi_GetSystemPropertyInt(&_java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH) * SS_MULTIPLIER);

	_appState.CpuLevel = CPU_LEVEL;
	_appState.GpuLevel = GPU_LEVEL;
	_appState.MainThreadTid = gettid();

	ovrEgl_CreateContext(&_appState.Egl, NULL);
	EglInitExtensions();

	// first handle any messages in the queue
	while (!_appState.Ovr)
		AppProcessMessageQueue();

	ovrRenderer_Create(vr.width, vr.height, &_appState.Renderer, &_java);
	if (!_appState.Ovr)
		return NULL;

	// create the scene if not yet created.
	ovrScene_Create(vr.width, vr.height, &_appState.Scene, &_java);

	chdir("/sdcard/DotQuest");

	// run loading loop until we are initialized
	while (!_destroyed && !vr.initialized) {
		AppProcessMessageQueue();
		AppIncrementFrameIndex();
		AppShowLoadingIcon();
	}

	// refresh rates are currently (12/2020) the following 4 : 60.0 / 72.0 / 80.0 / 90.0
	int numberOfRefreshRates = vrapi_GetSystemPropertyInt(&_java, VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
	float refreshRatesArray[16];
	if (numberOfRefreshRates > 16) numberOfRefreshRates = 16;
	vrapi_GetSystemPropertyFloatArray(&_java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES, &refreshRatesArray[0], numberOfRefreshRates);
	for (int i = 0; i < numberOfRefreshRates; i++) {
		ALOGV("Supported refresh rate : %s Hz", refreshRatesArray[i]);
		if (maximumSupportedFramerate < refreshRatesArray[i])
			maximumSupportedFramerate = refreshRatesArray[i];
	}
	if (maximumSupportedFramerate > 90.0) {
		ALOGV("Soft limiting to 90.0 Hz");
		maximumSupportedFramerate = 90.0;
	}

	// start
	AppMain(argc, argv);

	// we are done, shutdown cleanly
	AppShutdownVR();
	JNI_Shutdown();
	return NULL;
}

/*
================================================================================
Helper functions
================================================================================
*/

static void UnEscapeQuotes(char* arg) {
	char* last = NULL;
	while (*arg) {
		if (*arg == '"' && *last == '\\') {
			char* c_curr = arg;
			char* c_last = last;
			while (*c_curr) {
				*c_last = *c_curr;
				c_last = c_curr;
				c_curr++;
			}
			*c_last = '\0';
		}
		last = arg;
		arg++;
	}
}

static int ParseCommandLine(char* cmdline, char** argv) {
	char* bufp;
	char* lastp = NULL;
	int argc, last_argc;
	argc = last_argc = 0;
	for (bufp = cmdline; *bufp; ) {
		while (isspace(*bufp)) {
			++bufp;
		}
		if (*bufp == '"') {
			++bufp;
			if (*bufp) {
				if (argv) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while (*bufp && (*bufp != '"' || *lastp == '\\')) {
				lastp = bufp;
				++bufp;
			}
		}
		else {
			if (*bufp) {
				if (argv) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while (*bufp && !isspace(*bufp)) {
				++bufp;
			}
		}
		if (*bufp) {
			if (argv) {
				*bufp = '\0';
			}
			++bufp;
		}
		if (argv && last_argc != argc) {
			UnEscapeQuotes(argv[last_argc]);
		}
		last_argc = argc;
	}
	if (argv) {
		argv[argc] = NULL;
	}
	return(argc);
}
