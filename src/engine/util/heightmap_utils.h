//
// Created by William on 2025-02-23.
//

#ifndef HEIGHTMAP_UTILS_H
#define HEIGHTMAP_UTILS_H

#include <random>

#include <glm/glm.hpp>
#include <FastNoiseLite.h>

#include "engine/renderer/resource_manager.h"

namespace will_engine
{
constexpr uint32_t NOISE_MAP_DIMENSIONS = 512;
struct NoiseSettings
{
    float scale = 50.0f;
    float persistence = 0.5f;
    float lacunarity = 2.0f;
    int octaves = 6;
    glm::vec2 offset{0.0f};
    float heightScale = 50.0f;
};

class HeightmapUtil
{
public:
    static std::vector<float> generateFromNoise(const uint32_t width, const uint32_t height, const uint32_t seed, const NoiseSettings& settings = NoiseSettings{})
    {
        std::vector<float> heightData(width * height);
        std::vector<glm::vec2> octaveOffsets(settings.octaves);
        std::mt19937 rng(seed);
        std::uniform_real_distribution dist(-100000.0f, 100000.0f);

        for (int i = 0; i < settings.octaves; i++) {
            const float offsetX = dist(rng) + settings.offset.x;
            const float offsetY = dist(rng) + settings.offset.y;
            octaveOffsets[i] = glm::vec2(offsetX, offsetY);
        }

        float maxHeight = std::numeric_limits<float>::lowest();
        float minHeight = std::numeric_limits<float>::max();

        FastNoiseLite noise;
        noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        noise.SetSeed(seed);
        noise.SetFrequency(1.0f);

        const float halfWidth = width / 2.0f;
        const float halfHeight = height / 2.0f;
        const float invScale = 1.0f / settings.scale;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float amplitude = 1.0f;
                float frequency = 1.0f;
                float noiseHeight = 0.0f;

                // Accumulate noise from each octave
                for (int i = 0; i < settings.octaves; i++) {
                    const float sampleX = (x - halfWidth) * invScale * frequency + octaveOffsets[i].x;
                    const float sampleY = (y - halfHeight) * invScale * frequency + octaveOffsets[i].y;

                    const float perlinValue = noise.GetNoise(sampleX, sampleY);
                    noiseHeight += perlinValue * amplitude;

                    amplitude *= settings.persistence;
                    frequency *= settings.lacunarity;
                }

                heightData[y * width + x] = noiseHeight;

                maxHeight = std::max(maxHeight, noiseHeight);
                minHeight = std::min(minHeight, noiseHeight);
            }
        }

        for (uint32_t i = 0; i < width * height; i++) {
            heightData[i] = ((heightData[i] - minHeight) / (maxHeight - minHeight)) * settings.heightScale;
        }

        return heightData;
    }

    static std::vector<float> generateRawPerlinNoise(const uint32_t width, const uint32_t height, const uint32_t seed = 123456u, const float scale = 50.0f)
    {
        std::vector<float> noiseData(width * height);

        FastNoiseLite noise;
        noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        noise.SetSeed(seed);
        noise.SetFrequency(1.0f);

        const float invScale = 1.0f / scale;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                noiseData[y * width + x] = noise.GetNoise(x * invScale, y * invScale);
            }
        }

        return noiseData;
    }

    static std::vector<float> generateRawSimplexNoise(const uint32_t width, const uint32_t height, const uint32_t seed = 123456u, const float scale = 50.0f)
    {
        std::vector<float> noiseData(width * height);

        FastNoiseLite noise;
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
        noise.SetSeed(seed);
        noise.SetFrequency(1.0f);

        const float invScale = 1.0f / scale;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                noiseData[y * width + x] = noise.GetNoise(x * invScale, y * invScale);
            }
        }

        return noiseData;
    }

    static AllocatedImage createHeightmapImage(const ResourceManager& resourceManager, const std::vector<float>& heightData, const uint32_t width, const uint32_t height)
    {
        const VkExtent3D imageExtent{width, height, 1};
        return resourceManager.createImage(
            heightData.data(),
            heightData.size() * sizeof(float),
            imageExtent,
            VK_FORMAT_R32_SFLOAT,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        );
    }

    // static std::vector<float> loadFromImage(const std::string& filename);

    // static void saveToImage(const std::vector<float>& heightData, uint32_t width, uint32_t height, const std::string& filename);
};
} // namespace will_engine::util


#endif //HEIGHTMAP_UTILS_H
