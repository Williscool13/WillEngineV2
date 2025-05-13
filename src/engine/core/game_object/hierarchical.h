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

    virtual void beginPlay() = 0;
    virtual void beginDestroy() = 0;
    virtual void update(float deltaTime) = 0;

    /**
     * The preferred way to set a parent/child hierarchy relationship. Only works if the child's parent is \code nullptr\endcode.
     * \n Adds \code child\endcode to \code children\endcode and sets the child's \code parent\endcode to \code this\endcode.
     * @param child
     * @return
     */
    virtual bool addChild(IHierarchical* child) = 0;

    /**
     * Removes the child from \code this\endcode. Will attempt to clear the child's \code parent\endcode reference.
     * @param child
     * @return
     */
    virtual bool removeChild(IHierarchical* child) = 0;

    virtual bool removeParent() = 0;

    virtual void reparent(IHierarchical* newParent) = 0;

    virtual void destroy() = 0;

    /**
     * Should be called whenever the hierarchy is modified (be it the hierarchy itself or some property change that requires propagation to children)
     */
    virtual void dirty() = 0;

    virtual void setParent(IHierarchical* newParent) = 0;

    [[nodiscard]] virtual IHierarchical* getParent() const = 0;

    [[nodiscard]] virtual const std::vector<IHierarchical*>& getChildren() const = 0;

    virtual std::vector<IHierarchical*>& getChildren() = 0;

    virtual void setName(std::string newName) = 0;

    [[nodiscard]] virtual std::string_view getName() const = 0;
};
}


#endif //HIERARCHICAL_H
