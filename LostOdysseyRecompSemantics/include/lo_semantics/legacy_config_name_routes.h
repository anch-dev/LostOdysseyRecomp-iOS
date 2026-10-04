#pragma once

#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/manager_facade.h"
#include "lo_semantics/manager_index_operations.h"
#include "lo_semantics/metadata_name_record.h"
#include "lo_semantics/registered_constructor_family.h"

#include <cstdint>

namespace lo::semantic::gpu::legacy_config_name_routes
{
using Registers = crt_stream_operations::Registers;

struct Dependencies
{
    CrtThreadDataServices& thread;
    InvalidParameterServices& invalid;
    metadata_name_record::Services& records;
    ManagerFacadeServices& manager;
    ArrayResizeServices& arrays;
    registered_constructor_family::RegistrationServices& registration;
    manager_index_operations::FpServices& fp;
};

// Two complete 124-instruction name routes. Every direct guest callee has an
// accepted semantic implementation; its allocator, manager and registration
// callbacks retain the lower service's explicit ABI boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_config_name_routes
