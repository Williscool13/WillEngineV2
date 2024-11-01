//
// Created by William on 2024-10-06.
//

#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <filesystem>
#include <cstring>

namespace file_utils
{
    static const std::filesystem::path imagesSavePath = std::filesystem::current_path() / "images";

    static bool getOrCreateDirectory(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path)) {
            return true;
        }

        if (std::filesystem::create_directory(path)) {
            return true;
        }

        return false;
    }

    static const char* getFileName(const char* path)
    {
        static std::string filename; // Static to keep string alive
        filename = std::filesystem::path(path).filename().string();
        return filename.c_str();
    }

    static const char* getFileName(const std::string_view path)
    {
        static std::string filename; // Static to keep string alive
        filename = std::filesystem::path(path).filename().string();
        return filename.c_str();
    }
};


#endif //FILE_UTILS_H
