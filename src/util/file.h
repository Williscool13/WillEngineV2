//
// Created by William on 2025-01-24.
//

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <filesystem>

#include "src/core/scene/serializer.h"

namespace will_engine::file
{
static const std::filesystem::path imagesSavePath = std::filesystem::current_path() / "assets" / "images";

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
    auto relPath = relative(fullPath, std::filesystem::current_path() / "assets", ec);

    if (ec) return fullPath.string();

    return "assets" / relPath;
}

static std::vector<std::filesystem::path> findWillFiles(const std::filesystem::path& dir, const char* extension)
{
    std::vector<std::filesystem::path> models;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.path().extension() == extension) {
            models.push_back(entry.path());
        }
    }
    return models;
}

static std::filesystem::path getSampleScene()
{
    const std::filesystem::path devSampleScenePath = "../assets/maps/sampleScene.willmap";
    const std::filesystem::path releaseSampleScenePath = "assets/maps/sampleScene.willmap";

    if (exists(devSampleScenePath)) {
        return devSampleScenePath;
    }

    if (exists(releaseSampleScenePath)) {
        return releaseSampleScenePath;
    }

    fmt::print("Failed to find sample scene, perhaps assets folder wasn't copied?\n");
    throw std::runtime_error("Failed to find sample scene, perhaps assets folder wasn't copied?");
}

static uint32_t computePathHash(const std::filesystem::path& path)
{
    const std::string normalizedPath = path.lexically_normal().string();
    return std::hash<std::string>{}(normalizedPath);
}
}

#endif //FILE_UTILS_H
