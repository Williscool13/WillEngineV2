//
// Created by William on 2025-03-08.
//

#include "map.h"

#include <fmt/format.h>

#include "serializer.h"
#include "src/core/engine.h"
#include "src/util/file.h"

will_engine::Map::Map(const std::filesystem::path& mapSource, ResourceManager& resourceManager) : mapSource(mapSource), resourceManager(resourceManager)
{
    if (!exists(mapSource)) {
        fmt::print("Map source file not found, generating an empty map\n");
        // todo: generate empty map
        mapName = mapSource.filename().stem().string();
        return;
    }

    loadMap();

    isLoaded = true;

    if (Engine* engine = Engine::get()) {
        engine->addToBeginQueue(this);
    }
}

will_engine::Map::~Map()
{
    if (!children.empty()) {
        fmt::print("Error: GameObject destroyed with children, potentially not destroyed with ::destroy. This will result in orphaned children and null references");
    }
}

void will_engine::Map::destroy()
{
    for (int32_t i = static_cast<int32_t>(children.size()) - 1; i >= 0; i--) {
        recursiveDestroy(children[i]);
    }

    children.clear();

    if (Engine* engine = Engine::get()) {
        engine->addToDeletionQueue(this);
    }
}

bool will_engine::Map::loadMap()
{
    std::ifstream file(mapSource);
    if (!file.is_open()) {
        fmt::print("Failed to open scene file: %s\n", mapSource.string());
        return false;
    }

    ordered_json rootJ;
    try {
        file >> rootJ;
    } catch (const std::exception& e) {
        fmt::print("Failed to parse scene file: {}\n", e.what());
        return false;
    }

    if (!rootJ.contains("version")) {
        fmt::print("Scene file missing version information\n");
        return false;
    }

    const auto fileVersion = rootJ["version"].get<EngineVersion>();
    if (fileVersion > EngineVersion::current()) {
        fmt::print("Scene file version {} is newer than current engine version {}\n", fileVersion.toString(), EngineVersion::current().toString());
        return false;
    }

    if (rootJ.contains("mapName")) {
        mapName = rootJ["mapName"].get<std::string>();
    }

    if (mapName.empty()) {
        mapName = mapSource.filename().stem().string();
    }

    if (rootJ.contains("mapId")) {
        mapId = rootJ["mapId"].get<uint32_t>();
    }
    else {
        mapId = file::computePathHash(mapSource);
    }

    Serializer::deserializeMap(this, rootJ);

    return true;
}

bool will_engine::Map::saveMap(const std::filesystem::path& newSavePath)
{
    if (!newSavePath.empty() && mapSource != newSavePath) {
        mapSource = newSavePath;
    }

    ordered_json rootJ;

    return Serializer::serializeMap(this, rootJ, mapSource);
}

void will_engine::Map::addGameObject(IHierarchical* newChild)
{
    addChild(newChild);
}

bool will_engine::Map::canAddComponent(std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            fmt::print("Attempted to add a component of the same type to a gameobject. This is not supported at this time.\n");
            return false;
        }
    }

    return true;
}

will_engine::components::Component* will_engine::Map::addComponent(std::unique_ptr<components::Component> component)
{
    if (!component) { return nullptr; }

    for (const auto& _component : components) {
        if (component->getComponentType() == _component->getComponentType()) {
            fmt::print("Attempted to add a component of the same type to a gameobject. This is not supported at this time.\n");
            return nullptr;
        }
    }
    component->setOwner(this);
    components.push_back(std::move(component));

    terrainComponent = getComponent<components::TerrainComponent>();

    if (bHasBegunPlay) {
        components.back()->beginPlay();
    }

    return components.back().get();
}

void will_engine::Map::destroyComponent(components::Component* component)
{
    if (!component) { return; }

    const auto it = std::ranges::find_if(components, [component](const std::unique_ptr<components::Component>& comp) {
        return comp.get() == component;
    });

    if (it != components.end()) {
        component->beginDestroy();
        components.erase(it);
    }
    else {
        fmt::print("Attempted to remove a component that does not belong to this gameobject.\n");
    }

    terrainComponent = nullptr;
    terrainComponent = getComponent<components::TerrainComponent>();
}

void will_engine::Map::beginPlay()
{
    for (const auto& component : components) {
        component->beginPlay();
    }
    bHasBegunPlay = true;
}

void will_engine::Map::update(const float deltaTime)
{
    if (!bHasBegunPlay) { return; }

    for (const auto& component : components) {
        if (component->isEnabled()) {
            component->update(deltaTime);
        }
    }

    for (IHierarchical* child : getChildren()) {
        recursiveUpdate(child, deltaTime);
    }
}

void will_engine::Map::beginDestroy()
{
    for (const auto& component : components) {
        component->beginDestroy();
    }
}

void will_engine::Map::recursiveUpdate(IHierarchical* object, const float deltaTime)
{
    object->update(deltaTime);

    for (IHierarchical* child : object->getChildren()) {
        recursiveUpdate(child, deltaTime);
    }
}

void will_engine::Map::recursiveDestroy(IHierarchical* object)
{
    for (int32_t i = static_cast<int32_t>(object->getChildren().size()) - 1; i >= 0; i--) {
        recursiveDestroy(object->getChildren()[i]);
    }

    object->destroy();
}

bool will_engine::Map::addChild(IHierarchical* child)
{
    if (child == this) { return false; }
    if (child->getParent() != nullptr) { return false; }
    if (std::ranges::find(children, child) != children.end()) { return false; }

    child->setParent(this);
    children.push_back(child);
    return true;
}

bool will_engine::Map::removeChild(IHierarchical* child)
{
    if (child->getParent() != this) { return false; }

    const auto it = std::ranges::find(children, child);
    if (it == children.end()) { return false; }

    children.erase(it);
    child->setParent(nullptr);
    return true;
}

bool will_engine::Map::removeParent()
{
    fmt::print("Warning: You are not allowed to remove parent of a willmap");
    return false;
}

void will_engine::Map::reparent(IHierarchical* newParent)
{
    fmt::print("Warning: You re not allowed to reparent a willmap");
}

void will_engine::Map::dirty()
{
    cachedModelMatrix = transform.toModelMatrix();

    for (const auto& child : children) {
        child->dirty();
    }
}

void will_engine::Map::setParent(IHierarchical* newParent)
{
    fmt::print("Warning: You re not allowed to set the parent a willmap");
}

will_engine::IHierarchical* will_engine::Map::getParent() const
{
    return nullptr;
}

const std::vector<will_engine::IHierarchical*>& will_engine::Map::getChildren() const
{
    return children;
}

std::vector<will_engine::IHierarchical*>& will_engine::Map::getChildren()
{
    return children;
}

void will_engine::Map::setName(std::string newName)
{
    if (newName == "") { return; }
    mapName = std::move(newName);
}

void will_engine::Map::setLocalPosition(const glm::vec3 localPosition)
{
    transform.setPosition(localPosition);
    dirty();
}

void will_engine::Map::setLocalRotation(const glm::quat localRotation)
{
    transform.setRotation(localRotation);
    dirty();
}

void will_engine::Map::setLocalScale(const glm::vec3 localScale)
{
    transform.setScale(localScale);
    dirty();
}

void will_engine::Map::setLocalScale(const float localScale)
{
    transform.setScale(glm::vec3(localScale));
    dirty();
}

void will_engine::Map::setLocalTransform(const Transform& newLocalTransform)
{
    transform = newLocalTransform;
    dirty();
}

void will_engine::Map::translate(const glm::vec3 translation)
{
    transform.translate(translation);
    dirty();
}

void will_engine::Map::rotate(const glm::quat rotation)
{
    transform.rotate(rotation);
    dirty();
}

void will_engine::Map::rotateAxis(const float angle, const glm::vec3& axis)
{
    transform.rotateAxis(angle, axis);
    dirty();
}
