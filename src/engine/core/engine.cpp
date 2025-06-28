//
// Created by William on 8/11/2024.
//

#include "engine.h"

#include <thread>

#include <vk-bootstrap/VkBootstrap.h>

#include <Jolt/Jolt.h>

#include "camera/free_camera.h"
#include "game_object/game_object.h"
#include "scene/serializer.h"
#include "engine/engine_constants.h"
#include "engine/core/input.h"
#include "engine/core/time.h"
#include "engine/physics/physics.h"
#include "engine/physics/physics_utils.h"
#include "engine/renderer/immediate_submitter.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/assets/render_object/render_object.h"
#include "engine/renderer/environment/environment.h"
#include "engine/core/game_object/game_object_factory.h"
#include "engine/renderer/render_context.h"
#include "engine/renderer/pipelines/shadows/cascaded_shadow_map/cascaded_shadow_map.h"
#include "engine/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline.h"
#include "engine/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline.h"
#include "engine/renderer/pipelines/geometry/environment/environment_pipeline.h"
#include "engine/renderer/pipelines/geometry/terrain/terrain_pipeline.h"
#include "engine/renderer/pipelines/post/post_process/post_process_pipeline.h"
#include "engine/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline.h"
#include "engine/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline_types.h"
#include "engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ground_truth_ambient_occlusion_pipeline.h"
#include "engine/renderer/pipelines/visibility_pass/visibility_pass_pipeline.h"
#include "engine/renderer/resources/image.h"
#include "engine/renderer/resources/render_target.h"
#include "engine/util/file.h"
#include "engine/util/halton.h"
#if  WILL_ENGINE_DEBUG_DRAW
#include "engine/renderer/pipelines/debug/debug_composite_pipeline.h"
#include "engine/renderer/pipelines/debug/debug_highlighter.h"
#include "engine/renderer/pipelines/debug/debug_renderer.h"
#endif // !WILL_ENGINE_DEBUG

#ifdef WILL_ENGINE_DEBUG
#define USE_VALIDATION_LAYERS true
#else
#define USE_VALIDATION_LAYERS false
#endif

#ifdef  WILL_ENGINE_DEBUG
// vsync
#define PRESENT_MODE VK_PRESENT_MODE_FIFO_KHR
#else
// uncapped FPS
#define PRESENT_MODE VK_PRESENT_MODE_IMMEDIATE_KHR
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
    startupProfiler.addEntry("Start");
    const auto start = std::chrono::system_clock::now();


    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);
    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    renderContext = new renderer::RenderContext(DEFAULT_RENDER_EXTENT_2D, 1.0f);

    window = SDL_CreateWindow(
        ENGINE_NAME,
        static_cast<int32_t>(renderContext->windowExtent.width),
        static_cast<int32_t>(renderContext->windowExtent.height),
        window_flags);
    input::Input::get().init(window, renderContext->windowExtent.width, renderContext->windowExtent.height);
    startupProfiler.addEntry("Windowing");


    context = new renderer::VulkanContext(window, USE_VALIDATION_LAYERS);
    startupProfiler.addEntry("Vulkan Context");

    createSwapchain(renderContext->windowExtent.width, renderContext->windowExtent.height);
    startupProfiler.addEntry("Swapchain");

    // Command Pools
    const VkCommandPoolCreateInfo commandPoolInfo = renderer::vk_helpers::commandPoolCreateInfo(context->graphicsQueueFamily,
                                                                                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    for (auto& frame : frames) {
        VK_CHECK(vkCreateCommandPool(context->device, &commandPoolInfo, nullptr, &frame._commandPool));
        VkCommandBufferAllocateInfo cmdAllocInfo = renderer::vk_helpers::commandBufferAllocateInfo(frame._commandPool);
        VK_CHECK(vkAllocateCommandBuffers(context->device, &cmdAllocInfo, &frame._mainCommandBuffer));
    }

    // Sync Structures
    const VkFenceCreateInfo fenceCreateInfo = renderer::vk_helpers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = renderer::vk_helpers::semaphoreCreateInfo();
    for (auto& frame : frames) {
        VK_CHECK(vkCreateFence(context->device, &fenceCreateInfo, nullptr, &frame._renderFence));

        VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &frame._renderSemaphore));
    }

    startupProfiler.addEntry("Command Pool and Sync Structures");

    immediate = new renderer::ImmediateSubmitter(*context);
    resourceManager = new renderer::ResourceManager(*context, *immediate);
    assetManager = new renderer::AssetManager(*resourceManager);
    physics = new physics::Physics();
    physics::Physics::set(physics);

    startupProfiler.addEntry("Immediate, ResourceM, AssetM, Physics");

    startupProfiler.addEntry("Draw Resources");

#if WILL_ENGINE_DEBUG_DRAW
    debugRenderer = new renderer::DebugRenderer(*resourceManager);
    renderer::DebugRenderer::set(debugRenderer);
    debugPipeline = new renderer::DebugCompositePipeline(*resourceManager);
    debugHighlighter = new renderer::DebugHighlighter(*resourceManager);
#endif
    startupProfiler.addEntry("Debug Draws");

    environmentMap = new renderer::Environment(*resourceManager, *immediate);
    startupProfiler.addEntry("Environment");

    auto& componentFactory = game::ComponentFactory::getInstance();
    componentFactory.registerComponents();

    auto& gameObjectFactory = game::GameObjectFactory::getInstance();
    gameObjectFactory.registerGameObjects();

    const std::filesystem::path envMapSource = "assets/environments";

    environmentMap->loadEnvironment("Overcast Sky", (envMapSource / "kloofendal_overcast_puresky_4k.hdr").string().c_str(), 2);
    // environmentMap->loadEnvironment("Wasteland", (envMapSource / "wasteland_clouds_puresky_4k.hdr").string().c_str(), 1);
    // environmentMap->loadEnvironment("Belfast Sunset", (envMapSource / "belfast_sunset_puresky_4k.hdr").string().c_str(), 0);
    // environmentMap->loadEnvironment("Rogland Clear Night", (envMapSource / "rogland_clear_night_4k.hdr").string().c_str(), 4);
    startupProfiler.addEntry("Load Environments");


    cascadedShadowMap = new renderer::CascadedShadowMap(*resourceManager);
    cascadedShadowMap->setCascadedShadowMapProperties(csmSettings);
    cascadedShadowMap->generateSplits();

    startupProfiler.addEntry("CSM");

    if (engine_constants::useImgui) {
        imguiWrapper = new ImguiWrapper(*context, {window, swapchainImageFormat});
    }

    initRenderer();
    initGame();

    Serializer::deserializeEngineSettings(this, EngineSettingsTypeFlag::ALL_SETTINGS);
    if (!generateDefaultMap()) {
        createMap(file::getSampleScene());
    }


    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    fmt::print("Finished Initialization in {} seconds\n", static_cast<float>(elapsed.count()) / 1000000.0f);

    profiler.addTimer("0Physics");
    profiler.addTimer("1Game");
    profiler.addTimer("2Render");
    profiler.addTimer("3Total");

    resolutionChangedHandle = renderContext->resolutionChangedEvent.subscribe([this](const renderer::ResolutionChangedEvent& event) {
        this->handleResize(event);
    });
    postResolutionChangedHandle = renderContext->postResolutionChangedEvent.subscribe([this](const renderer::ResolutionChangedEvent& event) {
        this->setupDescriptorBuffers();
    });

    createDrawResources({renderContext->renderExtent.width, renderContext->renderExtent.height, 1});
    setupDescriptorBuffers();
}

void Engine::initRenderer()
{
    // +1 for the debug scene data buffer. Not multi-buffering the debug for simplicity, may result in some flickering
#if WILL_ENGINE_DEBUG
    int32_t sceneDataCount = FRAME_OVERLAP + 1;
#else
    int32_t sceneDataCount = FRAME_OVERLAP;
#endif

    sceneDataDescriptorBuffer = resourceManager->createResource<renderer::DescriptorBufferUniform>(
        resourceManager->getSceneDataLayout(), sceneDataCount);

    std::vector<DescriptorUniformData> sceneDataBufferData{1};
    for (int i{0}; i < FRAME_OVERLAP; i++) {
        sceneDataBuffers[i] = resourceManager->createResource<renderer::Buffer>(renderer::BufferType::HostSequential, sizeof(SceneData));
        sceneDataBufferData[0] = DescriptorUniformData{.buffer = sceneDataBuffers[i]->buffer, .allocSize = sizeof(SceneData)};
        sceneDataDescriptorBuffer->setupData(sceneDataBufferData, i);
    }

#if WILL_ENGINE_DEBUG
    debugSceneDataBuffer = resourceManager->createResource<renderer::Buffer>(renderer::BufferType::HostSequential, sizeof(SceneData));
    sceneDataBufferData[0] = DescriptorUniformData{.buffer = debugSceneDataBuffer->buffer, .allocSize = sizeof(SceneData)};
    sceneDataDescriptorBuffer->setupData(sceneDataBufferData, FRAME_OVERLAP);
#endif

    visibilityPassPipeline = new renderer::VisibilityPassPipeline(*resourceManager);
    startupProfiler.addEntry("Init Visibility Pass");
    environmentPipeline = new renderer::EnvironmentPipeline(*resourceManager, environmentMap->getCubemapDescriptorSetLayout());
    startupProfiler.addEntry("Init Environment Pass");
    terrainPipeline = new renderer::TerrainPipeline(*resourceManager);
    startupProfiler.addEntry("Init Terrain Pass");
    deferredMrtPipeline = new renderer::DeferredMrtPipeline(*resourceManager);
    startupProfiler.addEntry("Init Deffered MRT Pass");
    ambientOcclusionPipeline = new renderer::GroundTruthAmbientOcclusionPipeline(*resourceManager, *renderContext);
    startupProfiler.addEntry("Init GTAO Pass");
    contactShadowsPipeline = new renderer::ContactShadowsPipeline(*resourceManager, *renderContext);
    startupProfiler.addEntry("Init SSS Pass");
    deferredResolvePipeline = new renderer::DeferredResolvePipeline(*resourceManager, environmentMap->getDiffSpecMapDescriptorSetLayout(),
                                                                    cascadedShadowMap->getCascadedShadowMapUniformLayout(),
                                                                    cascadedShadowMap->getCascadedShadowMapSamplerLayout());
    startupProfiler.addEntry("Init Deferred Resolve Pass");
    temporalAntialiasingPipeline = new renderer::TemporalAntialiasingPipeline(*resourceManager);
    startupProfiler.addEntry("Init TAA Pass");
    transparentPipeline = new renderer::TransparentPipeline(*resourceManager, *renderContext,
                                                            environmentMap->getDiffSpecMapDescriptorSetLayout(),
                                                            cascadedShadowMap->getCascadedShadowMapUniformLayout(),
                                                            cascadedShadowMap->getCascadedShadowMapSamplerLayout());
    startupProfiler.addEntry("Init Transparent Pass");
    postProcessPipeline = new renderer::PostProcessPipeline(*resourceManager);
    startupProfiler.addEntry("Init PP Pass");
}

void Engine::initGame()
{
    assetManager->scanForAll();
    fallbackCamera = new FreeCamera();

    const auto testHandle = testDispatcher.subscribe([this](uint32_t test) {
        fmt::print("Test dispatcher successful {}\n", test);
    });

    testDispatcher.dispatch(10);

    testDispatcher.unsubscribe(testHandle);
}

void Engine::run()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Running {}\n", ENGINE_NAME);

    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        input::Input& input = input::Input::get();
        Time& time = Time::Get();
        input.frameReset();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { bQuit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { bQuit = true; }


            if (e.type == SDL_EVENT_WINDOW_MINIMIZED) { bStopRendering = true; }
            if (e.type == SDL_EVENT_WINDOW_RESTORED) { bStopRendering = false; }
            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bWindowChanged = true;
            }

            if (engine_constants::useImgui) {
                ImguiWrapper::handleInput(e);
            }

            input.processEvent(e);
        }

        if (bWindowChanged) {
            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);
            renderContext->requestWindowResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            bWindowChanged = false;
        }


        if (renderContext->hasPendingChanges()) {
            vkDeviceWaitIdle(context->device);

            const bool res = renderContext->applyPendingChanges();
            assert(res);

            vkDestroySwapchainKHR(context->device, swapchain, nullptr);
            for (const auto swapchainImage : swapchainImageViews) {
                vkDestroyImageView(context->device, swapchainImage, nullptr);
            }

            createSwapchain(renderContext->windowExtent.width, renderContext->windowExtent.height);
            input::Input::get().updateWindowExtent(renderContext->windowExtent.width, renderContext->windowExtent.height);
        }

        input.updateFocus(SDL_GetWindowFlags(window));
        time.update();

        if (engine_constants::useImgui) {
            imguiWrapper->imguiInterface(this);
        }

        const float deltaTime = Time::Get().getDeltaTime();
        profiler.beginTimer("3Total");

        profiler.beginTimer("0Physics");
        updatePhysics(deltaTime);
        profiler.endTimer("0Physics");

        profiler.beginTimer("1Game");
        updateGame(deltaTime);
        profiler.endTimer("1Game");

        updateDebug(deltaTime);

        if (bStopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else {
            render(deltaTime);
        }

#if WILL_ENGINE_DEBUG_DRAW
        debugRenderer->clear();
#endif

        profiler.endTimer("3Total");
    }
}

void Engine::updatePhysics(const float deltaTime) const
{
    if (bEnablePhysics) {
        physics::Physics::get()->update(deltaTime);
    }

#if WILL_ENGINE_DEBUG_DRAW
    if (bDebugPhysics) {
        physics::Physics::get()->drawDebug();
    }
#endif
}

void Engine::updateGame(const float deltaTime)
{
    if (fallbackCamera) { fallbackCamera->update(deltaTime); }

    const input::Input& input = input::Input::get();
#if WILL_ENGINE_DEBUG
    // Non-Core Gameplay Actions
    if (input.isKeyPressed(input::Key::R)) {
        if (fallbackCamera) {
            const glm::vec3 direction = fallbackCamera->getForwardWS();
            const physics::RaycastHit result = physics::PhysicsUtils::raycast(fallbackCamera->getPosition(), direction, 100.0f, {}, {}, {});

            if (result.hasHit) {
                physics::PhysicsUtils::addImpulseAtPosition(result.hitBodyID, normalize(direction) * 100.0f, result.hitPosition);
            }
            else {
                fmt::print("Failed to find an object with the raycast\n");
            }
        }
    }

    if (input.isKeyPressed(input::Key::G)) {
        if (fallbackCamera) {
            if (auto transformable = dynamic_cast<ITransformable*>(selectedItem)) {
                glm::vec3 itemPosition = transformable->getPosition();

                glm::vec3 itemScale = transformable->getScale();
                float maxDimension = glm::max(glm::max(itemScale.x, itemScale.y), itemScale.z);

                float distance = glm::max(5.0f, maxDimension * 3.0f);

                glm::vec3 currentCamPos = fallbackCamera->getTransform().getPosition();
                glm::vec3 currentDirection = glm::normalize(itemPosition - currentCamPos);

                glm::vec3 newCamPos = itemPosition - (currentDirection * distance);

                glm::vec3 forward = glm::normalize(newCamPos - itemPosition);
                glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
                glm::vec3 up = glm::cross(forward, right);

                glm::mat3 rotMatrix(right, up, forward);
                glm::quat newRotation = glm::quat_cast(rotMatrix);

                fallbackCamera->setCameraTransform(newCamPos, newRotation);
            }
        }
    }
#endif

    for (IHierarchical* hierarchal : hierarchalBeginQueue) {
        hierarchal->beginPlay();
    }
    hierarchalBeginQueue.clear();

    // This is the main game update loop.
    // Recursively updates all child gameobjects of each map
    for (auto& map : activeMaps) {
        map->update(deltaTime);
    }

    for (const auto& map : mapDeletionQueue) {
        map->beginDestructor();
    }
    mapDeletionQueue.clear();

    for (const auto& hierarchical : hierarchicalDeletionQueue) {
        hierarchical->beginDestructor();
    }
    hierarchicalDeletionQueue.clear();
}

void Engine::updateRender(VkCommandBuffer cmd, const float deltaTime, const int32_t currentFrameOverlap, const int32_t previousFrameOverlap)
{
    const renderer::Buffer* previousSceneDataBuffer = sceneDataBuffers[previousFrameOverlap].get();
    const renderer::Buffer* sceneDataBuffer = sceneDataBuffers[currentFrameOverlap].get();

    const auto pPreviousSceneData = static_cast<SceneData*>(previousSceneDataBuffer->info.pMappedData);
    assert(pPreviousSceneData != nullptr && "Previous scene data buffer not mapped");
    const auto pSceneData = static_cast<SceneData*>(sceneDataBuffer->info.pMappedData);
    assert(pSceneData != nullptr && "Scene data buffer not mapped");


    const bool bIsFrameZero = frameNumber == 0;
    glm::vec2 prevJitter = HaltonSequence::getJitterHardcoded(bIsFrameZero ? frameNumber : frameNumber - 1) - 0.5f;
    prevJitter.x /= renderContext->renderExtent.width;
    prevJitter.y /= renderContext->renderExtent.height;
    glm::vec2 currentJitter = HaltonSequence::getJitterHardcoded(frameNumber) - 0.5f;
    currentJitter.x /= renderContext->renderExtent.width;
    currentJitter.y /= renderContext->renderExtent.height;

    pSceneData->jitter = taaSettings.bEnabled ? glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y) : glm::vec4(0.0f);

    const glm::mat4 cameraLook = glm::lookAt(glm::vec3(0), fallbackCamera->getForwardWS(), glm::vec3(0, 1, 0));

    pSceneData->prevView = bIsFrameZero ? fallbackCamera->getViewMatrix() : pPreviousSceneData->view;
    pSceneData->prevProj = bIsFrameZero ? fallbackCamera->getProjMatrix() : pPreviousSceneData->proj;
    pSceneData->prevViewProj = bIsFrameZero ? fallbackCamera->getViewProjMatrix() : pPreviousSceneData->viewProj;

    pSceneData->prevInvView = bIsFrameZero ? inverse(fallbackCamera->getViewMatrix()) : pPreviousSceneData->invView;
    pSceneData->prevInvProj = bIsFrameZero ? inverse(fallbackCamera->getProjMatrix()) : pPreviousSceneData->invProj;
    pSceneData->prevInvViewProj = bIsFrameZero ? inverse(fallbackCamera->getViewProjMatrix()) : pPreviousSceneData->invViewProj;

    pSceneData->prevViewProjCameraLookDirection = bIsFrameZero ? pSceneData->proj * cameraLook : pPreviousSceneData->viewProjCameraLookDirection;

    pSceneData->view = fallbackCamera->getViewMatrix();
    pSceneData->proj = fallbackCamera->getProjMatrix();
    pSceneData->viewProj = pSceneData->proj * pSceneData->view;

    pSceneData->invView = glm::inverse(pSceneData->view);
    pSceneData->invProj = glm::inverse(pSceneData->proj);
    pSceneData->invViewProj = glm::inverse(pSceneData->viewProj);

    pSceneData->viewProjCameraLookDirection = pSceneData->proj * cameraLook;

    pSceneData->prevCameraWorldPos = bIsFrameZero ? fallbackCamera->getPosition() : pPreviousSceneData->cameraWorldPos;
    pSceneData->cameraWorldPos = fallbackCamera->getPosition();


    pSceneData->renderTargetSize = {renderContext->renderExtent.width, renderContext->renderExtent.height};
    pSceneData->texelSize = 1.0f / pSceneData->renderTargetSize;
    pSceneData->cameraPlanes = {fallbackCamera->getNearPlane(), fallbackCamera->getFarPlane()};
    pSceneData->deltaTime = deltaTime;


#if WILL_ENGINE_DEBUG
    if (!bFreezeVisibilitySceneData) {
        const auto pDebugSceneData = static_cast<SceneData*>(debugSceneDataBuffer->info.pMappedData);
        *pDebugSceneData = *pSceneData;
        renderer::vk_helpers::bufferBarrier(cmd, debugSceneDataBuffer->buffer, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
                                        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
    }
#endif

    renderer::vk_helpers::bufferBarrier(cmd, sceneDataBuffer->buffer, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
                                        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
}

void Engine::updateDebug(float deltaTime)
{
#if WILL_ENGINE_DEBUG_DRAW
    if (selectedItem) {
        if (const auto gameObject = dynamic_cast<game::GameObject*>(selectedItem)) {
            if (const auto rb = gameObject->getComponent<game::RigidBodyComponent>()) {
                if (rb->hasRigidBody()) {
                    physics::Physics::get()->drawDebug(rb->getPhysicsBodyId());
                }
            }
        }
    }

    const input::Input& input = input::Input::get();
    if (!input.isInFocus()) {
        if (input.isMousePressed(input::MouseButton::LMB)) {
            if (physics::Physics* physics = physics::Physics::get()) {
                const glm::vec3 direction = fallbackCamera->screenToWorldDirection(input.getMousePosition());
                if (const physics::RaycastHit result = physics::PhysicsUtils::raycast(fallbackCamera->getPosition(),
                                                                                      normalize(direction) * fallbackCamera->getNearPlane())) {
                    if (const physics::PhysicsObject* physicsObject = physics->getPhysicsObject(result.hitBodyID)) {
                        if (const auto* component = dynamic_cast<game::Component*>(physicsObject->physicsBody)) {
                            if (IComponentContainer* owner = component->getOwner()) {
                                if (const auto hierarchical = dynamic_cast<IHierarchical*>(owner)) {
                                    selectItem(hierarchical);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
#endif

#if WILL_ENGINE_DEBUG_DRAW
    constexpr glm::vec3 offset{-100, 100, 0};
    for (int32_t i{0}; i < 100; i++) {
        for (int32_t j{0}; j < 100; j++) {
            float z = glm::sin(Time::Get().getTime() - j * 0.5f) * 2.0;
            renderer::DebugRenderer::drawBox(offset + glm::vec3(i, j, z), {0.8f, 0.8f, 0.8f}, {0.0f, 0.7f, 0.1f});
        }
    }
#endif
}

void Engine::render(float deltaTime)
{
    // GPU -> VPU sync (fence)
    VK_CHECK(vkWaitForFences(context->device, 1, &getCurrentFrame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(context->device, 1, &getCurrentFrame()._renderFence));

    // GPU -> GPU sync (semaphore)
    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(context->device, swapchain, 1000000000, getCurrentFrame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bWindowChanged = true;
        fmt::print("Swapchain out of date or suboptimal (Acquire)\n");
        return;
    }

    int32_t currentFrameOverlap = getCurrentFrameOverlap();
    int32_t previousFrameOverlap = getPreviousFrameOverlap();

    // Destroy all resources queued to be destroyed on this frame (from FRAME_OVERLAP frames ago)
    resourceManager->update(currentFrameOverlap);

    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = renderer::vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    // only submit once
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    profiler.beginTimer("2Render");

    const std::vector<renderer::RenderObject*>& allRenderObjects = assetManager->getAllRenderObjects();

    // Update Render Object Buffers and Model Matrices
    for (renderer::RenderObject* renderObject : allRenderObjects) {
        renderObject->update(cmd, currentFrameOverlap, previousFrameOverlap);
    }

    for (ITerrain* terrain : activeTerrains) {
        if (auto chunk = terrain->getTerrainChunk()) {
            chunk->update(currentFrameOverlap, previousFrameOverlap);
        }
    }

    // Updates Scene Data buffer
    updateRender(cmd, deltaTime, currentFrameOverlap, previousFrameOverlap);

    VkDescriptorBufferBindingInfoEXT sceneDataBinding = sceneDataDescriptorBuffer->getBindingInfo();
    VkDeviceSize sceneDataBufferOffset = sceneDataDescriptorBuffer->getDescriptorBufferSize() * currentFrameOverlap;

    // Updates Cascaded Shadow Map Properties
    cascadedShadowMap->update(mainLight, fallbackCamera, currentFrameOverlap);

    // Clear Color
    {
        constexpr VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
#if WILL_ENGINE_DEBUG_DRAW
        constexpr uint32_t COLOR_IMAGE_COUNT = 8;
#else
        constexpr uint32_t COLOR_IMAGE_COUNT = 7;
#endif
        // History buffer is not reset
        std::array colorImages = {
            drawImage.get(),
            normalRenderTarget.get(),
            albedoRenderTarget.get(),
            pbrRenderTarget.get(),
            velocityRenderTarget.get(),
            taaResolveTarget.get(),
            finalImageBuffer.get(),
#if WILL_ENGINE_DEBUG_DRAW
            debugTarget.get(),
#endif
        };

        std::array<VkImageMemoryBarrier2, COLOR_IMAGE_COUNT + 1> barriers{};

        for (uint32_t i = 0; i < COLOR_IMAGE_COUNT; ++i) {
            VkImageMemoryBarrier2& barrier = barriers[i];
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.pNext = nullptr;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            barrier.image = colorImages[i]->image;
            colorImages[i]->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        VkImageMemoryBarrier2& depthBarrier = barriers[COLOR_IMAGE_COUNT];
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        depthBarrier.pNext = nullptr;
        depthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        depthBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        depthBarrier.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        depthBarrier.image = depthStencilImage->image;
        depthStencilImage->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.imageMemoryBarrierCount = COLOR_IMAGE_COUNT + 1;
        depInfo.pImageMemoryBarriers = barriers.data();
        vkCmdPipelineBarrier2(cmd, &depInfo);

        VkImageSubresourceRange colorRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        for (renderer::RenderTarget* rt : colorImages) {
            vkCmdClearColorImage(cmd, rt->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &colorRange);
        }

        VkClearDepthStencilValue depthClear = {0.0f, 0};
        VkImageSubresourceRange depthRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        vkCmdClearDepthStencilImage(cmd, depthStencilImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthClear, 1, &depthRange);

        for (uint32_t i = 0; i < COLOR_IMAGE_COUNT; ++i) {
            VkImageMemoryBarrier2& barrier = barriers[i];
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = colorImages[i]->afterClearFormat;
            colorImages[i]->imageLayout = colorImages[i]->afterClearFormat;
        }

        VkImageMemoryBarrier2& finalDepthBarrier = barriers[COLOR_IMAGE_COUNT];
        finalDepthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        finalDepthBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        finalDepthBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        finalDepthBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        finalDepthBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        finalDepthBarrier.newLayout = depthStencilImage->afterClearFormat;
        depthStencilImage->imageLayout = depthStencilImage->afterClearFormat;

        VkDependencyInfo finalDep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        finalDep.imageMemoryBarrierCount = COLOR_IMAGE_COUNT + 1;
        finalDep.pImageMemoryBarriers = barriers.data();
        vkCmdPipelineBarrier2(cmd, &finalDep);
    }

    renderer::VisibilityPassDrawInfo deferredFrustumCullDrawInfo{
        currentFrameOverlap,
        allRenderObjects,
        sceneDataBinding,
        sceneDataBufferOffset,
        true,
    };
#if WILL_ENGINE_DEBUG
    if (bFreezeVisibilitySceneData) {
        deferredFrustumCullDrawInfo.sceneDataOffset = sceneDataDescriptorBuffer->getDescriptorBufferSize() * FRAME_OVERLAP;
    }
#endif

    visibilityPassPipeline->draw(cmd, deferredFrustumCullDrawInfo);

    renderer::CascadedShadowMapDrawInfo csmDrawInfo{
        csmSettings.bEnabled,
        currentFrameOverlap,
        allRenderObjects,
        activeTerrains,
    };

    cascadedShadowMap->draw(cmd, csmDrawInfo);

    renderer::EnvironmentDrawInfo environmentPipelineDrawInfo{
        renderContext->renderExtent,
        normalRenderTarget->imageView,
        albedoRenderTarget->imageView,
        pbrRenderTarget->imageView,
        velocityRenderTarget->imageView,
        depthImageView->imageView,
        sceneDataBinding,
        sceneDataBufferOffset,
        environmentMap->getCubemapDescriptorBuffer()->getBindingInfo(),
        environmentMap->getCubemapDescriptorBuffer()->getDescriptorBufferSize() * environmentMapIndex,
    };
    environmentPipeline->draw(cmd, environmentPipelineDrawInfo);

    renderer::TerrainDrawInfo terrainDrawInfo{
        false,
        currentFrameOverlap,
        renderContext->renderExtent,
        activeTerrains,
        normalRenderTarget->imageView,
        albedoRenderTarget->imageView,
        pbrRenderTarget->imageView,
        velocityRenderTarget->imageView,
        depthImageView->imageView,
        sceneDataBinding,
        sceneDataBufferOffset,
    };
    terrainPipeline->draw(cmd, terrainDrawInfo);

    const renderer::DeferredMrtDrawInfo deferredMrtDrawInfo{
        false,
        currentFrameOverlap,
        renderContext->renderExtent,
        allRenderObjects,
        normalRenderTarget->imageView,
        albedoRenderTarget->imageView,
        pbrRenderTarget->imageView,
        velocityRenderTarget->imageView,
        depthImageView->imageView,
        sceneDataBinding,
        sceneDataBufferOffset,
    };
    deferredMrtPipeline->draw(cmd, deferredMrtDrawInfo);

    renderer::TransparentAccumulateDrawInfo transparentDrawInfo{
        true,
        renderContext->renderExtent,
        depthImageView->imageView,
        currentFrameOverlap,
        allRenderObjects,
        sceneDataBinding,
        sceneDataBufferOffset,
        environmentMap->getDiffSpecMapDescriptorBuffer()->getBindingInfo(),
        environmentMap->getDiffSpecMapDescriptorBuffer()->getDescriptorBufferSize() * environmentMapIndex,
        cascadedShadowMap->getCascadedShadowMapUniformBuffer()->getBindingInfo(),
        cascadedShadowMap->getCascadedShadowMapUniformBuffer()->getDescriptorBufferSize() * currentFrameOverlap,
        cascadedShadowMap->getCascadedShadowMapSamplerBuffer()->getBindingInfo(),
    };
    if (bDrawTransparents) {
        transparentPipeline->drawAccumulate(cmd, transparentDrawInfo);
    }

    renderer::vk_helpers::imageBarrier(cmd, drawImage.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::imageBarrier(cmd, depthStencilImage.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    renderer::vk_helpers::imageBarrier(cmd, normalRenderTarget.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::imageBarrier(cmd, albedoRenderTarget.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::imageBarrier(cmd, pbrRenderTarget.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::imageBarrier(cmd, velocityRenderTarget.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);


    renderer::GTAODrawInfo gtaoDrawInfo{
        renderContext->renderExtent,
        fallbackCamera,
        gtaoSettings.bEnabled,
        gtaoSettings.pushConstants,
        frameNumber,
        sceneDataBinding,
        sceneDataBufferOffset,
    };
    ambientOcclusionPipeline->draw(cmd, gtaoDrawInfo);

    renderer::ContactShadowsDrawInfo contactDrawInfo{
        fallbackCamera,
        mainLight,
        sssSettings.bEnabled,
        sssSettings.pushConstants,
        sceneDataBinding,
        sceneDataBufferOffset,
    };

    contactShadowsPipeline->draw(cmd, contactDrawInfo);

    const renderer::DeferredResolveDrawInfo deferredResolveDrawInfo{
        deferredDebug,
        csmSettings.pcfLevel,
        renderContext->renderExtent,
        sceneDataBinding,
        sceneDataBufferOffset,
        environmentMap->getDiffSpecMapDescriptorBuffer()->getBindingInfo(),
        environmentMap->getDiffSpecMapDescriptorBuffer()->getDescriptorBufferSize() * environmentMapIndex,
        cascadedShadowMap->getCascadedShadowMapUniformBuffer()->getBindingInfo(),
        cascadedShadowMap->getCascadedShadowMapUniformBuffer()->getDescriptorBufferSize() * currentFrameOverlap,
        cascadedShadowMap->getCascadedShadowMapSamplerBuffer()->getBindingInfo(),
        fallbackCamera->getNearPlane(),
        fallbackCamera->getFarPlane(),
        bEnableShadows,
        bEnableContactShadows
    };
    deferredResolvePipeline->draw(cmd, deferredResolveDrawInfo);

    renderer::vk_helpers::imageBarrier(cmd, drawImage.get(), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::TransparentCompositeDrawInfo compositeDrawInfo{
        renderContext->renderExtent,
        drawImage->imageView
    };
    if (bDrawTransparents) {
        transparentPipeline->drawComposite(cmd, compositeDrawInfo);
    }

    renderer::vk_helpers::imageBarrier(cmd, drawImage.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::imageBarrier(cmd, historyBuffer.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    const renderer::TemporalAntialiasingDrawInfo taaDrawInfo{
        taaSettings.blendValue,
        taaSettings.bEnabled ? 0 : 1,
        renderContext->renderExtent,
        sceneDataDescriptorBuffer->getBindingInfo(),
        sceneDataDescriptorBuffer->getDescriptorBufferSize() * currentFrameOverlap
    };
    temporalAntialiasingPipeline->draw(cmd, taaDrawInfo);

    // Copy to TAA History
    renderer::vk_helpers::imageBarrier(cmd, taaResolveTarget.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::imageBarrier(cmd, historyBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::copyImageToImage(cmd, taaResolveTarget->image, historyBuffer->image, taaResolveTarget->imageExtent,
                                           historyBuffer->imageExtent);
    renderer::vk_helpers::imageBarrier(cmd, taaResolveTarget.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);


    const renderer::PostProcessDrawInfo postProcessDrawInfo{
        postProcessData,
        renderContext->renderExtent,
        sceneDataBinding,
        sceneDataBufferOffset,
    };

    postProcessPipeline->draw(cmd, postProcessDrawInfo);

#if WILL_ENGINE_DEBUG_DRAW
    // Ensure all real rendering happens before this step, as debug draws do write to the depth buffer.
    // This should ALWAYS be the final step before copying to swapchain
    if (bDrawDebugRendering) {
        renderer::vk_helpers::imageBarrier(cmd, depthStencilImage.get(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                           VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);


        renderer::DebugRendererDrawInfo debugRendererDrawInfo{
            currentFrameOverlap,
            renderContext->renderExtent,
            debugTarget->imageView,
            depthImageView->imageView,
            sceneDataBinding,
            sceneDataBufferOffset,
        };

        debugRenderer->draw(cmd, debugRendererDrawInfo);

        bool stencilDrawn = false;

        renderer::DebugHighlighterDrawInfo highlightDrawInfo{
            nullptr,
            renderContext->renderExtent,
            depthStencilImage->imageView,
            sceneDataBinding,
            sceneDataBufferOffset,
        };


        if (auto cc = dynamic_cast<IComponentContainer*>(selectedItem)) {
            if (auto meshRenderer = cc->getComponent<game::MeshRendererComponent>()) {
                highlightDrawInfo.highlightTarget = meshRenderer;
                stencilDrawn = debugHighlighter->drawHighlightStencil(cmd, highlightDrawInfo);
            }
        }

        if (stencilDrawn) {
            renderer::vk_helpers::imageBarrier(cmd, debugTarget.get(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
            renderer::vk_helpers::imageBarrier(cmd, depthStencilImage.get(), VK_IMAGE_LAYOUT_GENERAL,
                                               VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

            debugHighlighter->drawHighlightProcessing(cmd, highlightDrawInfo);


            renderer::vk_helpers::imageBarrier(cmd, debugTarget.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        }
        else {
            renderer::vk_helpers::imageBarrier(cmd, debugTarget.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        }


        renderer::DebugCompositePipelineDrawInfo drawInfo{
            renderContext->renderExtent,
            sceneDataBinding,
            sceneDataBufferOffset,
        };

        // Composite all draws in the debug image into `finalImageBuffer`
        debugPipeline->draw(cmd, drawInfo);
    }
#endif
    renderer::vk_helpers::imageBarrier(cmd, finalImageBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_ASPECT_COLOR_BIT);
    renderer::vk_helpers::copyImageToImage(cmd, finalImageBuffer->image, swapchainImages[swapchainImageIndex], finalImageBuffer->imageExtent,
                                           swapchainExtent);

    if (engine_constants::useImgui) {
        renderer::vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        imguiWrapper->drawImgui(cmd, swapchainImageViews[swapchainImageIndex], swapchainExtent);

        renderer::vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                           VK_IMAGE_ASPECT_COLOR_BIT);
    }
    else {
        renderer::vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // End Command Buffer Recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    profiler.endTimer("2Render");

    // Submission
    const VkCommandBufferSubmitInfo cmdSubmitInfo = renderer::vk_helpers::commandBufferSubmitInfo(cmd);
    const VkSemaphoreSubmitInfo waitInfo = renderer::vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                                     getCurrentFrame()._swapchainSemaphore);
    const VkSemaphoreSubmitInfo signalInfo =
            renderer::vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);
    const VkSubmitInfo2 submit = renderer::vk_helpers::submitInfo(&cmdSubmitInfo, &signalInfo, &waitInfo);

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
    renderContext->advanceFrame();

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bWindowChanged = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}

void Engine::cleanup()
{
    fmt::print("----------------------------------------\n");
    fmt::print("Cleaning up {}\n", ENGINE_NAME);

#if WILL_ENGINE_DEBUG
    if (editorSettings.bSaveSettingsOnExit) {
        Serializer::serializeEngineSettings(this, EngineSettingsTypeFlag::ALL_SETTINGS);

        fmt::print("Cleanup: Saved all engine settings\n");
    }

    if (editorSettings.bSaveMapOnExit) {
        fmt::print("Cleanup: Saving all active maps:\n");
        for (const std::unique_ptr<game::Map>& map : activeMaps) {
            map->saveMap();
            fmt::print("    Map Saved ({})\n", map.get()->getName());
        }
    }
#endif

    vkDeviceWaitIdle(context->device);

    delete visibilityPassPipeline;
    delete environmentPipeline;
    delete terrainPipeline;
    delete deferredMrtPipeline;
    delete ambientOcclusionPipeline;
    delete contactShadowsPipeline;
    delete deferredResolvePipeline;
    delete temporalAntialiasingPipeline;
    delete transparentPipeline;
    delete postProcessPipeline;

    for (renderer::BufferPtr& sceneBuffer : sceneDataBuffers) {
        resourceManager->destroyResource(std::move(sceneBuffer));
    }
    resourceManager->destroyResource(std::move(debugSceneDataBuffer));
    resourceManager->destroyResource(std::move(sceneDataDescriptorBuffer));

    if (engine_constants::useImgui) {
        delete imguiWrapper;
    }


    for (const FrameData& frame : frames) {
        vkDestroyCommandPool(context->device, frame._commandPool, nullptr);

        //destroy sync objects
        vkDestroyFence(context->device, frame._renderFence, nullptr);
        vkDestroySemaphore(context->device, frame._renderSemaphore, nullptr);
        vkDestroySemaphore(context->device, frame._swapchainSemaphore, nullptr);
    }

    resourceManager->destroyResource(std::move(drawImage));
    resourceManager->destroyResource(std::move(depthStencilImage));
    resourceManager->destroyResource(std::move(depthImageView));
    resourceManager->destroyResource(std::move(stencilImageView));
    resourceManager->destroyResource(std::move(normalRenderTarget));
    resourceManager->destroyResource(std::move(albedoRenderTarget));
    resourceManager->destroyResource(std::move(pbrRenderTarget));
    resourceManager->destroyResource(std::move(velocityRenderTarget));
    resourceManager->destroyResource(std::move(taaResolveTarget));
    resourceManager->destroyResource(std::move(historyBuffer));
    resourceManager->destroyResource(std::move(finalImageBuffer));

    for (const auto& map : activeMaps) {
        map->destroy();
    }
    // clear map deletion
    for (const auto& map : mapDeletionQueue) {
        map->beginDestructor();
    }
    mapDeletionQueue.clear();

    // clear hierarchy deletion
    for (const auto& hierarchical : hierarchicalDeletionQueue) {
        hierarchical->beginDestructor();
    }
    hierarchicalDeletionQueue.clear();
    hierarchalBeginQueue.clear();

    delete assetManager;

    delete cascadedShadowMap;
    delete environmentMap;
#if WILL_ENGINE_DEBUG_DRAW
    resourceManager->destroyResource(std::move(debugTarget));
    delete debugRenderer;
    delete debugHighlighter;
    delete debugPipeline;
#endif
    delete physics;
    delete immediate;
    delete resourceManager;

    vkDestroySwapchainKHR(context->device, swapchain, nullptr);
    for (const VkImageView swapchainImageView : swapchainImageViews) {
        vkDestroyImageView(context->device, swapchainImageView, nullptr);
    }

    delete context;

    SDL_DestroyWindow(window);
}

#if WILL_ENGINE_DEBUG_DRAW
void Engine::selectItem(IHierarchical* hierarchical)
{
    if (selectedItem != nullptr) {
        if (selectedItem == hierarchical) { return; }
        deselectItem();
    }
    selectedItem = hierarchical;
}

void Engine::deselectItem()
{
    if (const auto gameObject = dynamic_cast<game::GameObject*>(selectedItem)) {
        if (const game::RigidBodyComponent* rb = gameObject->getComponent<game::RigidBodyComponent>()) {
            if (rb->hasRigidBody()) {
                physics::Physics::get()->stopDrawDebug(rb->getPhysicsBodyId());
            }
        }
    }
    selectedItem = nullptr;
}
#endif

IHierarchical* Engine::createGameObject(game::Map* map, const std::string& name)
{
    auto newGameObject = std::make_unique<game::GameObject>(name);
    const auto newGameObjectPtr = map->addChild(std::move(newGameObject));
    return newGameObjectPtr;
}

void Engine::addToBeginQueue(IHierarchical* obj)
{
    hierarchalBeginQueue.push_back(obj);
}

void Engine::addToMapDeletionQueue(game::Map* obj)
{
    auto it = std::ranges::find_if(activeMaps,
                                   [obj](const std::unique_ptr<game::Map>& ptr) {
                                       return ptr.get() == obj;
                                   });

    if (it != activeMaps.end()) {
        mapDeletionQueue.push_back(std::move(*it));
        activeMaps.erase(it);
    }
}


void Engine::addToDeletionQueue(std::unique_ptr<IHierarchical> obj)
{
    hierarchicalDeletionQueue.push_back(std::move(obj));
}

void Engine::addToActiveTerrain(ITerrain* terrain)
{
    activeTerrains.insert(terrain);
}

void Engine::removeFromActiveTerrain(ITerrain* terrain)
{
    if (activeTerrains.contains(terrain)) {
        activeTerrains.erase(terrain);
    }
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

    swapchainExtent = {vkbSwapchain.extent.width, vkbSwapchain.extent.height, 1};

    // Swapchain and SwapchainImages
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}

game::Map* Engine::createMap(const std::filesystem::path& path)
{
    auto newMap = std::make_unique<game::Map>(path, *resourceManager);
    if (!newMap) { return nullptr; }
    game::Map* newMapPtr = newMap.get();
    activeMaps.push_back(std::move(newMap));
    return newMapPtr;
}

void Engine::createDrawResources(VkExtent3D extents)
{
    // Draw Image
    {
        VkImageUsageFlags drawImageUsages{};
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo createInfo = renderer::vk_helpers::imageCreateInfo(DRAW_FORMAT, drawImageUsages, extents);

        VmaAllocationCreateInfo renderImageAllocationInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };


        VkImageViewCreateInfo renderViewInfo = renderer::vk_helpers::imageviewCreateInfo(DRAW_FORMAT, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);
        drawImage = resourceManager->createResource<renderer::RenderTarget>(createInfo, renderImageAllocationInfo, renderViewInfo,
                                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    // Depth Image
    {
        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo depthImageInfo = renderer::vk_helpers::imageCreateInfo(DEPTH_STENCIL_FORMAT, depthImageUsages, extents);

        constexpr VmaAllocationCreateInfo depthImageAllocationInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };


        VkImageViewCreateInfo combinedViewInfo = renderer::vk_helpers::imageviewCreateInfo(DEPTH_STENCIL_FORMAT, VK_NULL_HANDLE,
                                                                                           VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        depthStencilImage = resourceManager->createResource<renderer::RenderTarget>(depthImageInfo, depthImageAllocationInfo, combinedViewInfo,
                                                                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkImageViewCreateInfo depthViewInfo = renderer::vk_helpers::imageviewCreateInfo(depthStencilImage->imageFormat, depthStencilImage->image,
                                                                                        VK_IMAGE_ASPECT_DEPTH_BIT);
        VkImageViewCreateInfo stencilViewInfo = renderer::vk_helpers::imageviewCreateInfo(depthStencilImage->imageFormat, depthStencilImage->image,
                                                                                          VK_IMAGE_ASPECT_STENCIL_BIT);
        depthImageView = resourceManager->createResource<renderer::ImageView>(depthViewInfo);
        stencilImageView = resourceManager->createResource<renderer::ImageView>(stencilViewInfo);
    }
    // Render Targets
    {
        const auto generateRenderTarget = std::function([this, extents](const VkFormat renderTargetFormat) {
            VkImageUsageFlags usageFlags{};
            usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            const VkImageCreateInfo imageInfo = renderer::vk_helpers::imageCreateInfo(renderTargetFormat, usageFlags, extents);
            constexpr VmaAllocationCreateInfo allocInfo = {
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            };

            VkImageViewCreateInfo imageViewInfo = renderer::vk_helpers::imageviewCreateInfo(
                renderTargetFormat, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);


            return resourceManager->createResource<renderer::RenderTarget>(imageInfo, allocInfo, imageViewInfo,
                                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        });

        normalRenderTarget = generateRenderTarget(NORMAL_FORMAT);
        albedoRenderTarget = generateRenderTarget(ALBEDO_FORMAT);
        pbrRenderTarget = generateRenderTarget(PBR_FORMAT);
        velocityRenderTarget = generateRenderTarget(VELOCITY_FORMAT);
    }
    // TAA Resolve
    {
        VkImageUsageFlags taaResolveUsages{};
        taaResolveUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        taaResolveUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

        const VkImageCreateInfo imageCreateInfo = renderer::vk_helpers::imageCreateInfo(DRAW_FORMAT, taaResolveUsages, extents);

        constexpr VmaAllocationCreateInfo allocInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        VkImageViewCreateInfo imageViewCreateInfo = renderer::vk_helpers::imageviewCreateInfo(DRAW_FORMAT, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);


        taaResolveTarget = resourceManager->createResource<renderer::RenderTarget>(imageCreateInfo, allocInfo, imageViewCreateInfo,
                                                                                   VK_IMAGE_LAYOUT_GENERAL);
    }
    // Draw History
    {
        VkImageUsageFlags historyBufferUsages{};
        historyBufferUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        historyBufferUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkImageCreateInfo imageCreateInfo = renderer::vk_helpers::imageCreateInfo(DRAW_FORMAT, historyBufferUsages, extents);

        constexpr VmaAllocationCreateInfo allocInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        VkImageViewCreateInfo imageViewCreateInfo = renderer::vk_helpers::imageviewCreateInfo(DRAW_FORMAT, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);
        historyBuffer = resourceManager->createResource<renderer::RenderTarget>(imageCreateInfo, allocInfo, imageViewCreateInfo,
                                                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    // Final Image
    {
        VkImageUsageFlags usageFlags{};
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        const VkImageCreateInfo imageCreateInfo = renderer::vk_helpers::imageCreateInfo(DRAW_FORMAT, usageFlags, extents);

        constexpr VmaAllocationCreateInfo allocInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        VkImageViewCreateInfo imageViewCreateInfo = renderer::vk_helpers::imageviewCreateInfo(DRAW_FORMAT, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);
        finalImageBuffer = resourceManager->createResource<renderer::RenderTarget>(imageCreateInfo, allocInfo, imageViewCreateInfo,
                                                                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

#if WILL_ENGINE_DEBUG_DRAW
    // Debug Output (Gizmos, Debug Draws, etc. Output here before combined w/ final image. Goes around normal pass stuff. Expects inputs to be jittered because to test against depth buffer, fragments need to be jittered cause depth buffer is jittered)
    VkImageUsageFlags usageFlags{};
    usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo imageCreateInfo = renderer::vk_helpers::imageCreateInfo(DEBUG_FORMAT, usageFlags, extents);
    constexpr VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VkImageViewCreateInfo imageViewCreateInfo = renderer::vk_helpers::imageviewCreateInfo(DEBUG_FORMAT, VK_NULL_HANDLE, VK_IMAGE_ASPECT_COLOR_BIT);
    debugTarget = resourceManager->createResource<renderer::RenderTarget>(imageCreateInfo, allocInfo, imageViewCreateInfo,
                                                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

#endif
}

void Engine::setCsmSettings(const renderer::CascadedShadowMapSettings& settings)
{
    csmSettings = settings;
    if (cascadedShadowMap) {
        cascadedShadowMap->setCascadedShadowMapProperties(csmSettings);
        cascadedShadowMap->generateSplits();
    }
}

void Engine::hotReloadShaders() const
{
    vkDeviceWaitIdle(context->device);
    cascadedShadowMap->reloadShaders();
    visibilityPassPipeline->reloadShaders();
    environmentPipeline->reloadShaders();
    terrainPipeline->reloadShaders();
    deferredMrtPipeline->reloadShaders();
    transparentPipeline->reloadShaders();
    ambientOcclusionPipeline->reloadShaders();
    contactShadowsPipeline->reloadShaders();
    deferredResolvePipeline->reloadShaders();
    temporalAntialiasingPipeline->reloadShaders();
    postProcessPipeline->reloadShaders();
}

void Engine::handleResize(const renderer::ResolutionChangedEvent& event)
{
    resourceManager->destroyResource(std::move(drawImage));
    resourceManager->destroyResource(std::move(depthStencilImage));
    resourceManager->destroyResource(std::move(depthImageView));
    resourceManager->destroyResource(std::move(stencilImageView));
    resourceManager->destroyResource(std::move(normalRenderTarget));
    resourceManager->destroyResource(std::move(albedoRenderTarget));
    resourceManager->destroyResource(std::move(pbrRenderTarget));
    resourceManager->destroyResource(std::move(velocityRenderTarget));
    resourceManager->destroyResource(std::move(taaResolveTarget));
    resourceManager->destroyResource(std::move(historyBuffer));
    resourceManager->destroyResource(std::move(finalImageBuffer));
#if WILL_ENGINE_DEBUG_DRAW
    resourceManager->destroyResource(std::move(debugTarget));
#endif

    createDrawResources({event.newExtent.width, event.newExtent.height, 1});
}

void Engine::setupDescriptorBuffers() const
{
    ambientOcclusionPipeline->setupDepthPrefilterDescriptorBuffer(depthImageView->imageView);
    ambientOcclusionPipeline->setupAmbientOcclusionDescriptorBuffer(normalRenderTarget->imageView);
    ambientOcclusionPipeline->setupSpatialFilteringDescriptorBuffer();
    contactShadowsPipeline->setupDescriptorBuffer(depthImageView->imageView);

    const renderer::DeferredResolveDescriptor deferredResolveDescriptor{
        normalRenderTarget->imageView,
        albedoRenderTarget->imageView,
        pbrRenderTarget->imageView,
        depthImageView->imageView,
        velocityRenderTarget->imageView,
        ambientOcclusionPipeline->getAmbientOcclusionRenderTarget(),
        contactShadowsPipeline->getContactShadowRenderTarget(),
        drawImage->imageView,
        resourceManager->getDefaultSamplerLinear()
    };
    deferredResolvePipeline->setupDescriptorBuffer(deferredResolveDescriptor);

    const renderer::TemporalAntialiasingDescriptor temporalAntialiasingDescriptor{
        drawImage->imageView,
        historyBuffer->imageView,
        depthImageView->imageView,
        velocityRenderTarget->imageView,
        taaResolveTarget->imageView,
        resourceManager->getDefaultSamplerLinear()

    };
    temporalAntialiasingPipeline->setupDescriptorBuffer(temporalAntialiasingDescriptor);

    const renderer::PostProcessDescriptor postProcessDescriptor{
        taaResolveTarget->imageView,
        finalImageBuffer->imageView,
        resourceManager->getDefaultSamplerLinear()
    };
    postProcessPipeline->setupDescriptorBuffer(postProcessDescriptor);

#if WILL_ENGINE_DEBUG_DRAW
    debugPipeline->setupDescriptorBuffer(debugTarget->imageView, finalImageBuffer->imageView);
    debugHighlighter->setupDescriptorBuffer(stencilImageView->imageView, debugTarget->imageView);
#endif
}

bool Engine::generateDefaultMap()
{
    if (engineSettings.defaultMapToLoad.empty()) {
        fmt::print("Warning: Default Map To Load is empty, loading sample scene\n");
        return false;
    }
    if (!exists(engineSettings.defaultMapToLoad)) {
        fmt::print("Warning: Default Map To Load provided doesn't exist, loading sample scene\n");
        return false;
    }

    if (!createMap(relative(engineSettings.defaultMapToLoad))) {
        fmt::print("Warning: Default Map To Load failed to be loaded, loading sample scene\n");
        return false;
    }

    return true;
}
}
