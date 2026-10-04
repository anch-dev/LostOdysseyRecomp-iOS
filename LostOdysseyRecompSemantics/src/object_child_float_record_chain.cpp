#include "lo_semantics/object_child_float_record_chain.h"

#include <stdexcept>

namespace lo::semantic::gpu::object_child_float_record_chain
{
namespace
{
class NativeAdapter final : public object_child_float::NativeServices
{
public:
    explicit NativeAdapter(Dependencies dependencies)
        : dependencies_(dependencies) {}

    void SetHostFpControl(std::uint32_t control) override
    { dependencies_.child_external.SetHostFpControl(control); }

    void CallGuest(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    {
        if (target != 0x82384c08u)
        {
            dependencies_.child_external.CallGuest(target, memory, state);
            return;
        }

        object_float_record::Registers record{};
        record.sp = state.r[1]; record.lr = state.lr;
        record.r3 = state.r[3]; record.r11 = state.r[11];
        record.r12 = state.r[12]; record.r31 = state.r[31];
        record.xer_so = state.xer_so;
        record.cr6 = {state.cr6.lt, state.cr6.gt,
            state.cr6.eq, state.cr6.un};
        if (!object_float_record_post_chain::Apply(memory,
                dependencies_.record, record))
            throw std::runtime_error("missing object-float-record chain");
        state.r[1] = record.sp; state.lr = record.lr;
        state.r[3] = record.r3; state.r[11] = record.r11;
        state.r[12] = record.r12; state.r[31] = record.r31;
        state.xer_so = record.xer_so;
        state.cr6 = {record.cr6.lt, record.cr6.gt,
            record.cr6.eq, record.cr6.un};
    }

private:
    Dependencies dependencies_;
};
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state)
{
    NativeAdapter native(dependencies);
    return object_child_float::Apply(entry, memory, native, state);
}

} // namespace lo::semantic::gpu::object_child_float_record_chain
