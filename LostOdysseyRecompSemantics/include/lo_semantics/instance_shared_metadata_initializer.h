#pragma once

#include "lo_semantics/manager_facade.h"
#include "lo_semantics/registered_callback_family.h"
#include "lo_semantics/registered_constructor_family.h"

#include <cstdint>

namespace lo::semantic::gpu::instance_shared_metadata_initializer
{

// __savegprlr_28 writes these four full registers, then incoming LR's low
// word, above the 160-byte frame. __restgprlr_28 reloads their live values.
struct FrameRegisters
{
    std::uint64_t lr{};
    std::uint64_t r28{};
    std::uint64_t r29{};
    std::uint64_t r30{};
    std::uint64_t r31{};
};

// 824108C8 initializes shared metadata fields. 82412100 returns immediately
// for a low-word null pointer and otherwise tail-calls the initializer.
// The constructor and graph-registration implementations are existing guest
// callback compositions. Unknown addresses have no effects. Generic volatile
// registers, condition state, faults/MMIO and lower-callee ABI are outside this
// bounded ordinary-memory API.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    registered_callback_family::Services& callback_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_shared_metadata_initializer
