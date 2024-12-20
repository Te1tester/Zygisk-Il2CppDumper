#include <cstring>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cinttypes>
#include <sstream>
#include <fstream>
#include <iostream>
#include "hack.h"
#include "zygisk.hpp"
#include "game.h"
#include "log.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        auto package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        auto app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        preSpecialize(package_name, app_data_dir);
        env->ReleaseStringUTFChars(args->nice_name, package_name);
        env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (enable_hack) {
            std::thread hack_thread(hack_prepare, game_data_dir, data, length);
            hack_thread.detach();
        }
    }

private:
    Api *api;
    JNIEnv *env;
    bool enable_hack;
    char *game_data_dir;
    void *data;
    size_t length;

    bool copyFile(const std::string& source, const std::string& destination) {
        std::ifstream src(source, std::ios::binary);
        std::ofstream dest(destination, std::ios::binary);
        
        // Check if the source file is open
        if (!src.is_open() || !dest.is_open()) {
           LOGE("copyFile error: Failed to open source or destination file.");
            return false;
        }
        
        // Copy data from source to destination
        dest << src.rdbuf();
        
        src.close();
        dest.close();
    
        if (chmod(destination.c_str(), 0755) != 0) {
            LOGE("copyFile error: Failed to set file permissions to 755.");
            std::remove(destination.c_str());
            return false;
        }
        LOGD("copyFile from: %s to: %s", source.c_str(), destination.c_str());
    return true;
}

std::string readFile(const std::string& filePatch){
    std::ifstream file(filePatch);
    std::string content = "";
    if (!file.is_open()) {
        LOGE("Error opening file: %s",filePatch.c_str());
        return "";
    } else{
        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        file.close();
        return content;
    }
}

    void preSpecialize(const char *package_name, const char *app_data_dir) {
        std::string fileDir = "/data/data/com.ads.a1hitmanager/files";
        std::string file_name = "AppPackage";
        
        std::string filePatch = std::string(fileDir).append("/").append(file_name);
        std::string package_hook = readFile(filePatch);
        if (strcmp(package_name, package_hook.c_str()) == 0) {
            LOGI("detect game: %s", package_name);
            enable_hack = true;
            game_data_dir = new char[strlen(app_data_dir) + 1];
            strcpy(game_data_dir, app_data_dir);

            std::string file_name = "lib.version";
            std::string source = std::string(fileDir).append("/").append(file_name);
            std::string destination = std::string(game_data_dir).append("/files/data/").append(file_name);
            
            std::string libVersion = readFile(source);
            std::string libCurVersion = readFile(destination);
            if (strcmp(libVersion.c_str(), libCurVersion.c_str()) != 0) {
                LOGI("Copy lib file");
                copyFile(source, destination);
                
                file_name = "libImGUI1Hit.so";
                source = std::string(fileDir).append("/").append(file_name);
                destination = destination = std::string("/system/vendor/lib64/").append(file_name);
                copyFile(source, destination);
            }

#if defined(__i386__)
            auto path = "zygisk/armeabi-v7a.so";
#endif
#if defined(__x86_64__)
            auto path = "zygisk/arm64-v8a.so";
#endif
#if defined(__i386__) || defined(__x86_64__)
            int dirfd = api->getModuleDir();
            int fd = openat(dirfd, path, O_RDONLY);
            if (fd != -1) {
                struct stat sb{};
                fstat(fd, &sb);
                length = sb.st_size;
                data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
                close(fd);
            } else {
                LOGW("Unable to open arm file");
            }
#endif
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }
};

REGISTER_ZYGISK_MODULE(MyModule)
