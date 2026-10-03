#include "lo_semantics/manager_facade.h"

#include <cstdint>

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kManagerGlobal = 0x8330b608u;

class FacadeArrayServices final : public ArrayResizeServices
{
public:
    FacadeArrayServices(GuestMemory& memory, ManagerFacadeServices& services,
        GuestAddress resize_entry_sp)
        : memory_(memory), services_(services),
          init_frame_(resize_entry_sp - 128u - 112u) {}

    void InitializeManager() override
    { (void)lo::semantic::gpu::InitializeManager(memory_, services_, init_frame_); }

    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    { return services_.ResizeStorage(method, manager, old_storage, bytes, argument); }

private:
    GuestMemory& memory_;
    ManagerFacadeServices& services_;
    GuestAddress init_frame_;
};

GuestAddress CurrentManager(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress init_frame)
{
    GuestAddress manager = memory.ReadU32(kManagerGlobal);
    if (manager == 0)
    {
        (void)InitializeManager(memory, services, init_frame);
        manager = memory.ReadU32(kManagerGlobal);
    }
    return manager;
}

void ClearHeaderWords(GuestMemory& memory, GuestAddress header)
{
    memory.WriteU32(header, 0);
    memory.WriteU32(header + 8u, 0);
    memory.WriteU32(header + 4u, 0);
}

} // namespace

std::uint64_t ReleaseManagerBuffer(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t buffer_register,
    GuestAddress caller_sp)
{
    const GuestAddress manager = CurrentManager(memory, services,
        caller_sp - 112u - 112u);
    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 12u) & ~3u;
    return services.ReleaseStorage(method, manager, buffer_register);
}

std::uint64_t AllocateManagerBuffer(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t bytes_register,
    GuestAddress caller_sp)
{
    const GuestAddress manager = CurrentManager(memory, services,
        caller_sp - 112u - 112u);
    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 4u) & ~3u;
    return services.AllocateStorage(method, manager, bytes_register, 8);
}

std::uint64_t ClearBufferHeader(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress header,
    GuestAddress caller_sp)
{
    const GuestAddress buffer = memory.ReadU32(header);
    std::uint64_t result = buffer;
    if (buffer != 0)
        result = ReleaseManagerBuffer(memory, services, buffer, caller_sp - 96u);
    ClearHeaderWords(memory, header);
    return result;
}

std::uint64_t ReleaseTwoByteArray(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress array,
    GuestAddress caller_sp)
{
    const GuestAddress remove_entry_sp = caller_sp - 96u;
    FacadeArrayServices array_services(memory, services,
        remove_entry_sp - 128u);
    RemoveArrayRange(memory, array_services, array, 0,
        memory.ReadU32(array + 4u), 2, 8, remove_entry_sp - 128u);
    const GuestAddress buffer = memory.ReadU32(array);
    std::uint64_t result = buffer;
    if (buffer != 0)
        result = ReleaseManagerBuffer(memory, services, buffer, remove_entry_sp);
    ClearHeaderWords(memory, array);
    return result;
}

std::uint64_t ResetTwoByteArray(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress array,
    GuestAddress caller_sp)
{
    const std::uint32_t capacity = memory.ReadU32(array + 8u);
    memory.WriteU32(array + 4u, 0);
    const GuestAddress nested_sp = caller_sp - 96u;
    if (capacity != 0)
    {
        memory.WriteU32(array + 8u, 0);
        FacadeArrayServices array_services(memory, services, nested_sp);
        ResizeArray(memory, array_services, array, 2, 8);
    }
    return ReleaseTwoByteArray(memory, services, array, nested_sp);
}

} // namespace lo::semantic::gpu
