#include "lo_semantics/heap_decommit_range_chain.h"
#include "lo_semantics/heap_range_context.h"

#include <stdexcept>

namespace lo::semantic::gpu::heap_decommit_range_chain
{
namespace
{
class Calls final : public heap_decommit_context::BoundaryServices,
    public heap_range_context::BoundaryServices
{
public:
    explicit Calls(NativeServices& native):native_(native){}

    void CallDirect(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {
        if(!heap_range_context::Apply(entry,memory,*this,state))
            throw std::invalid_argument("unselected decommit range call");
    }

    void CallNative(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {native_.CallNative(entry,memory,state);}

private:
    NativeServices& native_;
};
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    NativeServices& native,Registers& state)
{
    if(entry!=0x827cc668u) return false;
    Calls calls(native);
    return heap_decommit_context::Apply(entry,memory,calls,state);
}
} // namespace lo::semantic::gpu::heap_decommit_range_chain
