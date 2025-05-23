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
#include "engine/renderer/descriptor_buffer/descriptor_buffer_uniform.h"
#include "engine/renderer/environment/environment.h"
#include "engine/core/game_object/game_object_factory.h"
#include "engine/renderer/pipelines/shadows/cascaded_shadow_map/cascaded_shadow_map.h"
#include "engine/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline.h"
#include "engine/renderer/pipelines/geometry/deferred_mrt/deferred_mrt_pipeline_types.h"
#include "engine/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline.h"
#include "engine/renderer/pipelines/geometry/deferred_resolve/deferred_resolve_pipeline_types.h"
#include "engine/renderer/pipelines/geometry/environment/environment_pipeline.h"
#include "engine/renderer/pipelines/geometry/environment/environment_pipeline_types.h"
#include "engine/renderer/pipelines/geometry/terrain/terrain_pipeline.h"
#include "engine/renderer/pipelines/geometry/terrain/terrain_pipeline_types.h"
#include "engine/renderer/pipelines/geometry/transparent_pipeline/transparent_pipeline_types.h"
#include "engine/renderer/pipelines/post/post_process/post_process_pipeline.h"
#include "engine/renderer/pipelines/post/temporal_antialiasing/temporal_antialiasing_pipeline.h"
#include "engine/renderer/pipelines/shadows/contact_shadow/contact_shadows_pipeline_types.h"
#include "engine/renderer/pipelines/shadows/ground_truth_ambient_occlusion/ground_truth_ambient_occlusion_pipeline.h"
#include "engine/renderer/pipelines/visibility_pass/visibility_pass_pipeline.h"
#include "engine/renderer/pipelines/visibility_pass/visibility_pass_pipeline_types.h"
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
    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE;
    windowExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height};

    //auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    window = SDL_CreateWindow(
        ENGINE_NAME,
        static_cast<int>(windowExtent.x),
        static_cast<int>(windowExtent.y),
        window_flags);

    input::Input::get().init(window, windowExtent);

    startupProfiler.addEntry("Windowing");


    context = new VulkanContext(window, USE_VALIDATION_LAYERS);

    startupProfiler.addEntry("Vulkan Context");

    createSwapchain(windowExtent.x, windowExtent.y);

    startupProfiler.addEntry("Swapchain");

    // Command Pools
    const VkCommandPoolCreateInfo commandPoolInfo = vk_helpers::commandPoolCreateInfo(context->graphicsQueueFamily,
                                                                                      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
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

    startupProfiler.addEntry("Command Pool and Sync Structures");

    immediate = new ImmediateSubmitter(*context);
    resourceManager = new ResourceManager(*context, *immediate);
    assetManager = new AssetManager(*resourceManager);
    physics = new physics::Physics();
    physics::Physics::set(physics);

    createDrawResources();

#if WILL_ENGINE_DEBUG_DRAW
    debugRenderer = new debug_renderer::DebugRenderer(*resourceManager);
    debug_renderer::DebugRenderer::set(debugRenderer);
    debugPipeline = new debug_pipeline::DebugCompositePipeline(*resourceManager);
    debugPipeline->setupDescriptorBuffer(debugTarget.imageView, finalImageBuffer.imageView);
    debugHighlighter = new debug_highlight_pipeline::DebugHighlighter(*resourceManager);
    debugHighlighter->setupDescriptorBuffer(stencilImageView, debugTarget.imageView);
#endif

    startupProfiler.addEntry("Immediate, ResourceM, AssetM, Physics");

    environmentMap = new environment::Environment(*resourceManager, *immediate);
    startupProfiler.addEntry("Environment");

    auto& componentFactory = components::ComponentFactory::getInstance();
    componentFactory.registerComponents();

    auto& gameObjectFactory = game_object::GameObjectFactory::getInstance();
    gameObjectFactory.registerGameObjects();

    const std::filesystem::path envMapSource = "assets/environments";

    environmentMap->loadEnvironment("Overcast Sky", (envMapSource / "kloofendal_overcast_puresky_4k.hdr").string().c_str(), 2);
    // environmentMap->loadEnvironment("Wasteland", (envMapSource / "wasteland_clouds_puresky_4k.hdr").string().c_str(), 1);
    // environmentMap->loadEnvironment("Belfast Sunset", (envMapSource / "belfast_sunset_puresky_4k.hdr").string().c_str(), 0);
    // environmentMap->loadEnvironment("Rogland Clear Night", (envMapSource / "rogland_clear_night_4k.hdr").string().c_str(), 4);
    startupProfiler.addEntry("Load Environments");


    cascadedShadowMap = new cascaded_shadows::CascadedShadowMap(*resourceManager);
    cascadedShadowMap->setCascadedShadowMapProperties(csmSettings);
    cascadedShadowMap->generateSplits();

    startupProfiler.addEntry("CSM");

    if (engine_constants::useImgui) {
        imguiWrapper = new ImguiWrapper(*context, {window, swapchainImageFormat});
    }


    initRenderer();

    initGame();
    startupProfiler.addEntry("Init Game");

    profiler.addTimer("0Physics");
    profiler.addTimer("1Game");
    profiler.addTimer("2Render");
    profiler.addTimer("3Total");

    Serializer::deserializeEngineSettings(this);

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    fmt::print("Finished Initialization in {} seconds\n", static_cast<float>(elapsed.count()) / 1000000.0f);
}

void Engine::initRenderer()
{
    for (int i{0}; i < FRAME_OVERLAP; i++) {
        sceneDataBuffers[i] = resourceManager->createHostSequentialBuffer(sizeof(SceneData));
    }
    // +1 for the debug scene data buffer. Not multi-buffering for simplicity
    sceneDataDescriptorBuffer = DescriptorBufferUniform(*context, resourceManager->getSceneDataLayout(), FRAME_OVERLAP + 1);
    std::vector<DescriptorUniformData> sceneDataBufferData{1};
    for (int i{0}; i < FRAME_OVERLAP; i++) {
        sceneDataBufferData[0] = DescriptorUniformData{.uniformBuffer = sceneDataBuffers[i], .allocSize = sizeof(SceneData)};
        sceneDataDescriptorBuffer.setupData(context->device, sceneDataBufferData, i);
    }
    debugSceneDataBuffer = resourceManager->createHostSequentialBuffer(sizeof(SceneData));
    sceneDataBufferData[0] = DescriptorUniformData{.uniformBuffer = debugSceneDataBuffer, .allocSize = sizeof(SceneData)};
    sceneDataDescriptorBuffer.setupData(context->device, sceneDataBufferData, FRAME_OVERLAP);

    visibilityPassPipeline = new visibility_pass_pipeline::VisibilityPassPipeline(*resourceManager);
    startupProfiler.addEntry("Init Vis Pass");
    environmentPipeline = new environment_pipeline::EnvironmentPipeline(*resourceManager, environmentMap->getCubemapDescriptorSetLayout());
    startupProfiler.addEntry("Init Environment Pass");
    terrainPipeline = new terrain::TerrainPipeline(*resourceManager);
    startupProfiler.addEntry("Init Terrain Pass");
    deferredMrtPipeline = new deferred_mrt::DeferredMrtPipeline(*resourceManager);
    startupProfiler.addEntry("Init Deffered MRT Pass");
    ambientOcclusionPipeline = new ambient_occlusion::GroundTruthAmbientOcclusionPipeline(*resourceManager);
    startupProfiler.addEntry("Init GTAO Pass");
    contactShadowsPipeline = new contact_shadows_pipeline::ContactShadowsPipeline(*resourceManager);
    startupProfiler.addEntry("Init SSS Pass");
    deferredResolvePipeline = new deferred_resolve::DeferredResolvePipeline(*resourceManager, environmentMap->getDiffSpecMapDescriptorSetlayout(),
                                                                            cascadedShadowMap->getCascadedShadowMapUniformLayout(),
                                                                            cascadedShadowMap->getCascadedShadowMapSamplerLayout());
    startupProfiler.addEntry("Init Deferred Resolve Pass");
    temporalAntialiasingPipeline = new temporal_antialiasing_pipeline::TemporalAntialiasingPipeline(*resourceManager);
    startupProfiler.addEntry("Init TAA Pass");
    transparentPipeline = new transparent_pipeline::TransparentPipeline(*resourceManager, environmentMap->getDiffSpecMapDescriptorSetlayout(),
                                                                        cascadedShadowMap->getCascadedShadowMapUniformLayout(),
                                                                        cascadedShadowMap->getCascadedShadowMapSamplerLayout());
    startupProfiler.addEntry("Init Transparent Pass");
    postProcessPipeline = new post_process_pipeline::PostProcessPipeline(*resourceManager);
    startupProfiler.addEntry("Init PP Pass");

    ambientOcclusionPipeline->setupDepthPrefilterDescriptorBuffer(depthImageView);
    ambientOcclusionPipeline->setupAmbientOcclusionDescriptorBuffer(normalRenderTarget.imageView);
    ambientOcclusionPipeline->setupSpatialFilteringDescriptorBuffer();
    contactShadowsPipeline->setupDescriptorBuffer(depthImageView);

    const deferred_resolve::DeferredResolveDescriptor deferredResolveDescriptor{
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        depthImageView,
        velocityRenderTarget.imageView,
        ambientOcclusionPipeline->getAmbientOcclusionRenderTarget().imageView,
        contactShadowsPipeline->getContactShadowRenderTarget().imageView,
        drawImage.imageView,
        resourceManager->getDefaultSamplerLinear()
    };
    deferredResolvePipeline->setupDescriptorBuffer(deferredResolveDescriptor);

    const temporal_antialiasing_pipeline::TemporalAntialiasingDescriptor temporalAntialiasingDescriptor{
        drawImage.imageView,
        historyBuffer.imageView,
        depthImageView,
        velocityRenderTarget.imageView,
        taaResolveTarget.imageView,
        resourceManager->getDefaultSamplerLinear()

    };
    temporalAntialiasingPipeline->setupDescriptorBuffer(temporalAntialiasingDescriptor);

    const post_process_pipeline::PostProcessDescriptor postProcessDescriptor{
        taaResolveTarget.imageView,
        finalImageBuffer.imageView,
        resourceManager->getDefaultSamplerLinear()
    };
    postProcessPipeline->setupDescriptorBuffer(postProcessDescriptor);
}

void Engine::initGame()
{
    assetManager->scanForAll();
    camera = new FreeCamera();
    createMap(file::getSampleScene());
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
            if (e.type == SDL_EVENT_WINDOW_RESTORED) { bStopRendering = true; }
            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bResizeRequested = true;
                fmt::print("Window resized, resize requested\n");
            }

            if (engine_constants::useImgui) {
                ImguiWrapper::handleInput(e);
            }

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

        render(deltaTime);
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
    if (camera) { camera->update(deltaTime); }

#if WILL_ENGINE_DEBUG
    // Non-Core Gameplay Actions
    const input::Input& input = input::Input::get();
    if (input.isKeyPressed(input::Key::R)) {
        if (camera) {
            const glm::vec3 direction = camera->getForwardWS();
            const physics::RaycastHit result = physics::PhysicsUtils::raycast(camera->getPosition(), direction, 100.0f, {}, {}, {});

            if (result.hasHit) {
                physics::PhysicsUtils::addImpulseAtPosition(result.hitBodyID, normalize(direction) * 100.0f, result.hitPosition);
            }
            else {
                fmt::print("Failed to find an object with the raycast\n");
            }
        }
    }

    if (input.isKeyPressed(input::Key::G)) {
        if (camera) {
            if (auto transformable = dynamic_cast<ITransformable*>(selectedItem)) {
                glm::vec3 itemPosition = transformable->getPosition();

                glm::vec3 itemScale = transformable->getScale();
                float maxDimension = glm::max(glm::max(itemScale.x, itemScale.y), itemScale.z);

                float distance = glm::max(5.0f, maxDimension * 3.0f);

                glm::vec3 currentCamPos = camera->getTransform().getPosition();
                glm::vec3 currentDirection = glm::normalize(itemPosition - currentCamPos);

                glm::vec3 newCamPos = itemPosition - (currentDirection * distance);

                glm::vec3 forward = glm::normalize(newCamPos - itemPosition);
                glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
                glm::vec3 up = glm::cross(forward, right);

                glm::mat3 rotMatrix(right, up, forward);
                glm::quat newRotation = glm::quat_cast(rotMatrix);

                camera->setCameraTransform(newCamPos, newRotation);
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
}

void Engine::updateRender(VkCommandBuffer cmd, const float deltaTime, const int32_t currentFrameOverlap, const int32_t previousFrameOverlap) const
{
    const AllocatedBuffer& previousSceneDataBuffer = sceneDataBuffers[previousFrameOverlap];
    const AllocatedBuffer& sceneDataBuffer = sceneDataBuffers[currentFrameOverlap];
    const auto pSceneData = static_cast<SceneData*>(sceneDataBuffer.info.pMappedData);
    const auto pPreviousSceneData = static_cast<SceneData*>(previousSceneDataBuffer.info.pMappedData);

    const bool bIsFrameZero = frameNumber == 0;
    glm::vec2 prevJitter = HaltonSequence::getJitterHardcoded(bIsFrameZero ? frameNumber : frameNumber - 1) - 0.5f;
    prevJitter.x /= RENDER_EXTENT_WIDTH;
    prevJitter.y /= RENDER_EXTENT_HEIGHT;
    glm::vec2 currentJitter = HaltonSequence::getJitterHardcoded(frameNumber) - 0.5f;
    currentJitter.x /= RENDER_EXTENT_WIDTH;
    currentJitter.y /= RENDER_EXTENT_HEIGHT;

    pSceneData->jitter = taaSettings.bEnabled ? glm::vec4(currentJitter.x, currentJitter.y, prevJitter.x, prevJitter.y) : glm::vec4(0.0f);

    const glm::mat4 cameraLook = glm::lookAt(glm::vec3(0), camera->getForwardWS(), glm::vec3(0, 1, 0));

    pSceneData->prevView = bIsFrameZero ? camera->getViewMatrix() : pPreviousSceneData->view;
    pSceneData->prevProj = bIsFrameZero ? camera->getProjMatrix() : pPreviousSceneData->proj;
    pSceneData->prevViewProj = bIsFrameZero ? camera->getViewProjMatrix() : pPreviousSceneData->viewProj;

    pSceneData->prevInvView = bIsFrameZero ? inverse(camera->getViewMatrix()) : pPreviousSceneData->invView;
    pSceneData->prevInvProj = bIsFrameZero ? inverse(camera->getProjMatrix()) : pPreviousSceneData->invProj;
    pSceneData->prevInvViewProj = bIsFrameZero ? inverse(camera->getViewProjMatrix()) : pPreviousSceneData->invViewProj;

    pSceneData->prevViewProjCameraLookDirection = bIsFrameZero ? pSceneData->proj * cameraLook : pPreviousSceneData->viewProjCameraLookDirection;

    pSceneData->view = camera->getViewMatrix();
    pSceneData->proj = camera->getProjMatrix();
    pSceneData->viewProj = pSceneData->proj * pSceneData->view;

    pSceneData->invView = glm::inverse(pSceneData->view);
    pSceneData->invProj = glm::inverse(pSceneData->proj);
    pSceneData->invViewProj = glm::inverse(pSceneData->viewProj);

    pSceneData->viewProjCameraLookDirection = pSceneData->proj * cameraLook;

    pSceneData->prevCameraWorldPos = bIsFrameZero ? camera->getPosition() : pPreviousSceneData->cameraWorldPos;
    pSceneData->cameraWorldPos = camera->getPosition();


    pSceneData->renderTargetSize = {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    pSceneData->texelSize = {1.0f / RENDER_EXTENT_WIDTH, 1.0f / RENDER_EXTENT_HEIGHT};
    pSceneData->cameraPlanes = {camera->getNearPlane(), camera->getFarPlane()};
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
    pDebugSceneData->invViewProj = glm::inverse(pDebugSceneData->viewProj);

    pDebugSceneData->prevCameraWorldPos = glm::vec4(0.0f);
    pDebugSceneData->cameraWorldPos = glm::vec4(0.0f);

    pDebugSceneData->renderTargetSize = {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT};
    pDebugSceneData->texelSize = {1.0f / RENDER_EXTENT_WIDTH, 1.0f / RENDER_EXTENT_HEIGHT};
    pDebugSceneData->deltaTime = deltaTime;

    vk_helpers::uniformBarrier(cmd, sceneDataBuffer, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
    vk_helpers::uniformBarrier(cmd, debugSceneDataBuffer, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
}

void Engine::updateDebug(float deltaTime)
{
#if WILL_ENGINE_DEBUG_DRAW
    if (selectedItem) {
        if (const auto gameObject = dynamic_cast<game_object::GameObject*>(selectedItem)) {
            if (const components::RigidBodyComponent* rb = gameObject->getRigidbody()) {
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
                const glm::vec3 direction = camera->screenToWorldDirection(input.getMousePosition());
                if (const physics::RaycastHit result = physics::PhysicsUtils::raycast(camera->getPosition(),
                                                                                      normalize(direction) * camera->getNearPlane())) {
                    if (const physics::PhysicsObject* physicsObject = physics->getPhysicsObject(result.hitBodyID)) {
                        if (const auto* component = dynamic_cast<components::Component*>(physicsObject->physicsBody)) {
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
            debug_renderer::DebugRenderer::drawBox(offset + glm::vec3(i, j, z), {0.8f, 0.8f, 0.8f}, {0.0f, 0.7f, 0.1f});
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
        bResizeRequested = true;
        fmt::print("Swapchain out of date or suboptimal, resize requested (At Acquire)\n");
        return;
    }

    int32_t currentFrameOverlap = getCurrentFrameOverlap();
    int32_t previousFrameOverlap = getPreviousFrameOverlap();

    // Destroy all resources queued to be destroyed on this frame (from FRAME_OVERLAP frames ago)
    resourceManager->update(currentFrameOverlap);

    VkCommandBuffer cmd = getCurrentFrame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = vk_helpers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // only submit once
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    profiler.beginTimer("2Render");


    std::vector<RenderObject*> allRenderObjects = assetManager->getAllRenderObjects();

    // Update Render Object Buffers and Model Matrices
    for (RenderObject* renderObject : allRenderObjects) {
        renderObject->update(cmd, currentFrameOverlap, previousFrameOverlap);
    }

    for (ITerrain* terrain : activeTerrains) {
        if (auto chunk = terrain->getTerrainChunk()) {
            chunk->update(currentFrameOverlap, previousFrameOverlap);
        }
    }

    // Updates Scene Data buffer
    updateRender(cmd, deltaTime, currentFrameOverlap, previousFrameOverlap);

    // Updates Cascaded Shadow Map Properties
    cascadedShadowMap->update(mainLight, camera, currentFrameOverlap);

    visibility_pass_pipeline::VisibilityPassDrawInfo csmFrustumCullDrawInfo{
        currentFrameOverlap,
        allRenderObjects,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        false,
        true,
        true,
    };
    visibilityPassPipeline->draw(cmd, csmFrustumCullDrawInfo);


    cascaded_shadows::CascadedShadowMapDrawInfo csmDrawInfo{
        csmSettings.bEnabled,
        currentFrameOverlap,
        allRenderObjects,
        activeTerrains,
    };

    cascadedShadowMap->draw(cmd, csmDrawInfo);

    vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_COLOR_BIT, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vk_helpers::imageBarrier(cmd, depthStencilImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    vk_helpers::imageBarrier(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);

    environment_pipeline::EnvironmentDrawInfo environmentPipelineDrawInfo{
        true,
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        velocityRenderTarget.imageView,
        depthImageView,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        environmentMap->getCubemapDescriptorBuffer().getDescriptorBufferBindingInfo(),
        environmentMap->getCubemapDescriptorBuffer().getDescriptorBufferSize() * environmentMapIndex,
    };
    environmentPipeline->draw(cmd, environmentPipelineDrawInfo);


    visibility_pass_pipeline::VisibilityPassDrawInfo deferredFrustumCullDrawInfo{
        currentFrameOverlap,
        allRenderObjects,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        true,
        false,
        true,
    };
    visibilityPassPipeline->draw(cmd, deferredFrustumCullDrawInfo);

    terrain::TerrainDrawInfo terrainDrawInfo{
        false,
        bDrawTerrainLines,
        currentFrameOverlap,
        {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT},
        activeTerrains,
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        velocityRenderTarget.imageView,
        depthImageView,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };
    terrainPipeline->draw(cmd, terrainDrawInfo);

    const deferred_mrt::DeferredMrtDrawInfo deferredMrtDrawInfo{
        false,
        currentFrameOverlap,
        {RENDER_EXTENT_WIDTH, RENDER_EXTENT_HEIGHT},
        allRenderObjects,
        normalRenderTarget.imageView,
        albedoRenderTarget.imageView,
        pbrRenderTarget.imageView,
        velocityRenderTarget.imageView,
        depthImageView,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };
    deferredMrtPipeline->draw(cmd, deferredMrtDrawInfo);

    if (bEnableDebugFrustumCullDraw) {
        const deferred_mrt::DeferredMrtDrawInfo debugDeferredMrtDrawInfo{
            false,
            currentFrameOverlap,
            {RENDER_EXTENT_WIDTH / 3.0f, RENDER_EXTENT_HEIGHT / 3.0f},
            allRenderObjects,
            normalRenderTarget.imageView,
            albedoRenderTarget.imageView,
            pbrRenderTarget.imageView,
            velocityRenderTarget.imageView,
            depthImageView,
            sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
            sceneDataDescriptorBuffer.getDescriptorBufferSize() * FRAME_OVERLAP
        };
        deferredMrtPipeline->draw(cmd, debugDeferredMrtDrawInfo);
    }

    transparent_pipeline::TransparentAccumulateDrawInfo transparentDrawInfo{
        true,
        depthImageView,
        currentFrameOverlap,
        allRenderObjects,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        environmentMap->getDiffSpecMapDescriptorBuffer().getDescriptorBufferBindingInfo(),
        environmentMap->getDiffSpecMapDescriptorBuffer().getDescriptorBufferSize() * environmentMapIndex,
        cascadedShadowMap->getCascadedShadowMapUniformBuffer().getDescriptorBufferBindingInfo(),
        cascadedShadowMap->getCascadedShadowMapUniformBuffer().getDescriptorBufferSize() * currentFrameOverlap,
        cascadedShadowMap->getCascadedShadowMapSamplerBuffer().getDescriptorBufferBindingInfo(),
    };
    if (bDrawTransparents) {
        transparentPipeline->drawAccumulate(cmd, transparentDrawInfo);
    }

    vk_helpers::imageBarrier(cmd, normalRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, albedoRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, pbrRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, velocityRenderTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, depthStencilImage.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    vk_helpers::imageBarrier(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    ambient_occlusion::GTAODrawInfo gtaoDrawInfo{
        camera,
        gtaoSettings.bEnabled,
        gtaoSettings.pushConstants,
        frameNumber,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };
    ambientOcclusionPipeline->draw(cmd, gtaoDrawInfo);

    contact_shadows_pipeline::ContactShadowsDrawInfo contactDrawInfo{
        camera,
        mainLight,
        sssSettings.bEnabled,
        sssSettings.pushConstants,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };

    contactShadowsPipeline->draw(cmd, contactDrawInfo);

    const deferred_resolve::DeferredResolveDrawInfo deferredResolveDrawInfo{
        deferredDebug,
        csmSettings.pcfLevel,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap,
        environmentMap->getDiffSpecMapDescriptorBuffer().getDescriptorBufferBindingInfo(),
        environmentMap->getDiffSpecMapDescriptorBuffer().getDescriptorBufferSize() * environmentMapIndex,
        cascadedShadowMap->getCascadedShadowMapUniformBuffer().getDescriptorBufferBindingInfo(),
        cascadedShadowMap->getCascadedShadowMapUniformBuffer().getDescriptorBufferSize() * currentFrameOverlap,
        cascadedShadowMap->getCascadedShadowMapSamplerBuffer().getDescriptorBufferBindingInfo(),
        camera->getNearPlane(),
        camera->getFarPlane(),
        bEnableShadows,
        bEnableContactShadows
    };
    deferredResolvePipeline->draw(cmd, deferredResolveDrawInfo);

    vk_helpers::imageBarrier(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    transparent_pipeline::TransparentCompositeDrawInfo compositeDrawInfo{
        drawImage.imageView
    };
    if (bDrawTransparents) {
        transparentPipeline->drawComposite(cmd, compositeDrawInfo);
    }

    vk_helpers::imageBarrier(cmd, drawImage.image, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    const VkImageLayout originLayout = frameNumber == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vk_helpers::imageBarrier(cmd, historyBuffer.image, originLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

    const temporal_antialiasing_pipeline::TemporalAntialiasingDrawInfo taaDrawInfo{
        taaSettings.blendValue,
        taaSettings.bEnabled ? 0 : 1,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };
    temporalAntialiasingPipeline->draw(cmd, taaDrawInfo);

    // Copy to TAA History
    vk_helpers::imageBarrier(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, historyBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, taaResolveTarget.image, historyBuffer.image, RENDER_EXTENTS, RENDER_EXTENTS);

    vk_helpers::imageBarrier(cmd, taaResolveTarget.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::imageBarrier(cmd, finalImageBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);


    const post_process_pipeline::PostProcessDrawInfo postProcessDrawInfo{
        postProcessData,
        sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
        sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
    };

    postProcessPipeline->draw(cmd, postProcessDrawInfo);

#if WILL_ENGINE_DEBUG_DRAW
    // Ensure all real rendering happens before this step, as debug draws do write to the depth buffer.
    // This should ALWAYS be the final step before copying to swapchain
    if (bDrawDebugRendering) {
        vk_helpers::clearColorImage(cmd, VK_IMAGE_ASPECT_COLOR_BIT, debugTarget.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {0.0f, 0.0f, 0.0f, 0.0f});
        vk_helpers::imageBarrier(cmd, depthStencilImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);


        debug_renderer::DebugRendererDrawInfo debugRendererDrawInfo{
            currentFrameOverlap,
            debugTarget.imageView,
            depthImageView,
            sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
            sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
        };

        debugRenderer->draw(cmd, debugRendererDrawInfo);

        bool stencilDrawn = false;

        debug_highlight_pipeline::DebugHighlighterDrawInfo highlightDrawInfo{
            nullptr,
            depthStencilImage,
            sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
            sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
        };


        if (IComponentContainer* cc = dynamic_cast<IComponentContainer*>(selectedItem)) {
            if (components::MeshRendererComponent* meshRenderer = cc->getComponent<components::MeshRendererComponent>()) {
                highlightDrawInfo.highlightTarget = meshRenderer;
                stencilDrawn = debugHighlighter->drawHighlightStencil(cmd, highlightDrawInfo);
            }
        }

        if (stencilDrawn) {
            vk_helpers::imageBarrier(cmd, debugTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
            vk_helpers::imageBarrier(cmd, depthStencilImage.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                     VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

            debugHighlighter->drawHighlightProcessing(cmd, highlightDrawInfo);


            vk_helpers::imageBarrier(cmd, debugTarget.image, VK_IMAGE_LAYOUT_GENERAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
        }
        else {
            vk_helpers::imageBarrier(cmd, debugTarget.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
        }


        debug_pipeline::DebugCompositePipelineDrawInfo drawInfo{
            sceneDataDescriptorBuffer.getDescriptorBufferBindingInfo(),
            sceneDataDescriptorBuffer.getDescriptorBufferSize() * currentFrameOverlap
        };

        // Composite all draws in the debug image into `finalImageBuffer`
        debugPipeline->draw(cmd, drawInfo);
    }
#endif
    vk_helpers::imageBarrier(cmd, finalImageBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);

    vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    vk_helpers::copyImageToImage(cmd, finalImageBuffer.image, swapchainImages[swapchainImageIndex], RENDER_EXTENTS, swapchainExtent);

    vk_helpers::imageBarrier(cmd, finalImageBuffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);

    if (engine_constants::useImgui) {
        vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        imguiWrapper->drawImgui(cmd, swapchainImageViews[swapchainImageIndex], swapchainExtent);

        vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                 VK_IMAGE_ASPECT_COLOR_BIT);
    }
    else {
        vk_helpers::imageBarrier(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // End Command Buffer Recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    profiler.endTimer("2Render");

    // Submission
    const VkCommandBufferSubmitInfo cmdSubmitInfo = vk_helpers::commandBufferSubmitInfo(cmd);
    const VkSemaphoreSubmitInfo waitInfo = vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                           getCurrentFrame()._swapchainSemaphore);
    const VkSemaphoreSubmitInfo signalInfo =
            vk_helpers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);
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

    if (engineSettings.saveOnExit) {
        Serializer::serializeEngineSettings(this);
    }

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

    for (AllocatedBuffer sceneBuffer : sceneDataBuffers) {
        resourceManager->destroy(sceneBuffer);
    }
    resourceManager->destroy(debugSceneDataBuffer);
    resourceManager->destroy(sceneDataDescriptorBuffer);

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

    resourceManager->destroy(drawImage);
    resourceManager->destroy(depthStencilImage);
    resourceManager->destroy(depthImageView);
    resourceManager->destroy(stencilImageView);
    resourceManager->destroy(normalRenderTarget);
    resourceManager->destroy(albedoRenderTarget);
    resourceManager->destroy(pbrRenderTarget);
    resourceManager->destroy(velocityRenderTarget);
    resourceManager->destroy(taaResolveTarget);
    resourceManager->destroy(historyBuffer);
    resourceManager->destroy(finalImageBuffer);

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
    resourceManager->destroy(debugTarget);
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
    if (const auto gameObject = dynamic_cast<game_object::GameObject*>(selectedItem)) {
        if (const components::RigidBodyComponent* rb = gameObject->getRigidbody()) {
            if (rb->hasRigidBody()) {
                physics::Physics::get()->stopDrawDebug(rb->getPhysicsBodyId());
            }
        }
    }
    selectedItem = nullptr;
}
#endif

IHierarchical* Engine::createGameObject(Map* map, const std::string& name)
{
    auto newGameObject = std::make_unique<game_object::GameObject>(name);
    auto newGameObjectPtr = newGameObject.get();
    map->addChild(std::move(newGameObject));
    return newGameObjectPtr;
}

void Engine::addToBeginQueue(IHierarchical* obj)
{
    hierarchalBeginQueue.push_back(obj);
}

void Engine::addToMapDeletionQueue(Map* obj)
{
    auto it = std::ranges::find_if(activeMaps,
                                   [obj](const std::unique_ptr<Map>& ptr) {
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

    int32_t w, h;
    // get new window size
    SDL_GetWindowSize(window, &w, &h);
    windowExtent = {w, h};


    createSwapchain(windowExtent.x, windowExtent.y);
    input::Input::get().setWindowExtent(windowExtent);

    bResizeRequested = false;
    fmt::print("Window extent has been updated to {}x{}\n", windowExtent.x, windowExtent.y);
}

Map* Engine::createMap(const std::filesystem::path& path)
{
    auto newMap = std::make_unique<Map>(path, *resourceManager);
    Map* newMapPtr = newMap.get();
    activeMaps.push_back(std::move(newMap));
    return newMapPtr;
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
        depthStencilImage.imageFormat = DEPTH_STENCIL_FORMAT;
        constexpr VkExtent3D depthImageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
        depthStencilImage.imageExtent = depthImageExtent;
        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageCreateInfo depthImageInfo = vk_helpers::imageCreateInfo(depthStencilImage.imageFormat, depthImageUsages, depthImageExtent);

        VmaAllocationCreateInfo depthImageAllocationInfo = {};
        depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        depthImageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &depthImageInfo, &depthImageAllocationInfo, &depthStencilImage.image, &depthStencilImage.allocation,
                       nullptr);

        VkImageViewCreateInfo combinedViewInfo = vk_helpers::imageviewCreateInfo(depthStencilImage.imageFormat, depthStencilImage.image,
                                                                                 VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        VK_CHECK(vkCreateImageView(context->device, &combinedViewInfo, nullptr, &depthStencilImage.imageView));

        VkImageViewCreateInfo depthViewInfo = vk_helpers::imageviewCreateInfo(depthStencilImage.imageFormat, depthStencilImage.image,
                                                                              VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(context->device, &depthViewInfo, nullptr, &depthImageView));

        VkImageViewCreateInfo stencilViewInfo = vk_helpers::imageviewCreateInfo(depthStencilImage.imageFormat, depthStencilImage.image,
                                                                                VK_IMAGE_ASPECT_STENCIL_BIT);
        VK_CHECK(vkCreateImageView(context->device, &stencilViewInfo, nullptr, &stencilImageView));
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
        vmaCreateImage(context->allocator, &taaResolveImageInfo, &taaResolveAllocationInfo, &taaResolveTarget.image, &taaResolveTarget.allocation,
                       nullptr);

        const VkImageViewCreateInfo taaResolveImageViewInfo = vk_helpers::imageviewCreateInfo(
            taaResolveTarget.imageFormat, taaResolveTarget.image, VK_IMAGE_ASPECT_COLOR_BIT);
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
        vmaCreateImage(context->allocator, &historyBufferImageInfo, &historyBufferAllocationInfo, &historyBuffer.image, &historyBuffer.allocation,
                       nullptr);

        const VkImageViewCreateInfo historyBufferImageViewInfo = vk_helpers::imageviewCreateInfo(
            historyBuffer.imageFormat, historyBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &historyBufferImageViewInfo, nullptr, &historyBuffer.imageView));
    }
    // Final Image
    {
        finalImageBuffer.imageFormat = DRAW_FORMAT;
        constexpr VkExtent3D imageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
        finalImageBuffer.imageExtent = imageExtent;
        VkImageUsageFlags usageFlags{};
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        const VkImageCreateInfo imageCreateInfo = vk_helpers::imageCreateInfo(finalImageBuffer.imageFormat, usageFlags, imageExtent);

        VmaAllocationCreateInfo imageAllocationInfo = {};
        imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreateImage(context->allocator, &imageCreateInfo, &imageAllocationInfo, &finalImageBuffer.image,
                       &finalImageBuffer.allocation, nullptr);

        VkImageViewCreateInfo rview_info = vk_helpers::imageviewCreateInfo(finalImageBuffer.imageFormat, finalImageBuffer.image,
                                                                           VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(context->device, &rview_info, nullptr, &finalImageBuffer.imageView));
    }

#if WILL_ENGINE_DEBUG_DRAW
    // Debug Output (Gizmos, Debug Draws, etc. Output here before combined w/ final image. Goes around normal pass stuff. Expects inputs to be jittered because to test against depth buffer, fragments need to be jittered cause depth buffer is jittered)
    constexpr VkExtent3D imageExtent = {RENDER_EXTENTS.width, RENDER_EXTENTS.height, 1};
    VkImageUsageFlags usageFlags{};
    usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo imageCreateInfo = vk_helpers::imageCreateInfo(DEBUG_FORMAT, usageFlags, imageExtent);
    debugTarget = resourceManager->createImage(imageCreateInfo);
#endif
}

void Engine::setCsmSettings(const cascaded_shadows::CascadedShadowMapSettings& settings)
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
}
