#include "lo_semantics/object_refresh_metadata_chain.h"

#include <stdexcept>

namespace lo::semantic::gpu::object_refresh_metadata_chain
{
namespace
{
class BoundaryAdapter final : public object_refresh_routes::PpcBoundaryServices
{
public:
    explicit BoundaryAdapter(Dependencies dependencies)
        : dependencies_(dependencies) {}

    void SetHostFpControl(std::uint32_t control) override
    { dependencies_.remaining.SetHostFpControl(control); }

    void CallGuest(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    { dependencies_.remaining.CallGuest(target, memory, state); }

    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    { dependencies_.remaining.CallVirtual(target, memory, state); }

    void CallDirect(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    {
        if (target == 0x825f41e8u || target == 0x822c42d8u)
        {
            if (!object_metadata_storage::Apply(target, memory,
                    dependencies_.metadata, state))
                throw std::logic_error("missing metadata storage lower");
            return;
        }
        dependencies_.remaining.CallDirect(target, memory, state);
    }

private:
    Dependencies dependencies_;
};
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state)
{
    BoundaryAdapter boundary(dependencies);
    return object_refresh_routes::Apply(entry, memory,
        {dependencies.child_chain, boundary}, state);
}

} // namespace lo::semantic::gpu::object_refresh_metadata_chain
