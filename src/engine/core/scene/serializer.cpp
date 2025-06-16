//
// Created by William on 2025-02-06.
//

#include "serializer.h"

#include "engine/core/engine.h"
#include "engine/core/engine_types.h"
#include "engine/core/game_object/game_object_factory.h"
#include "engine/renderer/lighting/directional_light.h"

namespace will_engine
{
void Serializer::serializeGameObject(ordered_json& j, IHierarchical* obj)

{
    if (const auto gameObject = dynamic_cast<game_object::GameObject*>(obj)) {
        j["id"] = gameObject->getId();
        j["gameObjectType"] = gameObject->getComponentType();
    }

    j["name"] = obj->getName();

    if (const auto transformable = dynamic_cast<ITransformable*>(obj)) {
        j["transform"] = transformable->getLocalTransform();
    }

    if (const auto componentContainer = dynamic_cast<IComponentContainer*>(obj)) {
        const std::vector<components::Component*> components = componentContainer->getAllComponents();
        if (components.size() > 0) {
            ordered_json componentsJson;
            for (const auto& component : components) {
                ordered_json componentData;
                component->serialize(componentData);
                std::string baseKey = component->getComponentName();
                std::string key = baseKey;
                int32_t counter = 0;

                if (componentsJson.contains(key)) {
                    size_t i = baseKey.size();
                    while (i > 0 && std::isdigit(baseKey[i - 1])) {
                        --i;
                    }

                    if (i < baseKey.size()) {
                        try {
                            counter = std::stoi(baseKey.substr(i));
                            baseKey = baseKey.substr(0, i);
                        } catch (...) {
                            counter = 0;
                        }
                    }

                    while (componentsJson.contains(key)) {
                        ++counter;
                        if (!baseKey.empty() && std::isdigit(baseKey.back())) {
                            key = baseKey + "_" + std::to_string(counter);
                        }
                        else {
                            key = baseKey + std::to_string(counter);
                        }
                    }
                }

                componentsJson[key] = componentData;
                componentsJson[key]["componentType"] = component->getComponentType();
            }
            j["components"] = componentsJson;
        }
    }

    if (!obj->getChildren().empty()) {
        ordered_json children = ordered_json::array();
        for (IHierarchical* child : obj->getChildren()) {
            ordered_json childJson;
            if (child == nullptr) {
                fmt::print("SerializeGameObject: null game object found in IHierarchical chain (child of {})\n", obj->getName());
                continue;
            }
            serializeGameObject(childJson, child);
            children.push_back(childJson);
        }
        j["children"] = children;
    }
}

std::unique_ptr<IHierarchical> Serializer::deserializeGameObject(const ordered_json& j)
{
    auto& gameObjectFactory = game_object::GameObjectFactory::getInstance();
    std::unique_ptr<game_object::GameObject> gameObject{nullptr};
    std::string name{};

    if (j.contains("name")) {
        name = j["name"].get<std::string>();
    }

    if (j.contains("gameObjectType")) {
        const std::string type = j["gameObjectType"];
        gameObject = gameObjectFactory.createGameObject(type, name);
    }

    if (!gameObject) {
        gameObject = gameObjectFactory.createGameObject(game_object::GameObject::getStaticType(), "");
    }


    if (j.contains("id")) {
        gameObject->setId(j["id"].get<uint32_t>());
    }

    if (j.contains("transform")) {
        gameObject->setLocalTransform(j["transform"].get<Transform>());
    }

    if (j.contains("components")) {
        auto& factory = components::ComponentFactory::getInstance();

        const auto& components = j["components"];
        for (const auto& [componentName, componentData] : components.items()) {
            if (!componentData.contains("componentType")) {
                // component must contain type, it is invalid otherwise.
                continue;
            }
            auto componentType = componentData["componentType"].get<std::string>();


            std::unique_ptr<components::Component> newComponent = factory.createComponent(componentType, componentName);
            components::Component* component = gameObject->addComponent(std::move(newComponent));


            if (!component) {
                fmt::print("Component failed to be created ({})", componentType);
                continue;
            }

            auto orderedComponentData = ordered_json(componentData);
            component->deserialize(orderedComponentData);
        }
    }

    if (j.contains("children")) {
        const auto& children = j["children"];
        for (const auto& childJson : children) {
            if (std::unique_ptr<IHierarchical> child = deserializeGameObject(childJson)) {
                gameObject->addChild(std::move(child));
            }
        }
    }

    return gameObject;
}

bool Serializer::serializeMap(IHierarchical* map, ordered_json& rootJ, const std::filesystem::path& filepath)
{
    if (map == nullptr) {
        fmt::print("Warning: map is null\n");
        return false;
    }

    rootJ["version"] = EngineVersion::current();
    rootJ["metadata"] = SceneMetadata::create(map->getName().data());

    if (const auto componentContainer = dynamic_cast<IComponentContainer*>(map)) {
        const std::vector<components::Component*> components = componentContainer->getAllComponents();
        if (components.size() > 0) {
            ordered_json componentsJson;
            for (const auto& component : components) {
                ordered_json componentData;
                component->serialize(componentData);
                componentsJson[component->getComponentType()] = componentData;
                componentsJson[component->getComponentType()]["componentName"] = component->getComponentNameView();
            }
            rootJ["rootComponents"] = componentsJson;
        }
    }

    ordered_json gameObjectJ;
    ordered_json renderObjectJ;

    serializeGameObject(gameObjectJ, map);
    rootJ["gameObjects"] = gameObjectJ;

    std::ofstream file(filepath);
    file << rootJ.dump(4);

    return true;
}

bool Serializer::deserializeMap(IHierarchical* root, ordered_json& rootJ)
{
    if (rootJ.contains("gameObjects")) {
        for (auto child : rootJ["gameObjects"]["children"]) {
            std::unique_ptr<IHierarchical> childObject = deserializeGameObject(child);
            root->addChild(std::move(childObject));
        }
    }

    if (rootJ.contains("rootComponents")) {
        if (const auto componentContainer = dynamic_cast<IComponentContainer*>(root)) {
            const auto& components = rootJ["rootComponents"];
            for (const auto& [componentType, componentData] : components.items()) {
                std::string componentName;
                if (componentData.contains("componentName")) {
                    componentName = componentData["componentName"].get<std::string>();
                }
                else {
                    componentName = componentType;
                }

                auto& factory = components::ComponentFactory::getInstance();
                auto newComponent = factory.createComponent(componentType, componentName);

                if (newComponent) {
                    auto orderedComponentData = ordered_json(componentData);
                    const auto _component = componentContainer->addComponent(std::move(newComponent));
                    _component->deserialize(orderedComponentData);
                }
            }
        }
    }

    return true;
}

bool Serializer::generateWillModel(const std::filesystem::path& gltfPath, const std::filesystem::path& outputPath)
{
    RenderObjectInfo info;
    info.gltfPath = gltfPath.string();
    info.name = gltfPath.stem().string();
    info.id = computePathHash(info.gltfPath);

    nlohmann::json j;
    j["version"] = WILL_MODEL_FORMAT_VERSION;
    j["renderObject"] = {
        {"gltfPath", info.gltfPath},
        {"name", info.name},
        {"id", info.id}
    };

    try {
        std::ofstream o(outputPath);
        o << std::setw(4) << j << std::endl;
        return true;
    } catch (const std::exception& e) {
        fmt::print("Failed to write willmodel file: {}\n", e.what());
        return false;
    }
}

std::optional<RenderObjectInfo> Serializer::loadWillModel(const std::filesystem::path& willmodelPath)
{
    try {
        std::ifstream i(willmodelPath);
        if (!i.is_open()) {
            fmt::print("Failed to open willmodel file: {}\n", willmodelPath.string());
            return std::nullopt;
        }

        nlohmann::json j;
        i >> j;

        if (!j.contains("version") || j["version"] != 1) {
            fmt::print("Invalid or unsupported willmodel version in: {}\n", willmodelPath.string());
            return std::nullopt;
        }

        if (!j.contains("renderObject")) {
            fmt::print("No renderObject data found in: {}\n", willmodelPath.string());
            return std::nullopt;
        }

        const auto& renderObject = j["renderObject"];

        RenderObjectInfo info;
        info.willmodelPath = willmodelPath;
        info.gltfPath = renderObject["gltfPath"].get<std::string>();
        info.name = renderObject["name"].get<std::string>();
        info.id = renderObject["id"].get<uint32_t>();

        uint32_t computedId = computePathHash(info.gltfPath);
        if (computedId != info.id) {
            fmt::print("Warning: ID mismatch in {}, expected: {}, found: {} (not necessarily a problem).\n", willmodelPath.string(), computedId,
                       info.id);
            info.id = computedId;
        }

        return info;
    } catch (const std::exception& e) {
        fmt::print("Error loading willmodel file {}: {}\n", willmodelPath.string(), e.what());
        return std::nullopt;
    }
}

bool Serializer::generateWillTexture(const std::filesystem::path& texturePath, const std::filesystem::path& outputPath)
{
    TextureInfo info;
    info.texturePath = texturePath.string();
    info.name = outputPath.stem().string();
    info.id = computePathHash(texturePath.string());

    nlohmann::json j;
    j["version"] = WILL_TEXTURE_FORMAT_VERSION;
    j["texture"] = {
        {"gltfPath", info.texturePath},
        {"name", info.name},
        {"id", info.id},
        {"textureProperties", info.textureProperties},
    };

    try {
        std::ofstream o(outputPath);
        o << std::setw(4) << j << std::endl;
        return true;
    } catch (const std::exception& e) {
        fmt::print("Failed to write willtexture file: {}\n", e.what());
        return false;
    }
}

std::optional<TextureInfo> Serializer::loadWillTexture(const std::filesystem::path& willtexturePath)
{
    try {
        std::ifstream i(willtexturePath);
        if (!i.is_open()) {
            fmt::print("Failed to open willtexture file: {}\n", willtexturePath.string());
            return std::nullopt;
        }

        nlohmann::json j;
        i >> j;

        if (!j.contains("version") || j["version"] != 1) {
            fmt::print("Invalid or unsupported willtexture version in: {}\n", willtexturePath.string());
            return std::nullopt;
        }

        if (!j.contains("texture")) {
            fmt::print("No texture data found in: {}\n", willtexturePath.string());
            return std::nullopt;
        }

        const auto& texture = j["texture"];

        TextureInfo info;
        info.willtexturePath = willtexturePath;
        info.texturePath = texture.value<std::string>("gltfPath", "");
        info.name = texture.value<std::string>("name", "");
        info.id = texture.value<uint32_t>("id", 0);;
        info.textureProperties = texture.value<TextureProperties>("textureProperties", {});

        uint32_t computedId = computePathHash(info.texturePath);
        if (computedId != info.id) {
            fmt::print("Warning: ID mismatch in {}, expected: {}, found: {} (not necessarily a problem).\n", willtexturePath.string(), computedId,
                       info.id);
            info.id = computedId;
        }

        return info;
    } catch (const std::exception& e) {
        fmt::print("Error loading willtexture file {}: {}\n", willtexturePath.string(), e.what());
        return std::nullopt;
    }
}

bool Serializer::serializeEngineSettings(Engine* engine, EngineSettingsTypeFlag engineSettings)
{
#if WILL_ENGINE_RELEASE
    fmt::print("Warning: Attempted to serialize engine settings in release mode, this is not allowed.");
    return false;
#endif
    if (engine == nullptr) {
        fmt::print("Warning: engine is null\n");
        return false;
    }

    const std::filesystem::path filepath = {"assets/settings.willengine"};

    ordered_json rootJ;
    bool fileExists = std::filesystem::exists(filepath);

    if (fileExists) {
        try {
            std::ifstream inFile(filepath);
            if (inFile.is_open()) {
                inFile >> rootJ;
                inFile.close();
            }
            else {
                fmt::print("Warning: Could not open existing settings file. Creating new file.\n");
                fileExists = false;
            }
        } catch (const std::exception& e) {
            fmt::print("Warning: Error parsing existing settings file: {}. Creating new file.\n", e.what());
            fileExists = false;
            rootJ = ordered_json();
        }
    }

    rootJ["version"] = EngineVersion::current();

#if WILL_ENGINE_DEBUG
    if (hasFlag(engineSettings, EngineSettingsTypeFlag::EDITOR_SETTINGS)) {
        ordered_json editorSettings;
        EditorSettings _editorSettings = engine->getEditorSettings();
        editorSettings["saveOnExit"] = _editorSettings.saveOnExit;

        rootJ["editorSettings"] = editorSettings;
    }
#endif

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::ENGINE_SETTINGS)) {
        ordered_json _engineSettings;
        EngineSettings mainEngineSettings = engine->getEngineSettings();
        _engineSettings["defaultMapToLoad"] = relative(mainEngineSettings.defaultMapToLoad);

        rootJ["engineSettings"] = _engineSettings;
    }

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::CAMERA_SETTINGS)) {
        ordered_json cameraSettings;
        const auto& camera = engine->getCamera();
        if (camera) {
            glm::vec3 position = camera->getTransform().getPosition();
            cameraSettings["position"] = {position.x, position.y, position.z};

            glm::quat rotation = camera->getTransform().getRotation();
            cameraSettings["rotation"] = {rotation.x, rotation.y, rotation.z, rotation.w};

            cameraSettings["fov"] = camera->getFov();
            cameraSettings["aspectRatio"] = camera->getAspectRatio();
            cameraSettings["nearPlane"] = camera->getNearPlane();
            cameraSettings["farPlane"] = camera->getFarPlane();
        }

        rootJ["cameraSettings"] = cameraSettings;
    }

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::LIGHT_SETTINGS)) {
        ordered_json lightSettings;
        DirectionalLight mainLight = engine->getMainLight();
        auto direction = normalize(mainLight.direction);
        auto color = mainLight.color;
        auto intensity = mainLight.intensity;
        lightSettings["mainLight"]["direction"] = {direction.x, direction.y, direction.z};
        lightSettings["mainLight"]["color"] = {color.x, color.y, color.z};
        lightSettings["mainLight"]["intensity"] = intensity;

        rootJ["lightSettings"] = lightSettings;
    }

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::ENVIRONMENT_SETTINGS)) {
        ordered_json environmentSettings;

        environmentSettings["environmentMapIndex"] = engine->getCurrentEnvironmentMapIndex();

        rootJ["environmentSettings"] = environmentSettings;
    }

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::AMBIENT_OCCLUSION_SETTINGS)) {
        ordered_json aoSettings;

        renderer::GTAOSettings settings = engine->getAoSettings();
        aoSettings["enabled"] = settings.bEnabled;

        aoSettings["properties"]["effect_radius"] = settings.pushConstants.effectRadius;
        aoSettings["properties"]["effect_falloff_range"] = settings.pushConstants.effectFalloffRange;
        aoSettings["properties"]["denoise_blur_beta"] = settings.pushConstants.denoiseBlurBeta;
        aoSettings["properties"]["radius_multiplier"] = settings.pushConstants.radiusMultiplier;
        aoSettings["properties"]["sample_distribution_power"] = settings.pushConstants.sampleDistributionPower;
        aoSettings["properties"]["thin_occluder_compensation"] = settings.pushConstants.thinOccluderCompensation;
        aoSettings["properties"]["final_value_power"] = settings.pushConstants.finalValuePower;
        aoSettings["properties"]["depth_mip_sampling_offset"] = settings.pushConstants.depthMipSamplingOffset;
        aoSettings["properties"]["slice_count"] = settings.pushConstants.sliceCount;
        aoSettings["properties"]["steps_per_slice_count"] = settings.pushConstants.stepsPerSliceCount;

        rootJ["aoSettings"] = aoSettings;
    }

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::SCREEN_SPACE_SHADOWS_SETTINGS)) {
        ordered_json sssSettings;

        renderer::ContactShadowSettings settings = engine->getSssSettings();
        sssSettings["enabled"] = settings.bEnabled;

        sssSettings["properties"]["surfaceThickness"] = settings.pushConstants.surfaceThickness;
        sssSettings["properties"]["bilinearThreshold"] = settings.pushConstants.bilinearThreshold;
        sssSettings["properties"]["shadowContrast"] = settings.pushConstants.shadowContrast;
        sssSettings["properties"]["ignoreEdgePixels"] = settings.pushConstants.bIgnoreEdgePixels;
        sssSettings["properties"]["usePrecisionOffset"] = settings.pushConstants.bUsePrecisionOffset;
        sssSettings["properties"]["bilinearSamplingOffsetMode"] = settings.pushConstants.bBilinearSamplingOffsetMode;

        rootJ["sssSettings"] = sssSettings;
    }

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::CASCADED_SHADOW_MAP_SETTINGS)) {
        ordered_json csmSettings;

        renderer::CascadedShadowMapSettings settings = engine->getCsmSettings();
        csmSettings["enabled"] = settings.bEnabled;

        csmSettings["properties"]["pcfLevel"] = settings.pcfLevel;
        csmSettings["properties"]["splitLambda"] = settings.splitLambda;
        csmSettings["properties"]["splitOverlap"] = settings.splitOverlap;
        csmSettings["properties"]["cascadeNearPlane"] = settings.cascadeNearPlane;
        csmSettings["properties"]["cascadeFarPlane"] = settings.cascadeFarPlane;

        ordered_json cascadeBiasArray = ordered_json::array();
        for (const auto& bias : settings.cascadeBias) {
            ordered_json biasObj;
            biasObj["constant"] = bias.constant;
            biasObj["slope"] = bias.slope;
            cascadeBiasArray.push_back(biasObj);
        }
        csmSettings["properties"]["cascadeBias"] = cascadeBiasArray;

        ordered_json cascadeExtentsArray = ordered_json::array();
        for (const auto& extent : settings.cascadeExtents) {
            ordered_json extentObj;
            extentObj["width"] = extent.width;
            extentObj["height"] = extent.height;
            extentObj["depth"] = extent.depth;
            cascadeExtentsArray.push_back(extentObj);
        }
        csmSettings["properties"]["cascadeExtents"] = cascadeExtentsArray;

        csmSettings["properties"]["useManualSplit"] = settings.useManualSplit;
        csmSettings["properties"]["manualCascadeSplits"] = settings.manualCascadeSplits;

        rootJ["csmSettings"] = csmSettings;
    }

    if (hasFlag(engineSettings, EngineSettingsTypeFlag::TEMPORAL_ANTIALIASING_SETTINGS)) {
        ordered_json taaSettings;

        auto settings = engine->getTaaSettings();
        taaSettings["enabled"] = settings.bEnabled;

        taaSettings["properties"]["blendValue"] = settings.blendValue;

        rootJ["taaSettings"] = taaSettings;
    }


    std::ofstream outFile(filepath);
    if (!outFile.is_open()) {
        fmt::print("Error: Could not open settings file for writing\n");
        return false;
    }

    outFile << rootJ.dump(4);
    outFile.close();
    return true;
}

bool Serializer::deserializeEngineSettings(Engine* engine, EngineSettingsTypeFlag engineSettings)
{
    if (engine == nullptr) {
        fmt::print("Warning: engine is null\n");
        return false;
    }

    const std::filesystem::path filepath = {"assets/settings.willengine"};

    if (!exists(filepath)) {
        fmt::print("Warning: settings file does not exist at {}\n", filepath.string());
        return false;
    }

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            fmt::print("Error: Could not open settings file at {}\n", filepath.string());
            return false;
        }

        ordered_json rootJ = ordered_json::parse(file);

#if WILL_ENGINE_DEBUG
        if (hasFlag(engineSettings, EngineSettingsTypeFlag::EDITOR_SETTINGS)) {
            ordered_json editorSettings = rootJ["editorSettings"];
            EditorSettings _editorSettings = engine->getEditorSettings();

            if (editorSettings.contains("saveOnExit")) {
                _editorSettings.saveOnExit = editorSettings["saveOnExit"].get<bool>();
            }

            engine->setEditorSettings(_editorSettings);
        }
#endif

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::ENGINE_SETTINGS)) {
            ordered_json _engineSettings = rootJ["engineSettings"];
            EngineSettings mainEngineSettings = engine->getEngineSettings();
            if (_engineSettings.contains("defaultMapToLoad")) {
                mainEngineSettings.defaultMapToLoad = _engineSettings["defaultMapToLoad"].get<std::string>();
            }

            engine->setEngineSettings(mainEngineSettings);
        }

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::CAMERA_SETTINGS)) {
            if (rootJ.contains("cameraSettings")) {
                const auto& cameraJ = rootJ["cameraSettings"];

                if (Camera* camera = engine->getCamera()) {
                    glm::vec3 position(0.0f);
                    if (cameraJ.contains("position") && cameraJ["position"].is_array() && cameraJ["position"].size() == 3) {
                        position.x = cameraJ["position"][0];
                        position.y = cameraJ["position"][1];
                        position.z = cameraJ["position"][2];
                    }

                    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
                    if (cameraJ.contains("rotation") && cameraJ["rotation"].is_array() && cameraJ["rotation"].size() == 4) {
                        rotation.x = cameraJ["rotation"][0];
                        rotation.y = cameraJ["rotation"][1];
                        rotation.z = cameraJ["rotation"][2];
                        rotation.w = cameraJ["rotation"][3];
                    }

                    camera->setCameraTransform(position, rotation);

                    if (cameraJ.contains("fov") && cameraJ["fov"].is_number() &&
                        cameraJ.contains("aspectRatio") && cameraJ["aspectRatio"].is_number() &&
                        cameraJ.contains("nearPlane") && cameraJ["nearPlane"].is_number() &&
                        cameraJ.contains("farPlane") && cameraJ["farPlane"].is_number()) {
                        float fov = cameraJ["fov"];
                        float aspectRatio = cameraJ["aspectRatio"];
                        float nearPlane = cameraJ["nearPlane"];
                        float farPlane = cameraJ["farPlane"];

                        camera->setProjectionProperties(fov, aspectRatio, nearPlane, farPlane);
                    }
                }
            }
        }

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::LIGHT_SETTINGS)) {
            if (rootJ.contains("lightSettings")) {
                if (rootJ["lightSettings"].contains("mainLight")) {
                    auto lightJ = rootJ["lightSettings"]["mainLight"];
                    glm::vec3 direction{};
                    glm::vec3 color{};
                    float intensity{};


                    if (lightJ.contains("direction") && lightJ["direction"].is_array() && lightJ["direction"].size() == 3) {
                        direction.x = lightJ["direction"][0];
                        direction.y = lightJ["direction"][1];
                        direction.z = lightJ["direction"][2];
                    }
                    else {
                        direction = normalize(engine->getMainLight().direction);
                    }

                    if (lightJ.contains("color") && lightJ["color"].is_array() && lightJ["color"].size() == 3) {
                        color.x = lightJ["color"][0];
                        color.y = lightJ["color"][1];
                        color.z = lightJ["color"][2];
                    }
                    else {
                        color = engine->getMainLight().color;
                    }

                    if (lightJ.contains("intensity") && !lightJ["intensity"].is_array()) {
                        intensity = lightJ["intensity"].get<float>();
                    }
                    else {
                        intensity = engine->getMainLight().intensity;
                    }

                    DirectionalLight light{direction, intensity, color};
                    engine->setMainLight(light);
                }
            }
        }

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::ENVIRONMENT_SETTINGS)) {
            if (rootJ.contains("environmentSettings")) {
                if (rootJ["environmentSettings"].contains("environmentMapIndex")) {
                    engine->setCurrentEnvironmentMapIndex(rootJ["environmentSettings"]["environmentMapIndex"]);
                }
            }
        }

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::AMBIENT_OCCLUSION_SETTINGS)) {
            if (rootJ.contains("aoSettings")) {
                ordered_json aoSettings = rootJ["aoSettings"];
                renderer::GTAOSettings settings = engine->getAoSettings();

                if (aoSettings.contains("enabled")) {
                    settings.bEnabled = aoSettings["enabled"].get<bool>();
                }

                if (aoSettings.contains("properties")) {
                    ordered_json aoProperties = aoSettings["properties"];

                    if (aoProperties.contains("effect_radius")) {
                        settings.pushConstants.effectRadius = aoProperties["effect_radius"].get<float>();
                    }

                    if (aoProperties.contains("effect_falloff_range")) {
                        settings.pushConstants.effectFalloffRange = aoProperties["effect_falloff_range"].get<float>();
                    }

                    if (aoProperties.contains("denoise_blur_beta")) {
                        settings.pushConstants.denoiseBlurBeta = aoProperties["denoise_blur_beta"].get<float>();
                    }

                    if (aoProperties.contains("radius_multiplier")) {
                        settings.pushConstants.radiusMultiplier = aoProperties["radius_multiplier"].get<float>();
                    }

                    if (aoProperties.contains("sample_distribution_power")) {
                        settings.pushConstants.sampleDistributionPower = aoProperties["sample_distribution_power"].get<float>();
                    }

                    if (aoProperties.contains("thin_occluder_compensation")) {
                        settings.pushConstants.thinOccluderCompensation = aoProperties["thin_occluder_compensation"].get<float>();
                    }

                    if (aoProperties.contains("final_value_power")) {
                        settings.pushConstants.finalValuePower = aoProperties["final_value_power"].get<float>();
                    }

                    if (aoProperties.contains("depth_mip_sampling_offset")) {
                        settings.pushConstants.depthMipSamplingOffset = aoProperties["depth_mip_sampling_offset"].get<float>();
                    }

                    if (aoProperties.contains("slice_count")) {
                        settings.pushConstants.sliceCount = aoProperties["slice_count"].get<float>();
                    }

                    if (aoProperties.contains("steps_per_slice_count")) {
                        settings.pushConstants.stepsPerSliceCount = aoProperties["steps_per_slice_count"].get<float>();
                    }
                }


                engine->setAoSettings(settings);
            }
        }

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::SCREEN_SPACE_SHADOWS_SETTINGS)) {
            if (rootJ.contains("sssSettings")) {
                ordered_json sssSettings = rootJ["sssSettings"];
                renderer::ContactShadowSettings settings = engine->getSssSettings();

                if (sssSettings.contains("enabled")) {
                    settings.bEnabled = sssSettings["enabled"].get<bool>();
                }

                if (sssSettings.contains("properties")) {
                    ordered_json properties = sssSettings["properties"];

                    if (properties.contains("surfaceThickness")) {
                        settings.pushConstants.surfaceThickness = properties["surfaceThickness"].get<float>();
                    }

                    if (properties.contains("bilinearThreshold")) {
                        settings.pushConstants.bilinearThreshold = properties["bilinearThreshold"].get<float>();
                    }

                    if (properties.contains("shadowContrast")) {
                        settings.pushConstants.shadowContrast = properties["shadowContrast"].get<float>();
                    }

                    if (properties.contains("ignoreEdgePixels")) {
                        settings.pushConstants.bIgnoreEdgePixels = properties["ignoreEdgePixels"].get<int32_t>();
                    }

                    if (properties.contains("usePrecisionOffset")) {
                        settings.pushConstants.bUsePrecisionOffset = properties["usePrecisionOffset"].get<int32_t>();
                    }

                    if (properties.contains("bilinearSamplingOffsetMode")) {
                        settings.pushConstants.bBilinearSamplingOffsetMode = properties["bilinearSamplingOffsetMode"].get<int32_t>();
                    }
                }


                engine->setSssSettings(settings);
            }
        }

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::CASCADED_SHADOW_MAP_SETTINGS)) {
            if (rootJ.contains("csmSettings")) {
                ordered_json csmSettings = rootJ["csmSettings"];
                renderer::CascadedShadowMapSettings settings = engine->getCsmSettings();

                if (csmSettings.contains("enabled")) {
                    settings.bEnabled = csmSettings["enabled"].get<bool>();
                }

                if (csmSettings.contains("properties")) {
                    auto properties = csmSettings["properties"];

                    if (properties.contains("pcfLevel")) {
                        settings.pcfLevel = properties["pcfLevel"].get<int32_t>();
                    }

                    if (properties.contains("splitLambda")) {
                        settings.splitLambda = properties["splitLambda"].get<float>();
                    }

                    if (properties.contains("splitOverlap")) {
                        settings.splitOverlap = properties["splitOverlap"].get<float>();
                    }

                    if (properties.contains("cascadeNearPlane")) {
                        settings.cascadeNearPlane = properties["cascadeNearPlane"].get<float>();
                    }

                    if (properties.contains("cascadeFarPlane")) {
                        settings.cascadeFarPlane = properties["cascadeFarPlane"].get<float>();
                    }

                    if (properties.contains("cascadeBias") && properties["cascadeBias"].is_array()) {
                        auto biasArray = properties["cascadeBias"];
                        for (size_t i = 0; i < std::min(biasArray.size(), settings.cascadeBias.size()); i++) {
                            auto biasObj = biasArray[i];
                            if (biasObj.contains("constant")) {
                                settings.cascadeBias[i].constant = biasObj["constant"].get<float>();
                            }

                            if (biasObj.contains("slope")) {
                                settings.cascadeBias[i].slope = biasObj["slope"].get<float>();
                            }
                        }
                    }

                    if (properties.contains("cascadeExtents") && properties["cascadeExtents"].is_array()) {
                        auto extentsArray = properties["cascadeExtents"];
                        for (size_t i = 0; i < std::min(extentsArray.size(), settings.cascadeExtents.size()); i++) {
                            auto extentObj = extentsArray[i];
                            if (extentObj.contains("width")) {
                                settings.cascadeExtents[i].width = extentObj["width"].get<uint32_t>();
                            }
                            if (extentObj.contains("height")) {
                                settings.cascadeExtents[i].height = extentObj["height"].get<uint32_t>();
                            }
                            if (extentObj.contains("depth")) {
                                settings.cascadeExtents[i].depth = extentObj["depth"].get<uint32_t>();
                            }
                        }
                    }

                    if (properties.contains("useManualSplit")) {
                        settings.useManualSplit = properties["useManualSplit"].get<bool>();
                    }

                    if (properties.contains("manualCascadeSplits") && properties["manualCascadeSplits"].is_array()) {
                        auto splitsArray = properties["manualCascadeSplits"];
                        for (size_t i = 0; i < std::min(splitsArray.size(), settings.manualCascadeSplits.size()); i++) {
                            settings.manualCascadeSplits[i] = splitsArray[i].get<float>();
                        }
                    }
                }

                engine->setCsmSettings(settings);
            }
        }

        if (hasFlag(engineSettings, EngineSettingsTypeFlag::TEMPORAL_ANTIALIASING_SETTINGS)) {
            if (rootJ.contains("taaSettings")) {
                ordered_json taaSettings = rootJ["taaSettings"];
                temporal_antialiasing_pipeline::TemporalAntialiasingSettings settings = engine->getTaaSettings();

                if (taaSettings.contains("enabled")) {
                    settings.bEnabled = taaSettings["enabled"].get<bool>();
                }

                if (taaSettings.contains("properties")) {
                    auto properties = taaSettings["properties"];

                    if (properties.contains("blendValue")) {
                        settings.blendValue = properties["blendValue"].get<float>();
                    }
                }

                engine->setTaaSettings(settings);
            }
        }

        return true;
    } catch
    (const std::exception&
        e
    ) {
        fmt::print("Error deserializing engine settings: {}\n", e.what());
        return false;
    }
}
} // will_engine
