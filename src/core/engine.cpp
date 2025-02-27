//
// Created by William on 8/11/2024.
//

#include "engine.h"

#include <thread>

#include <vk-bootstrap/VkBootstrap.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

#include "identifier/identifier_manager.h"
#include "src/physics/physics_filters.h"

#include "camera/free_camera.h"
#include "game_object/game_object.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "src/core/input.h"
#include "src/core/time.h"
#include "src/physics/physics.h"
#include "src/physics/physics_utils.h"
#include "src/renderer/immediate_submitter.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/render_object/render_object.h"
#include "src/renderer/descriptor_buffer/descriptor_buffer_uniform.h"
#include "src/renderer/environment/environment.h"
#include "src/renderer/lighting/shadows/cascaded_shadow_map.h"
#include "src/renderer/pipelines/deferred_mrt/deferred_mrt.h"
#include "src/renderer/pipelines/deferred_resolve/deferred_resolve.h"
#include "src/renderer/pipelines/environment/environment_pipeline.h"
#include "src/renderer/pipelines/visibility_pass/visibility_pass.h"
#include "src/renderer/pipelines/post_process/post_process_pipeline.h"
#include "src/renderer/pipelines/temporal_antialiasing_pipeline/temporal_antialiasing_pipeline.h"
#include "src/renderer/pipelines/terrain/terrain_pipeline.h"
#include "src/renderer/terrain/terrain_chunk.h"
#include "src/renderer/terrain/terrain_manager.h"
#include "src/util/file.h"
#include "src/util/halton.h"
#include "src/util/heightmap_utils.h"

#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif

#ifdef NDEBUG
// uncapped FPS
#define PRESENT_MODE VK_PRESENT_MODE_IMMEDIATE_KHR
#else
// vsync
#define PRESENT_MODE VK_PRESENT_MODE_FIFO_KHR
#endif

namespace will_engine
{
Engine* Engine::instance = nullptr;

void Engine::init()
{
    if (instance != nullptr) {
        throw std::runtime_error("More than 1 engine instance created, this is not allowed.");
    }
    instance = this;
    fmt::print("----------------------------------------\n");
    fmt::print("Initializing {}\n", ENGINE_NAME);
    const auto start = std::chrono::system_clock::now();


    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);
    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE;
    windowExtent = {1920, 1080};

    //auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    window = SDL_CreateWindow(
        ENGINE_NAME,
        static_cast<int>(windowExtent.width),
        static_cast<int>(windowExtent.height),
        window_flags);

    Input::Get().init(window);


    startupProfiler.addTimer("0Context");
    startupProfiler.addTimer("1Immediate");
    startupProfiler.addTimer("2ResourceManager");
    startupProfiler.addTimer("3Physics");
    startupProfiler.addTimer("4Environment");
    startupProfiler.addTimer("5Load Environment 0");
    startupProfiler.addTimer("6Load Environment 1");
    startupProfiler.addTimer("7Load CSM");
    startupProfiler.addTimer("8Init Renderer");
    startupProfiler.addTimer("9Init Game");

    startupProfiler.beginTimer("0Context");
    context = new VulkanContext(window, USE_VALIDATION_LAYERS);
    startupProfiler.endTimer("0Context");

    createSwapchain(windowExtent.width, windowExtent.height);
    createDrawResources();

    // Command Pools
    const VkCommandPoolCreateInfo commandPoolInfo = vk_helpers::commandPoolCreateInfo(context->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    for (auto& frame : frames) {
        VK_CHECK(vkCreateCommandPool(context->device, &commandPoolInfo, nullptr, &frame._commandPool));
        VkCommandBufferAllocateInfo cmdAllocInfo = vk_helpers::commandBufferAllocateInfo(frame._commandPool);
        VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdAllocInfo, &frame._mainCommandBuffer));
    }

    // Sync Structures
    const VkFenceCreateInfo fenceCreateInfo = vk_helpers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = vk_helpers::semaphoreCreateInfo();
    for (auto& frame : frames) {
        VK_CHECK(vkCreateFence(context->device, &fenceCreateInfo, nullptr, &frame._renderFence));

        VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));
    }


    startupProfiler.beginTimer("1Immediate");
    immediate = new ImmediateSubmitter(*context);
    startupProfiler.endTimer("1Immediate");

    startupProfiler.beginTimer("2ResourceManager");
    resourceManager = new ResourceManager(*context, *immediate);
    startupProfiler.endTimer("2ResourceManager");

    identifierManager = new identifier::IdentifierManager();
    identifier::IdentifierManager::Set(identifierManager);

    startupProfiler.beginTimer("3Physics");
    physics::Physics::set(new physics::Physics());
    startupProfiler.endTimer("3Physics");

    startupProfiler.beginTimer("4Environment");
    environmentMap = new environment::Environment(*resourceManager, *immediate);
    startupProfiler.endTimer("4Environment");

    auto& factory = components::ComponentFactory::getInstance();
    factory.registerComponents();

    const std::filesystem::path envMapSource = "assets/environments";

    startupProfiler.beginTimer("5Load Environment 0");
    environmentMap->loadEnvironment("Overcast Sky", (envMapSource / "kloofendal_overcast_puresky_4k.hdr").string().c_str(), 0);
    startupProfiler.endTimer("5Load Environment 0");

    startupProfiler.beginTimer("6Load Environment 1");
    environmentMap->loadEnvironment("Wasteland", (envMapSource / "wasteland_clouds_puresky_4k.hdr").string().c_str(), 1);
    startupProfiler.endTimer("6Load Environment 1");

    startupProfiler.beginTimer("7Load CSM");
    cascadedShadowMap = new cascaded_shadows::CascadedShadowMap(*resourceManager);
    startupProfiler.endTimer("7Load CSM");

    imguiWrapper = new ImguiWrapper(*context, {window, swapchainImageFormat});


    startupProfiler.beginTimer("8Init Renderer");
    initRenderer();
    startupProfiler.endTimer("8Init Renderer");

    startupProfiler.beginTimer("9Init Game");
    initGame();
    startupProfiler.endTimer("9Init Game");

    profiler.addTimer("0Physics");
    profiler.addTimer("1Game");
    profiler.addTimer("2Render");
    profiler.addTimer("3Total");

    constexpr float terrainScale = 100.0f;
    constexpr float terrainHeightScale = 50.0f;
    NoiseSettings settings{
        .scale = terrainScale,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .octaves = 4,
        .offset = {0.0f, 0.0f},
        .heightScale = terrainHeightScale
    };
    heightMapData = HeightmapUtil::generateFromNoise(512, 512, 13, settings);
    heightMap = HeightmapUtil::createHeightmapImage(*resourceManager, heightMapData, 512, 512);
    mainTerrainChunk = new terrain::TerrainChunk(*resourceManager, heightMapData, 512, 512);

    physics::Physics* physics = physics::Physics::get();
    constexpr float halfWidth = static_cast<float>(512 - 1) * 0.5f;
    constexpr float halfHeight = static_cast<float>(512 - 1) * 0.5f;
    JPH::HeightFieldShapeSettings heightFieldSettings{
        heightMapData.data(),
        JPH::Vec3(-halfWidth, 0.0f, -halfHeight),
        JPH::Vec3(1.0f, 1.0f, 1.0f),
        512,
        {},

    };
    JPH::ShapeSettings::ShapeResult result = heightFieldSettings.Create();
    if (!result.IsValid()) {
        fmt::print("Failed to create terrain collision shape: {}\n", result.GetError());
        assert(false);
        return;
    }

    terrainShape = result.Get();
    JPH::BodyCreationSettings bodySettings(
        terrainShape,
        physics::PhysicsUtils::toJolt(glm::vec3(0.0f)),
        physics::PhysicsUtils::toJolt(glm::quat{1.0f, 0.0f, 0.0f, 0.0f}),
        JPH::EMotionType::Static,
        physics::Layers::TERRAIN
    );

    terrainBodyId = physics->getBodyInterface().CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    fmt::print("Finished Initialization in {} seconds\n", static_cast<float>(elapsed.count()) / 1000000.0f);
}

void Engine::initRenderer()
{
    for (int i{0}; i < FRAME_OVERLAP; i++) {
        sceneDataBuffers[i] = resourceManager->createHostSequentialBuffer(sizeof(SceneData));
    }
    sceneDataDescriptorBuffer = DescriptorBufferUniform(*context, resourceManager->getSceneDataLayout(), FRAME_OVERLAP + 1);
    std::vector<DescriptorUniformData> sceneDataBufferData{1};
    for (int i{0}; i < FRAME_OVERLAP; i++) {
        sceneDataBufferData[0] = DescriptorUniformData{.uniformBuffer = sceneDataBuffers[i], .allocSize = sizeof(SceneData)};
        sceneDataDescriptorBuffer.setupData(context->device, sceneDataBufferData, i);
    }
    debugSceneDataBuffer = resourceManager->createHostSequentialBuffer(sizeof(SceneData));
    sceneDataBufferData[0] = DescriptorUniformData{.uniformBuffer = debugSceneDataBuffer, .allocSize = sizeof(SceneData)};
    sceneDataDescriptorBuffer.setupData(context->device, sceneDataBufferData, FRAME_OVERLAP);

    visibilityPassPipeline = new visibility_pass::VisibilityPassPipeline(*resourceManager);
    environmentPipeline = new environment_pipeline::EnvironmentPipeline(*resourceManager, environmentMap->getCubemapDescriptorSetLayout());
    terrainPipeline = new terrain::TerrainPipeline(*resourceManager);
    deferredMrtPipeline = new deferred_mrt::DeferredMrtPipeline(*resourceManager);
    deferredResolvePipeline = new deferred_resolve::DeferredResolvePipeline(*resourceManager, environmentMap->getDiffSpecMapDescriptorSetlayout(),
                                                                            cascadedShadowMap->getCascadedShadowMapUniformLayout(), cascadedShadowMap->getCascadedShadowMapSamplerLayout());
    temporalAntialiasingPipeline = new temporal_antialiasing_pipeline::TemporalAntialiasingPipeline(*resourceManager);
    postProcessPipeline = new post_process_pipeline::PostProcessPipeline(*resourceManager);

    const deferred_resolve::DeferredResolveDescriptor deferredResolveDescriptor{
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        depthImage.imageView,
        velocityRenderTarget.imageView,
        drawImage.imageView,
        resourceManager->getDefaultSamplerLinear()
    };
    deferredResolvePipeline->setupDescriptorBuffer(deferredResolveDescriptor);

    const temporal_antialiasing_pipeline::TemporalAntialiasingDescriptor temporalAntialiasingDescriptor{
        drawImage.imageView,
        historyBuffer.imageView,
        depthImage.imageView,
        velocityRenderTarget.imageView,
        taaResolveTarget.imageView,
        resourceManager->getDefaultSamplerLinear()

    };
    temporalAntialiasingPipeline->setupDescriptorBuffer(temporalAntialiasingDescriptor);

    const post_process_pipeline::PostProcessDescriptor postProcessDescriptor{
        taaResolveTarget.imageView,
        postProcessOutputBuffer.imageView,
        resourceManager->getDefaultSamplerLinear()
    };
    postProcessPipeline->setupDescriptorBuffer(postProcessDescriptor);
}

void Engine::initGame()
{
    const auto sceneRoot = new GameObject("Scene Root");
    scene = new Scene(sceneRoot);
    file::scanForModels(renderObjectInfoMap);
    camera = new FreeCamera();
    Serializer::deserializeScene(scene->getRoot(), camera, file::getSampleScene().string());
}

void Engine::run()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Running {}\n", ENGINE_NAME);

    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        Input& input = Input::Get();
        Time& time = Time::Get();
        input.frameReset();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { bQuit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { bQuit = true; }


            if (e.type == SDL_EVENT_WINDOW_MINIMIZED) { bStopRendering = true; }
            if (e.type == SDL_EVENT_WINDOW_RESTORED) { bStopRendering = true; }
            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bResizeRequested = true;
                fmt::print("Window resized, resize requested\n");
            }

            imguiWrapper->handleInput(e);
            input.processEvent(e);
        }

        input.updateFocus(SDL_GetWindowFlags(window));
        time.update();

        if (bResizeRequested) {
            resizeSwapchain();
        }

        // Minimized
        if (bStopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        imguiWrapper->imguiInterface(this);

        const float deltaTime = Time::Get().getDeltaTime();
        profiler.beginTimer("3Total");

        profiler.beginTimer("0Physics");
        physics::Physics::get()->update(deltaTime);
        profiler.endTimer("0Physics");

        profiler.beginTimer("1Game");
        updateGame(deltaTime);
        profiler.endTimer("1Game");

        draw(deltaTime);
        profiler.endTimer("3Total");
    }
}

void Engine::updateGame(const float deltaTime)
{
    if (camera) { camera->update(deltaTime); }

    const Input& input = Input::Get();
    if (input.isKeyPressed(SDLK_R)) {
        if (camera) {
            const glm::vec3 direction = camera->getForwardWS();
            //const physics::PlayerCollisionFilter dontHitPlayerFilter{};
            const physics::RaycastHit result = physics::PhysicsUtils::raycast(camera->getPosition(), direction, 100.0f, {}, {}, {});

            if (result.hasHit) {
                physics::PhysicsUtils::addImpulseAtPosition(result.hitBodyID, normalize(direction) * 100.0f, result.hitPosition);
            }
            else {
                fmt::print("Failed to find an object with the raycast\n");
            }
        }
    }

    for (IHierarchical* hierarchal : hierarchalBeginQueue) {
        hierarchal->beginPlay();
    }
    hierarchalBeginQueue.clear();

    scene->update(deltaTime);

    for (IHierarchical* hierarchical : hierarchicalDeletionQueue) {
        hierarchical->beginDestroy();
        delete hierarchical;
    }
    hierarchicalDeletionQueue.clear();
}

void Engine::updateRender(const float deltaTime, const int32_t currentFrameOverlap, const int32_t previousFrameOverlap) const
{
    const AllocatedBuffer& previousSceneDataBuffer = sceneDataBuffers[previousFrameOverlap];
    const AllocatedBuffer& sceneDataBuffer = sceneDataBuffers[currentFrameOverlap];
    const auto pSceneData = static_cast<SceneData*>(sceneDataBuffer.info.pMappedData);
    const auto pPreviousSceneData = static_cast<SceneData*>(previousSceneDataBuffer.info.pMappedData);

    const bool bIsFrameZero = frameNumber == 0;
    glm::vec2 prevJitter = HaltonSequence::getJitterHardcoded(bIsFrameZero ? frameNumber : frameNumber - 1) * 2.0f - 1.0f;
    prevJitter.x /= RENDER_EXTENT_WIDTH;
    prevJitter.y /= RENDER_EXTENT_HEIGHT;
    glm::vec2 currentJitter = HaltonSequence::getJitterHardcoded(frameNumber) * 2.0f - 1.0f;
    currentJitter.x /= RENDER_EXTENT_WIDTH;
    currentJitter.y /= RENDER_EXTENT_HEIGHT;

    pSceneData->prevView = bIsFrameZero ? camera->getViewMatrix() : pPreviousSceneData->view;
    pSceneData->prevProj = bIsFrameZero ? camera->getProjMatrix() : pPreviousSceneData->proj;
    pSceneData->prevViewProj = bIsFrameZero ? camera->getViewProjMatrix() : pPreviousSceneData->viewProj;
    pSceneData->jitter = bEnableTaa ? glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y) : glm::vec4(0.0f);

    pSceneData->view = camera->getViewMatrix();
    pSceneData->proj = camera->getProjMatrix();
    pSceneData->viewProj = pSceneData->proj * pSceneData->view;

    pSceneData->invView = glm::inverse(pSceneData->view);
    pSceneData->invProj = glm::inverse(pSceneData->proj);
    pSceneData->invViewProj = glm::inverse(pSceneData->viewProj);
    pSceneData->cameraWorldPos = camera->getPosition();
    const glm::mat4 cameraLook = glm::lookAt(glm::vec3(0), camera->getForwardWS(), glm::vec3(0, 1, 0));
    pSceneData->viewProjCameraLookDirection = pSceneData->proj * cameraLook;

    pSceneData->renderTargetSize = {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    pSceneData->deltaTime = deltaTime;


    const auto pDebugSceneData = static_cast<SceneData*>(debugSceneDataBuffer.info.pMappedData);
    pDebugSceneData->jitter = glm::vec4(0.0f);
    pDebugSceneData->view = glm::lookAt(glm::vec3(0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    pDebugSceneData->proj = camera->getProjMatrix();
    pDebugSceneData->viewProj = pDebugSceneData->proj * pDebugSceneData->view;

    pDebugSceneData->prevView = pDebugSceneData->view;
    pDebugSceneData->prevProj = pDebugSceneData->proj;
    pDebugSceneData->prevViewProj = pDebugSceneData->viewProj;

    pDebugSceneData->invView = glm::inverse(pDebugSceneData->view);
    pDebugSceneData->invProj = glm::inverse(pDebugSceneData->proj);
    pDebugSceneData->prevViewProj = pDebugSceneData->viewProj;
    pDebugSceneData->cameraWorldPos = glm::vec4(0.0f);
    pDebugSceneData->renderTargetSize = {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    pDebugSceneData->deltaTime = deltaTime;
}

void Engine::draw(float deltaTime)
{
    // GPU -> VPU sync (fence)
    VK_CHECK(vkWaitForFences(context->device, 1, &getCurrentFrame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(context->device, 1, &getCurrentFrame()._renderFence));

    // GPU -> GPU sync (semaphore)
    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(context->device, swapchain, 1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Acquire)\n");
        return;
    }

    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // only submit once
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    profiler.beginTimer("2Render");

    int32_t currentFrameOverlap = getCurrentFrameOverlap();
    int32_t previousFrameOverlap = getPreviousFrameOverlap();

    // Update Render Object Buffers and Model Matrices
    for (RenderObject* val : renderObjectMap | std::views::values) {
        val->update(currentFrameOverlap, previousFrameOverlap);
    }

    // Updates Scene Data buffer
    updateRender(deltaTime, currentFrameOverlap, previousFrameOverlap);

    // Updates Cascaded Shadow Map Properties
    cascadedShadowMap->update(mainLight, camera, currentFrameOverlap);

    visibility_pass::VisibilityPassDrawInfo csmFrustumCullDrawInfo{
        currentFrameOverlap,
        renderObjectMap,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        false,
        true,
    };
    visibilityPassPipeline->draw(cmd, csmFrustumCullDrawInfo);

    cascadedShadowMap->draw(cmd, renderObjectMap, currentFrameOverlap);

    vk_helpers::clearColorImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    environment_pipeline::EnvironmentDrawInfo environmentPipelineDrawInfo{
        drawImage.imageView,
        depthImage.imageView,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        environmentMap->getCubemapDescriptorBuffer().getDescriptorBufferBindingInfo(),
        environmentMap->getCubemapDescriptorBuffer().getDescriptorBufferSize() * environmentMapIndex,
    };
    environmentPipeline->draw(cmd, environmentPipelineDrawInfo);

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    visibility_pass::VisibilityPassDrawInfo deferredFrustumCullDrawInfo{
        currentFrameOverlap,
        renderObjectMap,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        true,
        false
    };
    visibilityPassPipeline->draw(cmd, deferredFrustumCullDrawInfo);

    terrain::TerrainDrawInfo terrainDrawInfo{
        true,
        currentFrameOverlap,
        {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT},
        {mainTerrainChunk},
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        velocityRenderTarget.imageView,
        depthImage.imageView,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };
    terrainPipeline->draw(cmd, terrainDrawInfo);

    const deferred_mrt::DeferredMrtDrawInfo deferredMrtDrawInfo{
        false,
        currentFrameOverlap,
        {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT},
        renderObjectMap,
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        velocityRenderTarget.imageView,
        depthImage.imageView,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };
    deferredMrtPipeline->draw(cmd, deferredMrtDrawInfo);

    if (bEnableDebugFrustumCullDraw) {
        const deferred_mrt::DeferredMrtDrawInfo debugDeferredMrtDrawInfo{
            false,
            currentFrameOverlap,
            {RENDER_EXTENT_WIDTH / 3.0f, RENDER_EXTENT_HEIGHT / 3.0f},
            renderObjectMap,
            normalRenderTarget.imageView,
            albedoRenderTarget.imageView,
            pbrRenderTarget.imageView,
            velocityRenderTarget.imageView,
            depthImage.imageView,
            sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
            sceneDataDescriptorBuffer.getDescriptorBufferSize() * FRAME_OVERLAP
        };
        deferredMrtPipeline->draw(cmd, debugDeferredMrtDrawInfo);
    }

    vk_helpers::transitionImage(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    const deferred_resolve::DeferredResolveDrawInfo deferredResolveDrawInfo{
        deferredDebug,
        csmPcf,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        environmentMap->getDiffSpecMapDescriptorBuffer().getDescriptorBufferBindingInfo(),
        environmentMap->getDiffSpecMapDescriptorBuffer().getDescriptorBufferSize() * environmentMapIndex,
        cascadedShadowMap->getCascadedShadowMapUniformBuffer().getDescriptorBufferBindingInfo(),
        cascadedShadowMap->getCascadedShadowMapUniformBuffer().getDescriptorBufferSize() * currentFrameOverlap,
        cascadedShadowMap->getCascadedShadowMapSamplerBuffer().getDescriptorBufferBindingInfo(),
        camera->getNearPlane(),
        camera->getFarPlane(),
    };
    deferredResolvePipeline->draw(cmd, deferredResolveDrawInfo);

    vk_helpers::transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    const VkImageLayout originLayout = frameNumber == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vk_helpers::transitionImage(cmd, historyBuffer.image, originLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    const temporal_antialiasing_pipeline::TemporalAntialiasingDrawInfo taaDrawInfo{
        0.1f,
        bEnableTaa ? 0 : 1,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };
    temporalAntialiasingPipeline->draw(cmd, taaDrawInfo);

    vk_helpers::transitionImage(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, historyBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, taaResolveTarget.image, historyBuffer.image, RENDER_EXTENTS, RENDER_EXTENTS);

    vk_helpers::transitionImage(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, postProcessOutputBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    postProcessPipeline->draw(cmd, post_process::PostProcessType::ALL);

    vk_helpers::transitionImage(cmd, postProcessOutputBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, postProcessOutputBuffer.image, swapchainImages[swapchainImageIndex], RENDER_EXTENTS, swapchainExtent);

    // draw ImGui into Swapchain Image
    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    imguiWrapper->drawImgui(cmd, swapchainImageViews[swapchainImageIndex], swapchainExtent);

    vk_helpers::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

    // End Command Buffer Recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    profiler.endTimer("2Render");

    // Submission
    const VkCommandBufferSubmitInfo cmdSubmitInfo = vk_helpers::commandBufferSubmitInfo(cmd);
    const VkSemaphoreSubmitInfo waitInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame()._swapchainSemaphore);
    const VkSemaphoreSubmitInfo signalInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);
    const VkSubmitInfo2 submit = vk_helpers::submitInfo(&cmdSubmitInfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(context->graphicsQueue, 1, &submit, getCurrentFrame()._renderFence));


    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &getCurrentFrame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(context->graphicsQueue, &presentInfo);

    //increase the number of frames drawn
    frameNumber++;

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Present)\n");
    }
}

void Engine::cleanup()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Cleaning up {}\n", ENGINE_NAME);

    vkDeviceWaitIdle(context->device);

    delete visibilityPassPipeline;
    delete environmentPipeline;
    delete terrainPipeline;
    delete deferredMrtPipeline;
    delete deferredResolvePipeline;
    delete temporalAntialiasingPipeline;
    delete postProcessPipeline;

    delete mainTerrainChunk;

    resourceManager->destroyImage(heightMap);

    for (AllocatedBuffer sceneBuffer : sceneDataBuffers) {
        resourceManager->destroyBuffer(sceneBuffer);
    }
    resourceManager->destroyBuffer(debugSceneDataBuffer);
    sceneDataDescriptorBuffer.destroy(context->allocator);

    delete imguiWrapper;

    for (const FrameData& frame : frames) {
        vkDestroyCommandPool(context->device, frame._commandPool, nullptr);

        //destroy sync objects
        vkDestroyFence(context->device, frame._renderFence, nullptr);
        vkDestroySemaphore(context->device, frame._renderSemaphore, nullptr);
        vkDestroySemaphore(context->device, frame._swapchainSemaphore, nullptr);
    }

    resourceManager->destroyImage(drawImage);
    resourceManager->destroyImage(depthImage);
    resourceManager->destroyImage(normalRenderTarget);
    resourceManager->destroyImage(albedoRenderTarget);
    resourceManager->destroyImage(pbrRenderTarget);
    resourceManager->destroyImage(velocityRenderTarget);
    resourceManager->destroyImage(taaResolveTarget);
    resourceManager->destroyImage(historyBuffer);
    resourceManager->destroyImage(postProcessOutputBuffer);

    delete scene;

    for (const std::pair<uint32_t, RenderObject*> renderObject : renderObjectMap) {
        delete renderObject.second;
    }
    renderObjectMap.clear();

    delete cascadedShadowMap;
    delete environmentMap;
    delete physics::Physics::get();
    delete immediate;
    delete resourceManager;
    delete identifierManager;

    vkDestroySwapchainKHR(context->device, swapchain, nullptr);
    for (const VkImageView swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImageView, nullptr);
    }

    delete context;

    SDL_DestroyWindow(window);
}

IHierarchical* Engine::createGameObject(const std::string& name) const
{
    const auto newGameObject = new GameObject(name);
    scene->addGameObject(newGameObject);
    return newGameObject;
}

void Engine::addToBeginQueue(IHierarchical* obj)
{
    hierarchalBeginQueue.push_back(obj);
}

void Engine::addToDeletionQueue(IHierarchical* obj)
{
    const auto found = std::ranges::find(hierarchalBeginQueue, obj);
    if (found != hierarchalBeginQueue.end()) {
        hierarchalBeginQueue.erase(found);
    }
    hierarchicalDeletionQueue.push_back(obj);
}

RenderObject* Engine::getRenderObject(const uint32_t renderRefIndex)
{
    const bool isLoaded = renderObjectMap.contains(renderRefIndex) && renderObjectMap[renderRefIndex] != nullptr;
    const auto renderObjectProperties = renderObjectInfoMap.find(renderRefIndex);
    if (!isLoaded) {
        renderObjectMap[renderRefIndex] = new RenderObject(renderObjectProperties->second.gltfPath, *resourceManager, renderRefIndex);
    }

    return renderObjectMap[renderRefIndex];
}

void Engine::createSwapchain(const uint32_t width, const uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{context->physicalDevice, context->device, context->surface};

    swapchainImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(PRESENT_MODE)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    swapchainExtent = vkbSwapchain.extent;

    // Swapchain and SwapchainImages
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void Engine::resizeSwapchain()
{
    vkDeviceWaitIdle(context->device);

    vkDestroySwapchainKHR(context->device, swapchain, nullptr);
    for (const auto swapchainImage : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImage, nullptr);
    }

    int w, h;
    // get new window size
    SDL_GetWindowSize(window, &w, &h);
    windowExtent.width = w;
    windowExtent.height = h;

    createSwapchain(windowExtent.width, windowExtent.height);

    bResizeRequested = false;
    fmt::print("Window extent has been updated to {}x{}\n", windowExtent.width, windowExtent.height);
}

void Engine::createDrawResources()
{
    // Draw Image
    {
        drawImage.imageFormat = DRAW_FORMAT;
        constexpr VkExtent3D drawImageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
        drawImage.imageExtent = drawImageExtent;
        VkImageUsageFlags drawImageUsages{};
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo renderImageInfo = vk_helpers::imageCreateInfo(drawImage.imageFormat, drawImageUsages, drawImageExtent);

        VmaAllocationCreateInfo renderImageAllocationInfo = {};
        renderImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        renderImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &renderImageInfo, &renderImageAllocationInfo, &drawImage.image, &drawImage.allocation, nullptr);

        VkImageViewCreateInfo renderViewInfo = vk_helpers::imageviewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &renderViewInfo, nullptr, &drawImage.imageView));
    }
    // Depth Image
    {
        depthImage.imageFormat = DEPTH_FORMAT;
        constexpr VkExtent3D depthImageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
        depthImage.imageExtent = depthImageExtent;
        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo depthImageInfo = vk_helpers::imageCreateInfo(depthImage.imageFormat, depthImageUsages, depthImageExtent);

        VmaAllocationCreateInfo depthImageAllocationInfo = {};
        depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        depthImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &depthImageInfo, &depthImageAllocationInfo, &depthImage.image, &depthImage.allocation, nullptr);

        VkImageViewCreateInfo depthViewInfo = vk_helpers::imageviewCreateInfo(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(context->device, &depthViewInfo, nullptr, &depthImage.imageView));
    }
    // Render Targets
    {
        const auto generateRenderTarget = std::function([this](const VkFormat renderTargetFormat) {
            constexpr VkExtent3D imageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
            VkImageUsageFlags usageFlags{};
            usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            VmaAllocationCreateInfo renderImageAllocationInfo = {};
            renderImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            renderImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            AllocatedImage renderTarget{};
            renderTarget.imageFormat = renderTargetFormat;
            renderTarget.imageExtent = imageExtent;
            const VkImageCreateInfo imageInfo = vk_helpers::imageCreateInfo(renderTarget.imageFormat, usageFlags, imageExtent);
            vmaCreateImage(context->allocator, &imageInfo, &renderImageAllocationInfo, &renderTarget.image, &renderTarget.allocation,
                           nullptr);

            const VkImageViewCreateInfo imageViewInfo = vk_helpers::imageviewCreateInfo(renderTarget.imageFormat, renderTarget.image,
                                                                                        VK_IMAGE_ASPECT_COLOR_BIT);
            VK_CHECK(vkCreateImageView(context->device, &imageViewInfo, nullptr, &renderTarget.imageView));

            return renderTarget;
        });

        normalRenderTarget = generateRenderTarget(NORMAL_FORMAT);
        albedoRenderTarget = generateRenderTarget(ALBEDO_FORMAT);
        pbrRenderTarget = generateRenderTarget(PBR_FORMAT);
        velocityRenderTarget = generateRenderTarget(VELOCITY_FORMAT);
    }
    // TAA Resolve
    {
        taaResolveTarget.imageFormat = DRAW_FORMAT;
        constexpr VkExtent3D imageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
        taaResolveTarget.imageExtent = imageExtent;
        VkImageUsageFlags taaResolveUsages{};
        taaResolveUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

        const VkImageCreateInfo taaResolveImageInfo = vk_helpers::imageCreateInfo(taaResolveTarget.imageFormat, taaResolveUsages, imageExtent);

        VmaAllocationCreateInfo taaResolveAllocationInfo = {};
        taaResolveAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        taaResolveAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &taaResolveImageInfo, &taaResolveAllocationInfo, &taaResolveTarget.image, &taaResolveTarget.allocation, nullptr);

        const VkImageViewCreateInfo taaResolveImageViewInfo = vk_helpers::imageviewCreateInfo(taaResolveTarget.imageFormat, taaResolveTarget.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &taaResolveImageViewInfo, nullptr, &taaResolveTarget.imageView));
    }
    // Draw History
    {
        historyBuffer.imageFormat = DRAW_FORMAT;
        constexpr VkExtent3D imageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
        historyBuffer.imageExtent = imageExtent;
        VkImageUsageFlags historyBufferUsages{};
        historyBufferUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        historyBufferUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkImageCreateInfo historyBufferImageInfo = vk_helpers::imageCreateInfo(historyBuffer.imageFormat, historyBufferUsages, imageExtent);

        VmaAllocationCreateInfo historyBufferAllocationInfo = {};
        historyBufferAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        historyBufferAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &historyBufferImageInfo, &historyBufferAllocationInfo, &historyBuffer.image, &historyBuffer.allocation, nullptr);

        const VkImageViewCreateInfo historyBufferImageViewInfo = vk_helpers::imageviewCreateInfo(historyBuffer.imageFormat, historyBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &historyBufferImageViewInfo, nullptr, &historyBuffer.imageView));
    }
    // Post Process Resolve
    {
        postProcessOutputBuffer.imageFormat = DRAW_FORMAT;
        constexpr VkExtent3D imageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
        postProcessOutputBuffer.imageExtent = imageExtent;
        VkImageUsageFlags usageFlags{};
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        const VkImageCreateInfo imageCreateInfo = vk_helpers::imageCreateInfo(postProcessOutputBuffer.imageFormat, usageFlags, imageExtent);

        VmaAllocationCreateInfo imageAllocationInfo = {};
        imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &imageCreateInfo, &imageAllocationInfo, &postProcessOutputBuffer.image, &postProcessOutputBuffer.allocation, nullptr);

        VkImageViewCreateInfo rview_info = vk_helpers::imageviewCreateInfo(postProcessOutputBuffer.imageFormat, postProcessOutputBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &rview_info, nullptr, &postProcessOutputBuffer.imageView));
    }
}

void Engine::hotReloadShaders() const
{
    vkDeviceWaitIdle(context->device);
    visibilityPassPipeline->reloadShaders();
    environmentPipeline->reloadShaders();
    terrainPipeline->reloadShaders();
    deferredMrtPipeline->reloadShaders();
    deferredResolvePipeline->reloadShaders();
    temporalAntialiasingPipeline->reloadShaders();
    postProcessPipeline->reloadShaders();
}
}
