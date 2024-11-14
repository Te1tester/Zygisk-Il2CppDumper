//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_HACK_H
#define ZYGISK_IL2CPPDUMPER_HACK_H

#include <stddef.h>
#include <sstream>
#include <fstream>
#include <iostream>

void hack_prepare(const char *game_data_dir, void *data, size_t length);
std::string readFile(const std::string& filePatch);
bool copyFile(const std::string& source, const std::string& destination);

#endif //ZYGISK_IL2CPPDUMPER_HACK_H
