//
// Created by William on 2024-10-06.
//

#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <filesystem>

namespace file_utils {
    static const std::filesystem::path imagesSavePath = std::filesystem::current_path() / "images";

    bool getOrCreateDirectory(std::filesystem::path path);
};



#endif //FILE_UTILS_H
