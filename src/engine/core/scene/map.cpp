//
// Created by William on 2025-03-08.
//

#include "map.h"

#include <fmt/format.h>

#include "serializer.h"
#include "engine/core/engine.h"
#include "engine/util/file.h"

will_engine::Map::Map(const std::filesystem::path& mapSource, renderer::ResourceManager& resourceManager) : mapSource(mapSource),
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

will_engine::Map::~Map()
{
    if (!children.empty()) {
        fmt::print(
            "Error: GameObject destroyed with children, potentially not destroyed with ::destroy. This will result in orphaned children and null references");
    }
}

void will_engine::Map::destroy()
{
    recursivelyDestroy();

    if (Engine* engine = Engine::get()) {
        engine->addToMapDeletionQueue(this);
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

bool will_engine::Map::saveMap()
{
#if WILL_ENGINE_RELEASE
    fmt::print("Warning: Attempted to save game in release, this should generally not be allowed");
    return false;
#endif
    ordered_json rootJ;

    return Serializer::serializeMap(this, rootJ, mapSource);
}

bool will_engine::Map::saveMap(const std::filesystem::path& newSavePath)
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

will_engine::components::Component* will_engine::Map::getComponentByTypeName(std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            return _component.get();
        }
    }

    return nullptr;
}

std::vector<will_engine::components::Component*> will_engine::Map::getComponentsByTypeName(std::string_view componentType)
{
    std::vector<components::Component*> outComponents;
    outComponents.reserve(components.size());
    for (const auto& component : components) {
        if (component->getComponentType() == componentType) {
            outComponents.push_back(component.get());
        }
    }

    return outComponents;
}

bool will_engine::Map::hasComponent(const std::string_view componentType)
{
    for (const auto& _component : components) {
        if (componentType == _component->getComponentType()) {
            return true;
        }
    }

    return false;
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

    recursivelyUpdate(deltaTime);
}

void will_engine::Map::beginDestructor()
{
    for (const auto& component : components) {
        component->beginDestroy();
    }
}

will_engine::IHierarchical* will_engine::Map::addChild(std::unique_ptr<IHierarchical> child, bool retainTransform)
{
    ITransformable* transformable = dynamic_cast<ITransformable*>(child.get());
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

bool will_engine::Map::moveChild(IHierarchical* child, IHierarchical* newParent, const bool retainTransform)
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

void will_engine::Map::moveChildToIndex(int32_t fromIndex, int32_t targetIndex)
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

void will_engine::Map::recursivelyUpdate(const float deltaTime)
{
    for (auto& child : children) {
        if (child == nullptr) { continue; }
        child->update(deltaTime);
    }
}

void will_engine::Map::recursivelyDestroy()
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

bool will_engine::Map::deleteChild(IHierarchical* child)
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

void will_engine::Map::dirty()
{
    cachedModelMatrix = transform.toModelMatrix();

    for (const auto& child : children) {
        if (child == nullptr) { continue; }
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

std::vector<will_engine::IHierarchical*>& will_engine::Map::getChildren()
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
