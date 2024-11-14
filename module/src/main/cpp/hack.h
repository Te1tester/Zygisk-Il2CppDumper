//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_HACK_H
#define ZYGISK_IL2CPPDUMPER_HACK_H

#include <stddef.h>
#include <sstream>
#include <fstream>
#include <iostream>

#include "log.h"

void hack_prepare(const char *game_data_dir, void *data, size_t length);
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

#endif //ZYGISK_IL2CPPDUMPER_HACK_H
