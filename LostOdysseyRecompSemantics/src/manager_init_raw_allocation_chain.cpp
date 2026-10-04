#include "lo_semantics/manager_init_raw_allocation_chain.h"
#include "lo_semantics/manager_init_context.h"
#include <stdexcept>
namespace lo::semantic::gpu::manager_init_raw_allocation_chain
{
namespace
{
class ManagerBoundary final : public manager_init_context::PpcBoundaryServices
{
public:
    ManagerBoundary(manager_init_raw_allocation_chain::PpcBoundaryServices& services, Registers& state)
        : services_(services), state_(state) {}
    void CallDirect(GuestAddress target, GuestMemory& memory,
        manager_init_context::Registers&) override
    {
        if (target == 0x823acbd0u)
        {
            if (!raw_allocation_context::Apply(target, memory, services_, state_))
                throw std::logic_error("recovered raw allocator unavailable");
        }
        else services_.CallDirect(target, memory, state_);
    }
    void CallVirtual(GuestAddress target, GuestMemory& memory,
        manager_init_context::Registers&) override
    { services_.CallVirtual(target, memory, state_); }
private:
    manager_init_raw_allocation_chain::PpcBoundaryServices& services_;
    Registers& state_;
};
}
void Apply(GuestMemory& memory, PpcBoundaryServices& services, Registers& state)
{
    ManagerBoundary boundary(services, state);
    manager_init_context::Apply(memory, boundary, state);
}
}
