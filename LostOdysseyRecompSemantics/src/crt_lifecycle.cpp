#include "lo_semantics/crt_lifecycle.h"

namespace lo::semantic::gpu
{
namespace
{

class ThreadDataAdapter final : public CrtThreadDataServices
{
public:
    ThreadDataAdapter(GuestMemory& memory, CrtLifecycleServices& services,
        CrtThreadDataCall& call, GuestAddress caller_sp)
        : memory_(memory), services_(services), call_(call),
          nested_sp_(caller_sp - 112u) {}

    std::uint64_t GetTlsValue(std::uint32_t index) override
    { return services_.GetTlsValue(index); }

    void SetTlsValue(std::uint32_t index, std::uint64_t value) override
    { services_.SetTlsValue(index, value); }

    std::uint64_t CallThreadDataGetter(GuestAddress function,
        std::uint64_t context) override
    { return services_.CallThreadDataGetter(function, context, call_); }

    std::uint64_t CallThreadDataGetterWithState(GuestAddress function,
        std::uint64_t context, CrtThreadDataCall& call) override
    { return services_.CallThreadDataGetter(function, context, call); }

    std::uint64_t AllocateThreadData(std::uint32_t count,
        std::uint32_t bytes_each) override
    { return AllocateCrtRecord(memory_, services_, count, bytes_each, nested_sp_); }

    std::uint64_t BindThreadData(GuestAddress function,
        std::uint64_t context, std::uint64_t data) override
    { return services_.BindThreadData(function, context, data, call_); }

    std::uint64_t BindThreadDataWithState(GuestAddress function,
        std::uint64_t context, std::uint64_t data,
        CrtThreadDataCall& call) override
    { return services_.BindThreadData(function, context, data, call); }

    void FreeThreadData(std::uint64_t data) override
    {
        (void)FreeCrtRecord(memory_, services_, data,
            static_cast<GuestAddress>(call_.thread_environment), nested_sp_);
    }

private:
    GuestMemory& memory_;
    CrtLifecycleServices& services_;
    CrtThreadDataCall& call_;
    GuestAddress nested_sp_;
};

} // namespace

std::uint64_t GetCrtThreadDataComposed(GuestMemory& memory,
    CrtLifecycleServices& services, CrtThreadDataCall& call,
    GuestAddress caller_sp)
{
    ThreadDataAdapter adapter(memory, services, call, caller_sp);
    return GetCrtThreadData(memory, adapter, call);
}

} // namespace lo::semantic::gpu
