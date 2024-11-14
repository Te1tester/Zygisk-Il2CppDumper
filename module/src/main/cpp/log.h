//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_LOG_H
#define ZYGISK_IL2CPPDUMPER_LOG_H

#include <android/log.h>

#define USELOGFUNCTION

#define LOG_TAG "Perfare"
#ifdef USELOGFUNCTION
  #define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
  #define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
  #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
  #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
  #define LOGD(...) // do nothing
  #define LOGW(...) // do nothing
  #define LOGE(...) // do nothing
  #define LOGI(...) // do nothing
#endif
#endif //ZYGISK_IL2CPPDUMPER_LOG_H
