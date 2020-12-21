GL4ES_PATH := $(call my-dir)/../../../lib/gl4es

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  include $(GL4ES_PATH)/Android.mk
endif

ifeq ($(TARGET_ARCH_ABI),x86_64)
  include $(GL4ES_PATH)/Android.mk
endif