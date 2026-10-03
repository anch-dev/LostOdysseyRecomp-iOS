#pragma once

#include "lo_semantics/allocation_array.h"
#include "lo_semantics/manager_facade.h"
#include "lo_semantics/registered_constructor_family.h"

#include <cstdint>

namespace lo::semantic::gpu::registered_metadata_string
{

// 82296830 counts big-endian UTF-16 code units through the first zero word.
// The PPC signed-word distance calculation determines the full r3 result.
[[nodiscard]] std::uint64_t Utf16Length(GuestMemory& memory,
    std::uint64_t source_register);

// 8229C8B0 initializes a three-word string header. 8229F5E0 assigns one,
// returning immediately if its current storage equals the source pointer.
// caller_sp is the guest stack pointer at entry to that function. Its nested
// forward copy may write the full destination register at caller_sp-120.
[[nodiscard]] std::uint64_t InitializeString(GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t destination_register,
    std::uint64_t source_register, GuestAddress caller_sp);
[[nodiscard]] std::uint64_t AssignString(GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t destination_register,
    std::uint64_t source_register, GuestAddress caller_sp);

// Apply one of the six reviewed entries. Constructor services are used only
// by 824070C8's nested 8240B1B8 singleton call. Unknown addresses have no
// effects. ABI save/restore and condition-register state belong to the caller
// adapter; ordinary bounded RAM and guest32 service contracts apply here.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    GuestAddress caller_sp, std::uint64_t& result);

} // namespace lo::semantic::gpu::registered_metadata_string
