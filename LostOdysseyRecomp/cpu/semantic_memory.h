#pragma once

#include "cpu/semantic_accessor.h"
#include "lo_semantics/read_only_fields.h"
#include "lo_semantics/memory_writes.h"
#include "lo_semantics/single_write_fields.h"

#include <cstdlib>
#include <cstring>

namespace lo::runtime::semantic_memory
{
inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_MEMORY_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

using NativeMemory = semantic_accessor::NativeAccessorMemory;
} // namespace lo::runtime::semantic_memory
