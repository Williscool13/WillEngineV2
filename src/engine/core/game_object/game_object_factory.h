//
// Created by William on 2025-05-09.
//

#ifndef GAME_OBJECT_FACTORY_H
#define GAME_OBJECT_FACTORY_H

#include "game_object.h"
#include "engine/core/factory/object_factory.h"


namespace will_engine::game
{
class GameObjectFactory : public ObjectFactory<GameObject>
{
public:
    static GameObjectFactory& getInstance()
    {
        static GameObjectFactory instance;
        return instance;
    }

    void registerGameObjects()
    {
        registerType<GameObject>(GameObject::CAN_BE_CREATED_MANUALLY);
    }
};
}


#endif //GAME_OBJECT_FACTORY_H
