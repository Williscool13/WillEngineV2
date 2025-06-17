//
// Created by William on 2025-06-17.
//

#ifndef RENDER_OBJECT_FACTORY_H
#define RENDER_OBJECT_FACTORY_H
#include "engine/core/factory/object_factory.h"

namespace will_engine::renderer
{

class RenderObjectFactory : public ObjectFactory<RenderObject>
{
public:
    static RenderObjectFactory& getInstance()
    {
        static RenderObjectFactory instance;
        return instance;
    }

    void registerComponents()
    {

    }
};

}

#endif //RENDER_OBJECT_FACTORY_H
