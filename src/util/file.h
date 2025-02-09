//
// Created by William on 2025-01-24.
//

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <filesystem>

namespace will_engine::file
{
static const std::filesystem::path imagesSavePath = std::filesystem::current_path() / "images";

static bool getOrCreateDirectory(const std::filesystem::path& path)
{
    if (exists(path)) {
        return true;
    }

    if (create_directory(path)) {
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

static std::filesystem::path getRelativePath(const std::filesystem::path& fullPath)
{
    if (fullPath.empty()) return "";

    std::error_code ec;
    auto relPath = relative(fullPath, std::filesystem::current_path(), ec);

    if (ec) return fullPath.string();

    return relPath;
}

static std::vector<std::filesystem::path> findWillmodels(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> models;
    for(const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if(entry.path().extension() == ".willmodel") {
            models.push_back(entry.path());
        }
    }
    return models;
}
}

#endif //FILE_UTILS_H
