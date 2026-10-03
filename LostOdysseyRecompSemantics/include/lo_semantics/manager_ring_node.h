#pragma once

#include "lo_semantics/instance_manager_link_family.h"
#include "lo_semantics/manager_facade.h"
#include "lo_semantics/ring_reservation.h"

namespace lo::semantic::gpu::manager_ring_node
{
struct Registers
{
    std::uint64_t lr;
    std::uint64_t r28;
    std::uint64_t r29;
    std::uint64_t r30;
    std::uint64_t r31;
    double f0;
    double f13;
};

class VirtualServices
{
public:
    virtual ~VirtualServices() = default;
    virtual std::uint64_t Call(GuestAddress method, GuestMemory& memory,
        std::uint64_t receiver, std::uint64_t caller_sp, Registers& registers) = 0;
};

// 82326580 chooses the ring-backed or direct-allocated node, initializes its
// data, invokes the node's vtable+4 and stores the result in its owner. The
// final vtable call remains external. Zero allocation/storage still reaches
// the original indirect dispatch, including its zero-address reads.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    ring_reservation::SynchronizationServices& synchronization,
    instance_manager_link_family::FpServices& fp_services,
    VirtualServices& virtual_services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    Registers& registers, std::uint64_t& result);
} // namespace lo::semantic::gpu::manager_ring_node
