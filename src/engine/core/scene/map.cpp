//
// Created by William on 2025-03-08.
//

#include "map.h"

#include <fmt/format.h>

#include "serializer.h"
#include "engine/core/engine.h"
#include "engine/util/file.h"

namespace will_engine::game
{
Map::Map(const std::filesystem::path& mapSource, renderer::ResourceManager& resourceManager) : mapSource(mapSource),
                                                                                                            resourceManager(resourceManager)
{
    if (!exists(mapSource)) {
        fmt::print("Map source file not found, generating an empty map\n");
        mapName = mapSource.filename().stem().string();
        return;
    }

    loadMap();

    isLoaded = true;

    if (Engine* engine = Engine::get()) {
        engine->addToBeginQueue(this);
    }
}

Map::~Map()
{
    if (!children.empty()) {
        fmt::print(
            "Error: GameObject destroyed with children, potentially not destroyed with ::destroy. This will result in orphaned children and null references");
    }
}

void Map::destroy()
{
    recursivelyDestroy();

    if (Engine* engine = Engine::get()) {
        engine->addToMapDeletionQueue(this);
    }
}

bool Map::loadMap()
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

bool Map::saveMap()
{
#if WILL_ENGINE_RELEASE
    fmt::print("Warning: Attempted to save game in release, this should generally not be allowed");
    return false;
#endif
    ordered_json rootJ;

    return Serializer::serializeMap(this, rootJ, mapSource);
}

bool Map::saveMap(const std::filesystem::path& newSavePath)
{
#if WILL_ENGINE_RELEASE
    fmt::print("Warning: Attempted to save game in release, this should generally not be allowed");
    return false;
#endif
    if (!newSavePath.empty() && mapSource != newSavePath) {
        mapSource = newSavePath;
    }

    ordered_json rootJ;

    return Serializer::serializeMap(this, rootJ, mapSource);
}

Component* Map::getComponentByTypeName(std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            return _component.get();
        }
    }

    return nullptr;
}

std::vector<Component*> Map::getComponentsByTypeName(std::string_view componentType)
{
    std::vector<Component*> outComponents;
    outComponents.reserve(components.size());
    for (const auto& component : components) {
        if (component->getComponentType() == componentType) {
            outComponents.push_back(component.get());
        }
    }

    return outComponents;
}

bool Map::hasComponent(const std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            return true;
        }
    }

    return false;
}

Component* Map::addComponent(std::unique_ptr<Component> component)
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

    terrainComponent = getComponent<TerrainComponent>();

    if (bHasBegunPlay) {
        components.back()->beginPlay();
    }

    return components.back().get();
}

void Map::destroyComponent(Component* component)
{
    if (!component) { return; }

    const auto it = std::ranges::find_if(components, [component](const std::unique_ptr<Component>& comp) {
        return comp.get() == component;
    });

    if (it != components.end()) {
        component->beginDestroy();
        components.erase(it);
    }
    else {
        fmt::print("Attempted to remove a component that does not belong to this gameobject.\n");
    }

    terrainComponent = getComponent<TerrainComponent>();
}

void Map::beginPlay()
{
    for (const auto& component : components) {
        component->beginPlay();
    }
    bHasBegunPlay = true;
}

void Map::update(const float deltaTime)
{
    if (!bHasBegunPlay) { return; }

    for (const auto& component : components) {
        if (component->isEnabled()) {
            component->update(deltaTime);
        }
    }

    recursivelyUpdate(deltaTime);
}

void Map::beginDestructor()
{
    for (const auto& component : components) {
        component->beginDestroy();
    }
}

IHierarchical* Map::addChild(std::unique_ptr<IHierarchical> child, bool retainTransform)
{
    const auto transformable = dynamic_cast<ITransformable*>(child.get());
    Transform prevTransform = Transform::Identity;
    if (retainTransform && transformable) {
        prevTransform = transformable->getGlobalTransform();
    }

    children.push_back(std::move(child));
    children.back()->setParent(this);
    bChildrenCacheDirty = true;

    if (transformable && retainTransform) {
        transformable->setGlobalTransform(prevTransform);
    }
    else {
        dirty();
    }

    return children.back().get();
}

bool Map::moveChild(IHierarchical* child, IHierarchical* newParent, const bool retainTransform)
{
    const auto it = std::ranges::find_if(children,
                                         [child](const std::unique_ptr<IHierarchical>& ptr) {
                                             return ptr.get() == child;
                                         });
    if (it != children.end()) {
        newParent->addChild(std::move(*it), retainTransform);
        children.erase(it);
        bChildrenCacheDirty = true;
        return true;
    }

    return false;
}

void Map::moveChildToIndex(int32_t fromIndex, int32_t targetIndex)
{
    if (fromIndex >= children.size() || targetIndex >= children.size() || fromIndex == targetIndex) {
        return;
    }

    if (fromIndex < targetIndex) {
        std::rotate(
            children.begin() + fromIndex,
            children.begin() + fromIndex + 1,
            children.begin() + targetIndex + 1
        );
    }
    else {
        std::rotate(
            children.begin() + targetIndex,
            children.begin() + fromIndex,
            children.begin() + fromIndex + 1
        );
    }

    bChildrenCacheDirty = true;
}

void Map::recursivelyUpdate(const float deltaTime)
{
    for (auto& child : children) {
        if (child == nullptr) { continue; }
        child->update(deltaTime);
    }
}

void Map::recursivelyDestroy()
{
    for (const auto& child : children) {
        if (child == nullptr) { continue; }
        child->recursivelyDestroy();
        child->setParent(nullptr);
    }

    if (Engine* engine = Engine::get()) {
        for (auto& child : children) {
            if (child == nullptr) { continue; }
            engine->addToDeletionQueue(std::move(child));
        }

        children.clear();
        bChildrenCacheDirty = true;
    }
    else {
        fmt::print("Warning: Failed to get engine to recursively delete gameobjects, may result in orphans");
    }
}

bool Map::deleteChild(IHierarchical* child)
{
    const auto it = std::ranges::find_if(children,
                                         [child](const std::unique_ptr<IHierarchical>& ptr) {
                                             return ptr.get() == child;
                                         });

    if (it != children.end()) {
        if (Engine* engine = Engine::get()) {
            it->get()->setParent(nullptr);
            engine->addToDeletionQueue(std::move(*it));
            children.erase(it);
            bChildrenCacheDirty = true;
            return true;
        }
    }

    return false;
}

void Map::dirty()
{
    cachedModelMatrix = transform.toModelMatrix();

    for (const auto& child : children) {
        if (child == nullptr) { continue; }
        child->dirty();
    }
}

void Map::setParent(IHierarchical* newParent)
{
    fmt::print("Warning: You re not allowed to set the parent a willmap");
}

IHierarchical* Map::getParent() const
{
    return nullptr;
}

std::vector<IHierarchical*>& Map::getChildren()
{
    if (bChildrenCacheDirty) {
        childrenCache.clear();
        childrenCache.reserve(children.size());
        for (const auto& child : children) {
            if (child == nullptr) { continue; }
            childrenCache.push_back(child.get());
        }
        bChildrenCacheDirty = false;
    }

    return childrenCache;
}

void Map::setName(std::string newName)
{
    if (newName == "") { return; }
    mapName = std::move(newName);
}

void Map::setLocalPosition(const glm::vec3 localPosition)
{
    transform.setPosition(localPosition);
    dirty();
}

void Map::setLocalRotation(const glm::quat localRotation)
{
    transform.setRotation(localRotation);
    dirty();
}

void Map::setLocalScale(const glm::vec3 localScale)
{
    transform.setScale(localScale);
    dirty();
}

void Map::setLocalScale(const float localScale)
{
    transform.setScale(glm::vec3(localScale));
    dirty();
}

void Map::setLocalTransform(const Transform& newLocalTransform)
{
    transform = newLocalTransform;
    dirty();
}

void Map::translate(const glm::vec3 translation)
{
    transform.translate(translation);
    dirty();
}

void Map::rotate(const glm::quat rotation)
{
    transform.rotate(rotation);
    dirty();
}

void Map::rotateAxis(const float angle, const glm::vec3& axis)
{
    transform.rotateAxis(angle, axis);
    dirty();
}

}

