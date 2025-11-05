//
// Created by Perfare on 2020/7/4.
//

#include "game.h"
#include "hack.h"
#include "il2cpp_dump.h"
#include "xdl.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <jni.h>
#include <thread>
#include <sys/mman.h>
#include <linux/unistd.h>
#include <array>
#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>

#include "log.h"

void hack_start(const char *game_data_dir) {
    bool load = false;
    for (int i = 0; i < 10; i++) {
        void *handle = xdl_open("libil2cpp.so", 0);
        if (handle) {
            load = true;
            LOGI("hack_start");
            sleep(SLEEPTIME);
            il2cpp_api_init(handle);
            LOGI("il2cpp_api_init");
            il2cpp_dump(game_data_dir);
            LOGI("il2cpp_dump: %s", game_data_dir);
            break;
        } else {
            sleep(1);
        }
    }
    if (!load) {
        LOGI("libil2cpp.so not found in thread %d", gettid());
    }
}

std::string GetLibDir(JavaVM *vms) {
    JNIEnv *env = nullptr;
    vms->AttachCurrentThread(&env, nullptr);
    jclass activity_thread_clz = env->FindClass("android/app/ActivityThread");
    if (activity_thread_clz != nullptr) {
        jmethodID currentApplicationId = env->GetStaticMethodID(activity_thread_clz,
                                                                "currentApplication",
                                                                "()Landroid/app/Application;");
        if (currentApplicationId) {
            jobject application = env->CallStaticObjectMethod(activity_thread_clz,
                                                              currentApplicationId);
            jclass application_clazz = env->GetObjectClass(application);
            if (application_clazz) {
                jmethodID get_application_info = env->GetMethodID(application_clazz,
                                                                  "getApplicationInfo",
                                                                  "()Landroid/content/pm/ApplicationInfo;");
                if (get_application_info) {
                    jobject application_info = env->CallObjectMethod(application,
                                                                     get_application_info);
                    jfieldID native_library_dir_id = env->GetFieldID(
                            env->GetObjectClass(application_info), "nativeLibraryDir",
                            "Ljava/lang/String;");
                    if (native_library_dir_id) {
                        auto native_library_dir_jstring = (jstring) env->GetObjectField(
                                application_info, native_library_dir_id);
                        auto path = env->GetStringUTFChars(native_library_dir_jstring, nullptr);
                        LOGI("lib dir %s", path);
                        std::string lib_dir(path);
                        env->ReleaseStringUTFChars(native_library_dir_jstring, path);
                        return lib_dir;
                    } else {
                        LOGE("nativeLibraryDir not found");
                    }
                } else {
                    LOGE("getApplicationInfo not found");
                }
            } else {
                LOGE("application class not found");
            }
        } else {
            LOGE("currentApplication not found");
        }
    } else {
        LOGE("ActivityThread not found");
    }
    return {};
}

static std::string GetNativeBridgeLibrary() {
    auto value = std::array<char, PROP_VALUE_MAX>();
    __system_property_get("ro.dalvik.vm.native.bridge", value.data());
    return {value.data()};
}

struct NativeBridgeCallbacks {
    uint32_t version;
    void *initialize;

    void *(*loadLibrary)(const char *libpath, int flag);

    void *(*getTrampoline)(void *handle, const char *name, const char *shorty, uint32_t len);

    void *isSupported;
    void *getAppEnv;
    void *isCompatibleWith;
    void *getSignalHandler;
    void *unloadLibrary;
    void *getError;
    void *isPathSupported;
    void *initAnonymousNamespace;
    void *createNamespace;
    void *linkNamespaces;

    void *(*loadLibraryExt)(const char *libpath, int flag, void *ns);
};

bool NativeBridgeLoad(const char *game_data_dir, int api_level, void *data, size_t length) {
    //TODO 等待houdini初始化
    sleep(3);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // void* libart = 0;
    // while (libart == 0){
    //     libart = dlopen("libart.so", RTLD_NOW);
    //     LOGI("try load libil2cpp");
    // }

jint (*JNI_GetCreatedJavaVMs)(JavaVM **, jsize, jsize *) = nullptr;

// Try up to 10 seconds (100 * 100ms)
for (int i = 0; i < 100; ++i) {
    JNI_GetCreatedJavaVMs = (jint (*)(JavaVM **, jsize, jsize *))
        dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs");

    if (JNI_GetCreatedJavaVMs) {
        LOGI("JNI_GetCreatedJavaVMs found at %p", JNI_GetCreatedJavaVMs);
        break;
    }

    LOGI("Waiting for JNI_GetCreatedJavaVMs... (%d)", i);
    usleep(100 * 1000); // 100ms
}

if (!JNI_GetCreatedJavaVMs) {
    LOGE("JNI_GetCreatedJavaVMs not found after waiting!");
    return false;
}
    LOGI("JNI_GetCreatedJavaVMs %p", JNI_GetCreatedJavaVMs);
    JavaVM *vms_buf[1];
    JavaVM *vms;
    jsize num_vms;
    jint status = JNI_GetCreatedJavaVMs(vms_buf, 1, &num_vms);
    if (status == JNI_OK && num_vms > 0) {
        vms = vms_buf[0];
    } else {
        LOGE("GetCreatedJavaVMs error");
        return false;
    }

    auto lib_dir = GetLibDir(vms);

    if (lib_dir.empty()) {
        LOGE("GetLibDir error");
        return false;
    }
    if (lib_dir.find("/lib/x86") != std::string::npos) {
        LOGI("no need NativeBridge");
        munmap(data, length);
        return false;
    }

    auto nb = dlopen("libhoudini.so", RTLD_NOW);
    LOGI("libhoudini %p", nb);
    if (!nb) {
        auto native_bridge = GetNativeBridgeLibrary();
        LOGI("native bridge: %s", native_bridge.data());
        nb = dlopen(native_bridge.data(), RTLD_NOW);
    }
    if (nb) {
        LOGI("nb %p", nb);
        auto callbacks = (NativeBridgeCallbacks *) dlsym(nb, "NativeBridgeItf");
        if (callbacks) {
            LOGI("NativeBridgeLoadLibrary %p", callbacks->loadLibrary);
            LOGI("NativeBridgeLoadLibraryExt %p", callbacks->loadLibraryExt);
            LOGI("NativeBridgeGetTrampoline %p", callbacks->getTrampoline);

            int fd = syscall(__NR_memfd_create, "anon", MFD_CLOEXEC);
            ftruncate(fd, (off_t) length);
            void *mem = mmap(nullptr, length, PROT_WRITE, MAP_SHARED, fd, 0);
            memcpy(mem, data, length);
            munmap(mem, length);
            munmap(data, length);
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "/proc/self/fd/%d", fd);
            LOGI("arm path %s", path);

            void *arm_handle;
            if (api_level >= 26) {
                arm_handle = callbacks->loadLibraryExt(path, RTLD_NOW, (void *) 3);
            } else {
                arm_handle = callbacks->loadLibrary(path, RTLD_NOW);
            }
            if (arm_handle) {
                LOGI("arm handle %p", arm_handle);
                auto init = (void (*)(JavaVM *, void *)) callbacks->getTrampoline(arm_handle,
                                                                                  "JNI_OnLoad",
                                                                                  nullptr, 0);
                LOGI("JNI_OnLoad %p", init);
                LOGI("JNI_OnLoad patch: %s", path);
                init(vms, (void *) game_data_dir);
                return true;
            }
            close(fd);
        }
    }
    return false;
}

bool LoadArmLibrary() {
    sleep(3);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    void* nblib = 0;
    while (nblib == 0){
        nblib = dlopen("libnativebridge.so", RTLD_NOW);
        LOGI("try load libnativebridge");
    }
    
    if (!nblib) {
        LOGI("Failed to load libnativebridge.so: %s", dlerror());
        return false;
    }

    auto GetCallbacks = (const NativeBridgeCallbacks*(*)())dlsym(nblib, "NativeBridgeGetCallbacks");
    if (!GetCallbacks) {
        LOGI("NativeBridgeGetCallbacks not found");
        return false;
    }

    const NativeBridgeCallbacks* cb = GetCallbacks();
    if (!cb) {
        LOGI("NativeBridge callbacks = null");
        return false;
    }

    LOGI("Using NativeBridge via system Houdini");
    LOGI("loadLibrary: %p", cb->loadLibrary);
    LOGI("loadLibraryExt: %p", cb->loadLibraryExt);

    void* arm_handle = nullptr;
    if (cb->loadLibraryExt)
        arm_handle = cb->loadLibraryExt("/data/local/tmp/OHit/libTool.so", RTLD_NOW, (void*)3);
    else if (cb->loadLibrary)
        arm_handle = cb->loadLibrary("/data/local/tmp/OHit/libTool.so", RTLD_NOW);

    LOGI("arm_handle = %p", arm_handle);
    return arm_handle != nullptr;
}

void hack_prepare(const char *game_data_dir, void *data, size_t length) {
    LOGI("hack thread: %d", gettid());
    int api_level = android_get_device_api_level();
    LOGI("api level: %d", api_level);
#if defined(__i386__) || defined(__x86_64__)
    LoadArmLibrary();
    // if (!LoadArmLibrary()) {
    if (!NativeBridgeLoad(game_data_dir, api_level, data, length)) {
#endif
        hack_start(game_data_dir);
#if defined(__i386__) || defined(__x86_64__)
    }
#endif
}

#if defined(__arm__) || defined(__aarch64__)

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    auto game_data_dir = (const char *) reserved;
    std::string libName = "libTool.so";
    std::string libPath = std::string("/data/local/tmp/OHit/").append(libName);

    void* handle = dlopen(libPath.c_str(), RTLD_NOW);
    if (!handle) {
        // If dlopen fails, print the error message
       LOGI("Error loading library: %s", dlerror());
        void* hokLib = dlopen(libName.c_str(), RTLD_NOW);
         if (hokLib) {
            LOGI("LoadLib done");
         } else{
              LOGI("Error loading library: %s", dlerror());
         }
    }
    // dlopen typically calls JNI_OnLoad automatically, but we can verify or manually invoke if needed
    typedef jint (*JNI_OnLoadFn)(JavaVM *, void *);
    JNI_OnLoadFn liba_onload = (JNI_OnLoadFn)dlsym(handle, "JNI_OnLoad");
    if (liba_onload) {
        LOGD("Calling liba.so JNI_OnLoad");
        jint result = liba_onload(vm, reserved);
        LOGD("liba.so JNI_OnLoad returned %d", result);
    } else {
        LOGD("liba.so JNI_OnLoad not found, assuming auto-called by dlopen");
    }
    //std::thread hack_thread(hack_start, game_data_dir);
    //hack_thread.detach();
    return JNI_VERSION_1_6;
}

#endif
