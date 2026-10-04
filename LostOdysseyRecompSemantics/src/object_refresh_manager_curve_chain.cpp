#include "lo_semantics/object_refresh_manager_curve_chain.h"

namespace lo::semantic::gpu::object_refresh_manager_curve_chain
{
namespace
{
class MetadataAdapter final : public object_metadata_storage::PpcBoundaryServices
{
public:
    MetadataAdapter(object_metadata_storage::PpcBoundaryServices& allocator,
        manager_init_context::PpcBoundaryServices& manager)
        : allocator_(allocator), manager_(manager) {}

    void InitializeManager827C5F38(GuestMemory& memory,
        object_child_float::Registers& state) override
    { manager_init_context::Apply(memory, manager_, state); }

    void CallAllocator(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    { allocator_.CallAllocator(target, memory, state); }

private:
    object_metadata_storage::PpcBoundaryServices& allocator_;
    manager_init_context::PpcBoundaryServices& manager_;
};
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_curve_sample::Registers& state)
{
    MetadataAdapter metadata(dependencies.curve.refresh.metadata,
        dependencies.manager);
    return object_refresh_curve_chain::Apply(entry, memory,
        {{dependencies.curve.refresh.child_chain,
              dependencies.curve.refresh.remaining, metadata},
            dependencies.curve.curve}, state);
}

} // namespace lo::semantic::gpu::object_refresh_manager_curve_chain
