#include "lo_semantics/crt_thread_data.h"

namespace lo::semantic::gpu
{

std::uint64_t GetCrtThreadData(GuestMemory& memory,
    CrtThreadDataServices& services, GuestAddress thread_environment)
{
    constexpr GuestAddress context_global = 0x83214d74u;
    constexpr GuestAddress tls_index_global = 0x83214d78u;
    constexpr GuestAddress getter_fallback_global = 0x832d3adcu;
    constexpr GuestAddress binder_global = 0x832d3ae0u;
    constexpr GuestAddress record_vtable = 0x83215478u;

    // The allocation path may change the calling thread's current error code.
    const bool has_error_slot = memory.ReadU32(thread_environment + 336u) == 0;
    const GuestAddress error_state = has_error_slot ?
        memory.ReadU32(thread_environment + 256u) : 0;
    const std::uint32_t saved_error = has_error_slot ?
        memory.ReadU32(error_state + 352u) : 0;

    const std::uint64_t getter_context = memory.ReadU32(context_global);
    std::uint64_t getter = services.GetTlsValue(memory.ReadU32(tls_index_global));
    if (static_cast<GuestAddress>(getter) == 0)
    {
        getter = memory.ReadU32(getter_fallback_global);
        services.SetTlsValue(memory.ReadU32(tls_index_global), getter);
    }

    std::uint64_t record = services.CallThreadDataGetter(
        static_cast<GuestAddress>(getter) & ~3u, getter_context);
    if (static_cast<GuestAddress>(record) == 0)
    {
        record = services.AllocateThreadData(1, 196);
        if (static_cast<GuestAddress>(record) != 0)
        {
            const std::uint64_t bound = services.BindThreadData(
                memory.ReadU32(binder_global) & ~3u,
                memory.ReadU32(context_global), record);
            if (static_cast<GuestAddress>(bound) != 0)
            {
                const GuestAddress address = static_cast<GuestAddress>(record);
                memory.WriteU32(address + 20u, 1);
                memory.WriteU32(address + 92u, record_vtable);
                const GuestAddress thread_state = memory.ReadU32(thread_environment + 256u);
                memory.WriteU32(address, memory.ReadU32(thread_state + 332u));
                memory.WriteU32(address + 4u, 0xffffffffu);
            }
            else
            {
                services.FreeThreadData(record);
                record = 0;
            }
        }
    }

    if (memory.ReadU32(thread_environment + 336u) == 0)
    {
        const GuestAddress current_error_state = memory.ReadU32(thread_environment + 256u);
        memory.WriteU32(current_error_state + 352u, saved_error);
    }
    return record;
}

std::uint64_t OutputCrtErrorMessage(GuestMemory& memory,
    CrtErrorOutputServices& services, GuestAddress message,
    GuestAddress caller_sp)
{
    const GuestAddress descriptor = caller_sp - 16u;
    memory.WriteU16(descriptor, 0);
    memory.WriteU16(descriptor + 2u, 0);
    memory.WriteU32(descriptor + 4u, 0);
    services.InitAnsiString(memory, descriptor, message);
    return services.WriteAnsi(memory.ReadU32(descriptor + 4u),
                              memory.ReadU16(descriptor));
}

} // namespace lo::semantic::gpu
