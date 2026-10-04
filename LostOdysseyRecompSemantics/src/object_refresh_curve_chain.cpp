#include "lo_semantics/object_refresh_curve_chain.h"

#include <stdexcept>

namespace lo::semantic::gpu::object_refresh_curve_chain
{
namespace
{
class BoundaryAdapter final : public object_refresh_routes::PpcBoundaryServices,
    public object_curve_sample::NativeServices
{
public:
    BoundaryAdapter(object_refresh_routes::PpcBoundaryServices& remaining,
        object_curve_sample::NativeServices& curve,
        object_curve_sample::Registers& outer)
        : remaining_(remaining), curve_(curve), outer_(outer) {}

    void SetHostFpControl(std::uint32_t control) override
    { remaining_.SetHostFpControl(control); }

    void CallGuest(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    { remaining_.CallGuest(target, memory, state); }

    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    { remaining_.CallVirtual(target, memory, state); }

    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_curve_sample::Registers& state) override
    { curve_.CallVirtual(target, memory, state); }

    void CallDirect(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    {
        if (target != 0x822c7388u)
        {
            remaining_.CallDirect(target, memory, state);
            return;
        }
        object_curve_sample::Registers nested{};
        static_cast<object_child_float::Registers&>(nested) = state;
        nested.f10_bits = outer_.f10_bits;
        nested.f11_bits = outer_.f11_bits;
        nested.f12_bits = outer_.f12_bits;
        if (!object_curve_sample::Apply(target, memory, *this, nested))
            throw std::logic_error("missing curve sample lower");
        state = static_cast<const object_child_float::Registers&>(nested);
        outer_.f10_bits = nested.f10_bits;
        outer_.f11_bits = nested.f11_bits;
        outer_.f12_bits = nested.f12_bits;
    }

private:
    object_refresh_routes::PpcBoundaryServices& remaining_;
    object_curve_sample::NativeServices& curve_;
    object_curve_sample::Registers& outer_;
};
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_curve_sample::Registers& state)
{
    BoundaryAdapter boundary(dependencies.refresh.remaining,
        dependencies.curve, state);
    return object_refresh_metadata_chain::Apply(entry, memory,
        {dependencies.refresh.child_chain, boundary,
            dependencies.refresh.metadata}, state);
}

} // namespace lo::semantic::gpu::object_refresh_curve_chain
