//
// Created by William on 2025-04-14.
//

#include "contact_shadows_pipeline.h"

#include "src/renderer/resource_manager.h"

namespace will_engine::contact_shadows_pipeline
{
ContactShadowsPipeline::ContactShadowsPipeline(ResourceManager& resourceManager) : resourceManager(resourceManager)
{

}

ContactShadowsPipeline::~ContactShadowsPipeline()
{
    resourceManager.destroyImage(contactShadowImage);
    resourceManager.destroyImage(debugImage);
}

void ContactShadowsPipeline::draw(VkCommandBuffer cmd, const ContactShadowsDrawInfo& drawInfo)
{

}

void ContactShadowsPipeline::createPipeline()
{

}
}
