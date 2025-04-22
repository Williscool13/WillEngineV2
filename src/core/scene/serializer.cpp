//
// Created by William on 2025-02-06.
//

#include "serializer.h"

#include "src/core/engine.h"

namespace will_engine
{
void Serializer::serializeGameObject(ordered_json& j, IHierarchical* obj)

{
    if (const auto gameObject = dynamic_cast<GameObject*>(obj)) {
        j["id"] = gameObject->getId();
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
                componentsJson[component->getComponentType()] = componentData;
                componentsJson[component->getComponentType()]["componentName"] = component->getComponentName();
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

GameObject* Serializer::deserializeGameObject(const ordered_json& j, IHierarchical* parent)
{
    const auto gameObject = new GameObject();

    if (j.contains("id")) {
        gameObject->setId(j["id"].get<uint32_t>());
    }

    if (j.contains("name")) {
        gameObject->setName(j["name"].get<std::string>());
    }

    if (j.contains("transform")) {
        if (const auto transformable = dynamic_cast<ITransformable*>(gameObject)) {
            transformable->setLocalTransform(j["transform"].get<Transform>());
        }
    }

    if (j.contains("components")) {
        if (const auto componentContainer = dynamic_cast<IComponentContainer*>(gameObject)) {
            const auto& components = j["components"];
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

    if (j.contains("children")) {
        const auto& children = j["children"];
        for (const auto& childJson : children) {
            if (IHierarchical* child = deserializeGameObject(childJson, gameObject)) {
                gameObject->addChild(child);
            }
        }
    }

    if (parent != nullptr) {
        parent->addChild(gameObject);
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
                componentsJson[component->getComponentType()]["componentName"] = component->getComponentName();
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
            IHierarchical* childObject = deserializeGameObject(child, root);
            root->addChild(childObject);
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

bool Serializer::serializeEngineSettings(Engine* engine)
{
    ordered_json rootJ;
    if (engine == nullptr) {
        fmt::print("Warning: engine is null\n");
        return false;
    }

    rootJ["version"] = EngineVersion::current();


    ordered_json cameraProperties;
    const auto& camera = engine->getCamera();
    if (camera) {
        glm::vec3 position = camera->getTransform().getPosition();
        cameraProperties["position"] = {position.x, position.y, position.z};

        glm::quat rotation = camera->getTransform().getRotation();
        cameraProperties["rotation"] = {rotation.x, rotation.y, rotation.z, rotation.w};

        cameraProperties["fov"] = camera->getFov();
        cameraProperties["aspectRatio"] = camera->getAspectRatio();
        cameraProperties["nearPlane"] = camera->getNearPlane();
        cameraProperties["farPlane"] = camera->getFarPlane();
    }

    DirectionalLight mainLight = engine->getMainLight();
    auto direction = normalize(mainLight.getDirection());
    auto color = mainLight.getColor();
    auto intensity = mainLight.getIntensity();
    rootJ["mainLight"]["direction"] = {direction.x, direction.y, direction.z};
    rootJ["mainLight"]["color"] = {color.x, color.y, color.z};
    rootJ["mainLight"]["intensity"] = intensity;

    rootJ["cameraProperties"] = cameraProperties;

    rootJ["environmentMapIndex"] = engine->getCurrentEnvironmentMapIndex();


    const std::filesystem::path filepath = {"assets/settings.willengine"};

    std::ofstream file(filepath);
    file << rootJ.dump(4);
    return true;
}

bool Serializer::deserializeEngineSettings(Engine* engine)
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

        if (rootJ.contains("cameraProperties")) {
            const auto& cameraJ = rootJ["cameraProperties"];
            auto camera = engine->getCamera();

            if (camera) {
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

        if (rootJ.contains("mainLight")) {
            auto lightJ = rootJ["mainLight"];
            glm::vec3 direction{};
            glm::vec3 color{};
            float intensity{};


            if (lightJ.contains("direction") && lightJ["direction"].is_array() && lightJ["direction"].size() == 3) {
                direction.x = lightJ["direction"][0];
                direction.y = lightJ["direction"][1];
                direction.z = lightJ["direction"][2];
            } else {
                direction = normalize(engine->getMainLight().getDirection());
            }

            if (lightJ.contains("color") && lightJ["color"].is_array() && lightJ["color"].size() == 3) {
                color.x = lightJ["color"][0];
                color.y = lightJ["color"][1];
                color.z = lightJ["color"][2];
            } else {
                color = engine->getMainLight().getColor();
            }

            if (lightJ.contains("intensity") && !lightJ["intensity"].is_array()) {
                intensity = lightJ["intensity"].get<float>();
            } else {
                intensity = engine->getMainLight().getIntensity();
            }


            DirectionalLight light{direction, intensity, color};
            engine->setMainLight(light);
        }

        if (rootJ.contains("environmentMapIndex")) {
            engine->setCurrentEnvironmentMapIndex(rootJ["environmentMapIndex"]);
        }


        return true;
    } catch (const std::exception& e) {
        fmt::print("Error deserializing engine settings: {}\n", e.what());
        return false;
    }
}
} // will_engine
