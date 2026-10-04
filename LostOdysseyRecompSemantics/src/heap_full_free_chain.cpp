#include "lo_semantics/heap_full_free_chain.h"
#include "lo_semantics/heap_decommit_range_chain.h"
#include "lo_semantics/heap_insert_context.h"

#include <stdexcept>

namespace lo::semantic::gpu::heap_full_free_chain
{
namespace
{
class Calls final : public heap_coalesce_context::BoundaryServices,
    public heap_decommit_range_chain::NativeServices
{
public:
    explicit Calls(heap_full_free_chain::NativeServices& native):
        native_(native){}

    void CallDirect(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {
        if(heap_insert_context::Apply(entry,memory,state) ||
            heap_decommit_range_chain::Apply(entry,memory,*this,state))
            return;
        throw std::invalid_argument("unselected free guest call");
    }

    void CallNative(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {native_.CallNative(entry,memory,state);}

private:
    heap_full_free_chain::NativeServices& native_;
};
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    NativeServices& native,Registers& state)
{
    if(entry!=0x823ade28u && entry!=0x823ae0bcu) return false;
    Calls calls(native);
    return heap_coalesce_context::ApplyFree(entry,memory,calls,state);
}
} // namespace lo::semantic::gpu::heap_full_free_chain
