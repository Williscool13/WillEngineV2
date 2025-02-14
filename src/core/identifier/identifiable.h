//
// Created by William on 2025-02-09.
//

#ifndef IDENTIFIABLE_H
#define IDENTIFIABLE_H
#include <cstdint>

namespace will_engine
{
class IIdentifiable
{
public:
    virtual ~IIdentifiable() = default;

    virtual void setId(uint64_t identifier) = 0;

    [[nodiscard]] virtual uint64_t getId() const = 0;
};
}


#endif //IDENTIFIABLE_H
