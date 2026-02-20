#pragma once
#include "config.hpp"

inline std::vector<char> readFile(std::string filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    size_t fileSize = file.tellg();
    std::vector<char> buff(fileSize);
    file.seekg(0);
    file.read(buff.data(), fileSize);
    file.close();
    return buff;
}

#define VK_CHECK(x)                                                                         \
    {                                                                                       \
        VkResult res = VkResult(x);                                                         \
        if (res!=VK_SUCCESS) {                                                              \
            throw std::runtime_error(std::string("VK Error:") + string_VkResult(res));      \
        }                                                                                   \
    }