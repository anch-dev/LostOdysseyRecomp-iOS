#pragma once

#include <cstdlib>
#include <cstring>

namespace lo::runtime::semantic_leaf
{

// A build with leaf wrappers still uses the original recompiled functions by
// default. Read the opt-in once so every guest call sees the same decision.
inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_LEAF_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

} // namespace lo::runtime::semantic_leaf
