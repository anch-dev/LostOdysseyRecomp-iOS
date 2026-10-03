#pragma once

#include "cpu/semantic_accessor.h"
#include "lo_semantics/pointer_fields.h"
#include "lo_semantics/global_assignments.h"
#include "lo_semantics/field_operations.h"

#include <cstdlib>
#include <cstring>

namespace lo::runtime::semantic_objects
{
inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_OBJECT_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

using NativeObjectMemory = semantic_accessor::NativeAccessorMemory;
} // namespace lo::runtime::semantic_objects
