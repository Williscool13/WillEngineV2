//
// Created by William on 2025-02-16.
//

#ifndef IMGUI_RENDERABLE_H
#define IMGUI_RENDERABLE_H

namespace will_engine
{
class IImguiRenderable
{
public:
    virtual ~IImguiRenderable() = default;

    virtual void renderImgui() = 0;

    virtual void selectedRenderImgui() = 0;
};
}

#endif //IMGUI_RENDERABLE_H
