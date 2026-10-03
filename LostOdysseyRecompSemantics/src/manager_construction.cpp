#include "lo_semantics/manager_construction.h"

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kPrimaryVtable = 0x82002700u;
constexpr GuestAddress kPrimaryConstant = 0x82000fe8u;
constexpr GuestAddress kFallbackVtable = 0x8201dd90u;
constexpr GuestAddress kTransientVtable = 0x82189900u;
constexpr GuestAddress kCriticalSectionVtable = 0x8201fa3cu;
constexpr GuestAddress kCriticalSectionGlobal = 0x83315fd4u;
constexpr GuestAddress kPreviousManagerGlobal = 0x83315fd8u;
} // namespace

std::uint64_t ConstructPrimaryManager(GuestMemory& memory,
    std::uint64_t allocation_register, GuestAddress stack_pointer)
{
    const GuestAddress object = static_cast<GuestAddress>(allocation_register);
    const std::uint32_t constant_high = memory.ReadU32(kPrimaryConstant);
    const std::uint32_t constant_low = memory.ReadU32(kPrimaryConstant + 4);
    memory.WriteU32(stack_pointer - 16u, constant_high);
    memory.WriteU32(stack_pointer - 12u, constant_low);

    memory.WriteU32(object, kPrimaryVtable);
    memory.WriteU32(object + 0x20dbcu, 0);
    memory.WriteU32(object + 0x20dc0u, 0);
    memory.WriteU32(object + 0x20dc8u, 0);
    memory.WriteU32(object + 0x20dc4u, 0);
    memory.WriteU32(object + 0x20dccu, 0);
    memory.WriteU32(object + 0x20dd0u, 0);
    memory.WriteU32(object + 0x20dd4u, 0);
    memory.WriteU32(object + 0x48de4u, 0);
    memory.WriteU32(object + 0x48de8u, 0);

    // Reload both halves after all stores and before either destination write.
    const std::uint32_t reloaded_high = memory.ReadU32(stack_pointer - 16u);
    const std::uint32_t reloaded_low = memory.ReadU32(stack_pointer - 12u);
    memory.WriteU32(object + 0x20dd8u, reloaded_high);
    memory.WriteU32(object + 0x20ddcu, reloaded_low);
    return allocation_register;
}

std::uint64_t ConstructFallbackManager(GuestMemory& memory,
    ManagerConstructionServices& services, std::uint64_t allocation_register,
    std::uint64_t current_manager_register, GuestAddress frame_base)
{
    const GuestAddress object = static_cast<GuestAddress>(allocation_register);
    const GuestAddress critical_section_object = object + 4u;
    memory.WriteU32(frame_base + 148u, object);
    memory.WriteU32(object, kFallbackVtable);
    memory.WriteU32(frame_base + 80u, critical_section_object);
    memory.WriteU32(critical_section_object, kTransientVtable);
    memory.WriteU32(critical_section_object, kCriticalSectionVtable);

    services.InitializeCriticalSection(allocation_register + 8u);
    memory.WriteU32(kCriticalSectionGlobal, critical_section_object);
    memory.WriteU32(kPreviousManagerGlobal,
                    static_cast<GuestAddress>(current_manager_register));
    return allocation_register;
}

} // namespace lo::semantic::gpu
