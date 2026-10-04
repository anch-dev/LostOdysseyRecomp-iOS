#include "lo_semantics/object_float_record_post_chain.h"

#include "lo_semantics/recovery_abi.h"

#include <stdexcept>

namespace lo::semantic::gpu::object_float_record_post_chain
{
namespace
{
using recovery_abi::Address;

class PostAdapter final : public registered_constructor_family::RegistrationServices
{
public:
    PostAdapter(GuestMemory& memory, Dependencies dependencies,
        std::uint64_t outer_sp)
        : memory_(memory), dependencies_(dependencies), outer_sp_(outer_sp) {}

    std::uint64_t Register(GuestAddress target, std::uint64_t incoming_r3,
        GuestAddress caller_sp) override
    {
        if (target != 0x82627230u)
            return dependencies_.external_registration.Register(
                target, incoming_r3, caller_sp);

        // This callback is reached from 8242D038's 128-byte frame inside
        // 82384C08's 96-byte frame. The accepted lower exposes no volatile
        // PPC registers, so only r1/r3/LR are initialized; nested generic
        // register and frame scratch remain outside this validation scope.
        object_registration_post::Registers post{};
        post.r[1] = outer_sp_ - 96u - 128u;
        if (Address(post.r[1]) != caller_sp)
            throw std::runtime_error("unexpected post-registration guest stack");
        dependencies_.observer.Enter(target, incoming_r3, caller_sp,
            memory_.ReadU32(0x833189ecu));
        post.r[3] = incoming_r3;
        post.lr = 0x8242d0e0u;
        object_registration_post::Dependencies post_dependencies{
            dependencies_.manager, *this, dependencies_.callback,
            dependencies_.graph, dependencies_.dynamic};
        (void)object_registration_post::Apply(0x82627230u, memory_,
            post_dependencies, post);
        return post.r[3];
    }

private:
    GuestMemory& memory_;
    Dependencies dependencies_;
    std::uint64_t outer_sp_;
};
} // namespace

bool Apply(GuestMemory& memory, Dependencies dependencies,
    object_float_record::Registers& state)
{
    PostAdapter registration(memory, dependencies, state.sp);
    return object_float_record::Apply(0x82384c08u, memory,
        dependencies.manager, registration, state);
}

} // namespace lo::semantic::gpu::object_float_record_post_chain
