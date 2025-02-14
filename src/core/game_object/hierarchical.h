//
// Created by William on 2025-01-27.
//

#ifndef HIERARCHICAL_H
#define HIERARCHICAL_H
#include <vector>
#include <string>
#include <string_view>

namespace will_engine
{
class IHierarchical
{
public:
    virtual ~IHierarchical() = default;

    virtual bool addChild(IHierarchical* child) = 0;

    virtual bool removeChild(IHierarchical* child) = 0;

    virtual void reparent(IHierarchical* newParent) = 0;

    /**
     * Should be called whenever the hierarchy is modified (be it the hierarchy itself or some property change that requires propagation to children)
     */
    virtual void dirty() = 0;

    virtual void recursiveUpdate(int32_t currentFrameOverlap, int32_t previousFrameOverlap) = 0;

    virtual void setParent(IHierarchical* newParent) = 0;

    virtual IHierarchical* getParent() const = 0;

    virtual const std::vector<IHierarchical*>& getChildren() const = 0;

    virtual std::vector<IHierarchical*>& getChildren() = 0;

    virtual void setName(std::string newName) = 0;

    virtual std::string_view getName() const = 0;
};
}


#endif //HIERARCHICAL_H
