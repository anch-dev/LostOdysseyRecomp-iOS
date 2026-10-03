"""Compare the real bulk registry body and real record/index callees together."""
import argparse
import json
from pathlib import Path

from generate_metadata_name_registry import reviewed, MANIFEST, TABLE
from generate_metadata_name_record import generate as record_manifest
from generate_metadata_name_index import originals as index_entries
from test_semantic_metadata_name_index import HELPERS, helper_body
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-name-registry-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest, table = reviewed(args.ppc_root)
    records = record_manifest()
    indices = index_entries(args.ppc_root)
    if (json.loads(MANIFEST.read_text()) != manifest or TABLE.read_text() != table or
            json.loads((semantics / "metadata_name_record_families.json").read_text()) != records or
            json.loads((semantics / "metadata_name_index_families.json").read_text())["entries"] != indices):
        raise ValueError("registry or dependency manifest changed")
    declarations = b"\n".join(f"PPC_FUNC({name});".encode() for name in (
        "sub_823F7B08", "sub_823F44E8", "sub_82296F68", "sub_82296FE8",
        "sub_82296830", "sub_8230BAC0", "sub_8229F678", "sub_827C5F38",
        "__savegprlr_27", "__restgprlr_27", "__savegprlr_29", "__restgprlr_29"))
    declarations += b"""
void RegistryAllocate(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) RegistryAllocate(ctx, base, address)
"""
    bodies = b"\n".join([declarations] + [
        helper_body(args.ppc_root, *helper) for helper in HELPERS] + [
        e["translated_body"].encode() for e in indices] + [
        records["entries"][0]["translated_body"].encode(),
        manifest["entries"][0]["translated_body"].encode()])
    sources = ["metadata_name_registry", "metadata_name_record", "metadata_name_index",
        "registered_metadata_composed", "registered_metadata_string", "manager_init",
        "registered_metadata_words", "registered_constructor_family", "registered_callback_family",
        "registered_getter_family", "registered_inline_constructor", "object_registration",
        "object_startup", "manager_facade", "allocation_array", "memory_move"]
    compile_and_run("metadata-name-registry", bodies,
        (semantics / "tests/metadata_name_registry_oracle.cpp").read_bytes(),
        [semantics / f"src/{name}.cpp" for name in sources], args.output)


if __name__ == "__main__":
    main()
