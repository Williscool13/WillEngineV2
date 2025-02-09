//
// Created by William on 2025-02-09.
//

#ifndef IDENTIFIER_MANAGER_H
#define IDENTIFIER_MANAGER_H
#include <cstdint>
#include <vector>

#include "../../util/math_constants.h"

namespace will_engine::identifier {

class IdentifierManager {
public:
    static IdentifierManager* Get() { return identifierManager; }
    static void Set(IdentifierManager* _identifierManager) { identifierManager = _identifierManager; }

    /**
     * Application-wide identifier context. Exists for the application's lifetime
     */
    static IdentifierManager* identifierManager;
public:
    IdentifierManager();

    uint64_t getIdentifier();

    uint64_t registerIdentifier(uint64_t id);

private:
    std::vector <uint64_t> ids{};
    uint64_t currentId = 0;
};

} // will_engine

#endif //IDENTIFIER_MANAGER_H
