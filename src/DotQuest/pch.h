#include <errno.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <android/log.h>

#define ALOG_TAG "GAMETAG"
#define ALOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, ALOG_TAG, __VA_ARGS__))
#define ALOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, ALOG_TAG, __VA_ARGS__))
#define ALOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, ALOG_TAG, __VA_ARGS__))
#if _DEBUG
#define ALOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#else
#define ALOGV(...)
#endif
