//
// Created by William on 2025-02-09.
//

#include "identifier_manager.h"

#include <../../../extern/fmt/include/fmt/format.h>

namespace will_engine {
identifier::IdentifierManager* identifier::IdentifierManager::identifierManager = nullptr;

identifier::IdentifierManager::IdentifierManager()
{
    ids.reserve(1024);
}

uint64_t identifier::IdentifierManager::getIdentifier()
{
    return currentId++;
}

uint64_t identifier::IdentifierManager::registerIdentifier(const uint64_t id)
{
    if (id == INDEX64_NONE) {
        return currentId++;
    }
#if _DEBUG
    if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
        fmt::print("WARNING: Identifier already exists, the system will assign a new identifier. This might damage references.");
        return currentId++;
    }
#endif
    if (ids.capacity() == ids.size()) {
        ids.reserve(ids.size() * 2);
    }

    ids.push_back(id);
    if (id > currentId) {
        currentId = id + 1;
    }
    return id;
}
} // will_engine