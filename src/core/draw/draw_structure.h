//
// Created by William on 2024-08-25.
//

#ifndef DRAW_STRUCTURE_H
#define DRAW_STRUCTURE_H
#include "../engine.h"
#include "../scene.h"


class GltfMetallicRoughness {
public:
    GltfMetallicRoughness(Engine* engine, Scene* scene, const char* vertShaderPath, const char* fragShaderPath);
    ~GltfMetallicRoughness();
    void buildBuffers();

private:
    Engine* creator;
    static VkDescriptorSetLayout bufferAddressesDescriptorSetLayout;
    static VkDescriptorSetLayout textureDescriptorSetLayout;
    static VkDescriptorSetLayout computeCullingDescriptorSetLayout;
    static VkPipelineLayout _computeCullingPipelineLayout;
    static VkPipeline _computeCullingPipeline;
    static bool staticsInitialized;
    static int useCount;

    static void initializeStatics(Engine* engine);
};



#endif //DRAW_STRUCTURE_H
