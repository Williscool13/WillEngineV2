//
// Created by William on 2025-05-09.
//

#ifndef GAME_OBJECT_FACTORY_H
#define GAME_OBJECT_FACTORY_H
#include <string_view>

#include "game_object.h"


namespace will_engine::game_object
{
template<typename T>
concept HasGetStaticType = requires(T)
{
    { T::getStaticType() } -> std::convertible_to<std::string_view>;
};


class GameObjectFactory
{
public:
    void registerGameObjects()
    {
        registerGameObject<GameObject>(GameObject::CAN_BE_CREATED_MANUALLY);
    }

    using GameObjectCreator = std::function<std::unique_ptr<GameObject>(const std::string& name)>;

    static GameObjectFactory& getInstance()
    {
        static GameObjectFactory instance;
        return instance;
    }

    const std::unordered_map<std::string_view, GameObjectCreator>& getManuallyCreatableGameObjectCreators() { return canManuallyCreateCreators; }


    template<typename T>
    void registerGameObject(bool canManuallyCreate) requires HasGetStaticType<T>
    {
        static_assert(std::is_base_of_v<GameObject, T>, "Must inherit from GameObject");
        allCreators[T::getStaticType()] = [](const std::string& name) { return std::make_unique<T>(name); };
        if (canManuallyCreate) {
            canManuallyCreateCreators[T::getStaticType()] = [](const std::string& name) { return std::make_unique<T>(name); };
        }
    }

    std::unique_ptr<GameObject> createGameObject(const std::string_view& type, const std::string& name)
    {
        const auto it = allCreators.find(type);
        if (it != allCreators.end()) {
            return it->second(name);
        }
        return nullptr;
    }

private:
    GameObjectFactory() = default;

    std::unordered_map<std::string_view, GameObjectCreator> allCreators;

    std::unordered_map<std::string_view, GameObjectCreator> canManuallyCreateCreators;
};
}

class game_object_factory {

};



#endif //GAME_OBJECT_FACTORY_H
