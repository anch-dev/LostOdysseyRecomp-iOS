#include "lo_semantics/crt_stream_follow61.h"
namespace lo::semantic::gpu::crt_stream_follow61 {
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    dependencies.accepted.full_flush=&dependencies.full_flush;
    return crt_stream_reader_final61::ApplySupport(entry,memory,dependencies.accepted,state);
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    dependencies.accepted.full_flush=&dependencies.full_flush;
    return crt_stream_reader_final61::ApplyAcceptedLower(entry,memory,dependencies.accepted,state);
}
}
