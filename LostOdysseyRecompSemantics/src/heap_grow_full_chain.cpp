#include "lo_semantics/heap_grow_full_chain.h"
#include "lo_semantics/heap_growth_lower_context.h"

#include <stdexcept>

namespace lo::semantic::gpu::heap_grow_full_chain
{
namespace
{
class Calls final : public heap_grow_context::BoundaryServices,
    public heap_growth_lower_context::BoundaryServices
{
public:
    explicit Calls(heap_grow_full_chain::BoundaryServices& services):
        services_(services){}

    void CallDirect(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {
        if(!heap_growth_lower_context::Apply(entry,memory,*this,state))
            throw std::invalid_argument("unselected grow guest call");
    }

    void CallNative(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {services_.CallNative(entry,memory,state);}

    void CallIndirect(GuestAddress target,GuestMemory& memory,
        Registers& state) override
    {services_.CallIndirect(target,memory,state);}

private:
    heap_grow_full_chain::BoundaryServices& services_;
};
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    if(entry!=0x827cc428u) return false;
    Calls calls(services);
    return heap_grow_context::Apply(entry,memory,calls,state);
}
} // namespace lo::semantic::gpu::heap_grow_full_chain
