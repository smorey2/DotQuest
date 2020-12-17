#include "DotQuestJNI.h"

#include <android/native_window_jni.h>	// for native window JNI
#include <android/input.h>

#include "argtable3.h"
#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"
#include "VrApi_Types.h"

#define LOG_TAG "GAMETAG"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#if _DEBUG
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#else
#define LOGV(...)
#endif

int dotnet(int argc, char* argv[]);

#if defined(__arm__)
#if defined(__ARM_ARCH_7A__)
#if defined(__ARM_NEON__)
#define ABI "armeabi-v7a/NEON"
#else
#define ABI "armeabi-v7a"
#endif
#else
#define ABI "armeabi"
#endif
#elif defined(__i386__)
#define ABI "x86"
#else
#define ABI "unknown"
#endif

extern "C" {

	/*
	================================================================================

	Activity lifecycle

	================================================================================
	*/

	jmethodID _android_shutdown;
	static JavaVM* _jVM;
	static jobject _shutdownCallbackObj = 0;

	jint JNI_OnLoad(JavaVM* vm, void* reserved) {
		LOGI("JNI_OnLoad");
		JNIEnv* env;
		_jVM = vm;
		if (vm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
			LOGE("Failed JNI_OnLoad");
			return -1;
		}
		return JNI_VERSION_1_4;
	}

	void jni_shutdown() {
		LOGV("Calling: jni_shutdown");
		JNIEnv* env;
		if ((_jVM->GetEnv((void**)&env, JNI_VERSION_1_4)) < 0) {
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

	// Let's go to the maximum!
	int CPU_LEVEL = 4;
	int GPU_LEVEL = 4;
	int NUM_MULTI_SAMPLES = 1;
	float SS_MULTIPLIER = 1.25f;

	static int ParseCommandLine(char* cmdline, char** argv);

	JNIEXPORT jlong JNICALL Java_com_dotquest_quest_DotQuestJNI_onCreate(JNIEnv* env, jclass activityClass, jobject activity, jstring commandLineParams) {
		LOGV("    DotQuestJNI::onCreate()");

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

		LOGV("Command line %s", cmdLine);
		argv = (char**)malloc(sizeof(char*) * 255);
		argc = ParseCommandLine(strdup(cmdLine), argv);

		// dotnet
		//dotnet(argc, argv);

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

		// initialize_gl4es();


		//ovrAppThread* appThread = (ovrAppThread*)malloc(sizeof(ovrAppThread));
		//ovrAppThread_Create(appThread, env, activity, activityClass);

		//ovrMessageQueue_Enable(&appThread->MessageQueue, true);
		//ovrMessage message;
		//ovrMessage_Init(&message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED);
		//ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);

		//return (jlong)((size_t)appThread);

		return 10;
	}

	JNIEXPORT void JNICALL Java_com_dotquest_quest_DotQuestJNI_onStart(JNIEnv* env, jobject obj, jlong handle, jobject obj1) {
		LOGV("    DotQuestJNI::onStart()");
		_shutdownCallbackObj = (jobject)env->NewGlobalRef(obj1);
		jclass callbackClass = env->GetObjectClass(_shutdownCallbackObj);
		_android_shutdown = env->GetMethodID(callbackClass, "shutdown", "()V");
		//ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
		//ovrMessage message;
		//ovrMessage_Init(&message, MESSAGE_ON_START, MQ_WAIT_PROCESSED);
		//ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	}

	JNIEXPORT void JNICALL Java_com_dotquest_quest_DotQuestJNI_onResume(JNIEnv* env, jobject obj, jlong handle) {
		LOGV("    DotQuestJNI::onResume()");
		//ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
		//ovrMessage message;
		//ovrMessage_Init(&message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED);
		//ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	}

	JNIEXPORT void JNICALL Java_com_dotquest_quest_DotQuestJNI_onPause(JNIEnv* env, jobject obj, jlong handle) {
		LOGV("    DotQuestJNI::onPause()");
		//ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
		//ovrMessage message;
		//ovrMessage_Init(&message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED);
		//ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	}

	JNIEXPORT void JNICALL Java_com_dotquest_quest_DotQuestJNI_onStop(JNIEnv* env, jobject obj, jlong handle) {
		LOGV("    DotQuestJNI::onStop()");
		//ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
		//ovrMessage message;
		//ovrMessage_Init(&message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED);
		//ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	}

	JNIEXPORT void JNICALL Java_com_dotquest_quest_DotQuestJNI_onDestroy(JNIEnv* env, jobject obj, jlong handle) {
		LOGV("    DotQuestJNI::onDestroy()");
		//ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
		//ovrMessage message;
		//ovrMessage_Init(&message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED);
		//ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
		//ovrMessageQueue_Enable(&appThread->MessageQueue, false);
		//ovrAppThread_Destroy(appThread, env);
		//free(appThread);
	}

	/*
	================================================================================

	Surface lifecycle

	================================================================================
	*/

	JNIEXPORT void JNICALL Java_com_com_dotquest_quest_DotQuestJNI_onSurfaceCreated(JNIEnv* env, jobject obj, jlong handle, jobject surface) {
		LOGV("    DotQuestJNI::onSurfaceCreated()");
		/*
		ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);

		ANativeWindow* newNativeWindow = ANativeWindow_fromSurface(env, surface);
		if (ANativeWindow_getWidth(newNativeWindow) < ANativeWindow_getHeight(newNativeWindow)) {
			// An app that is relaunched after pressing the home button gets an initial surface with
			// the wrong orientation even though android:screenOrientation="landscape" is set in the
			// manifest. The choreographer callback will also never be called for this surface because
			// the surface is immediately replaced with a new surface with the correct orientation.
			LOGE("        Surface not in landscape mode!");
		}

		LOGV("        NativeWindow = ANativeWindow_fromSurface( env, surface )");
		appThread->NativeWindow = newNativeWindow;
		ovrMessage message;
		ovrMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
		ovrMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
		ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
		*/
	}

	JNIEXPORT void JNICALL Java_com_dotquest_quest_DotQuestJNI_onSurfaceChanged(JNIEnv* env, jobject obj, jlong handle, jobject surface) {
		LOGV("    DotQuestJNI::onSurfaceChanged()");
		/*
		ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);

		ANativeWindow* newNativeWindow = ANativeWindow_fromSurface(env, surface);
		if (ANativeWindow_getWidth(newNativeWindow) < ANativeWindow_getHeight(newNativeWindow)) {
			// An app that is relaunched after pressing the home button gets an initial surface with
			// the wrong orientation even though android:screenOrientation="landscape" is set in the
			// manifest. The choreographer callback will also never be called for this surface because
			// the surface is immediately replaced with a new surface with the correct orientation.
			LOGE("        Surface not in landscape mode!");
		}

		if (newNativeWindow != appThread->NativeWindow) {
			if (appThread->NativeWindow != NULL) {
				ovrMessage message;
				ovrMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
				ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
				ALOGV("        ANativeWindow_release( NativeWindow )");
				ANativeWindow_release(appThread->NativeWindow);
				appThread->NativeWindow = NULL;
			}
			if (newNativeWindow != NULL) {
				ALOGV("        NativeWindow = ANativeWindow_fromSurface( env, surface )");
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
		*/
	}

	JNIEXPORT void JNICALL Java_com_dotquest_quest_DotQuestJNI_onSurfaceDestroyed(JNIEnv* env, jobject obj, jlong handle) {
		LOGV("    DotQuestJNI::onSurfaceDestroyed()");
		/*
		ovrAppThread* appThread = (ovrAppThread*)((size_t)handle);
		ovrMessage message;
		ovrMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
		ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
		ALOGV("        ANativeWindow_release( NativeWindow )");
		ANativeWindow_release(appThread->NativeWindow);
		appThread->NativeWindow = NULL;
		*/
	}
}


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

