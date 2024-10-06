//
// Created by William on 2024-10-06.
//

#include "file_utils.h"
bool file_utils::getOrCreateDirectory(std::filesystem::path path)
{
    if (std::filesystem::exists(path)) {
        return true;
    }

    if (std::filesystem::create_directory(path)) {
        return true;
    }

    return false;
}
