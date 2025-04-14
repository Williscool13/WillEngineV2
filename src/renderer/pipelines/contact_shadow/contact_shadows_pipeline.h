//
// Created by William on 2025-04-14.
//

#ifndef CONTACT_SHADOWS_H
#define CONTACT_SHADOWS_H

class ResourceManager;

namespace will_engine::contact_shadows_pipeline
{

class ContactShadowsPipeline {
public:
    ContactShadowsPipeline(ResourceManager& resourceManager);

private:
    ResourceManager& resourceManager;
};

}

#endif //CONTACT_SHADOWS_H
