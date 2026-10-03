"""Recover exact constant metadata-word sequences from the cached PPC entries.

Every complete instruction list must match a reviewed form. No PPC scan or hash.
"""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
BEGIN = "    // BEGIN GENERATED METADATA WORD PARAMETERS"
END = "    // END GENERATED METADATA WORD PARAMETERS"
SINGLE = {"82407B80", "825AA760", "825EA998", "825EC9B0", "825F1B28",
          "8267A528", "826D6F90", "826FA288", "82723D80"}
MULTIPLE = {"8240C9B8", "825E8728", "8240CD38", "825AABD8", "825E7698"}
SPECIAL = {"825B3338", "8267A438"}


def constant_words(instructions):
    constants = {}
    words = []
    for instruction in instructions:
        match = re.fullmatch(r"(lis|li) (r\d+),(-?\d+)", instruction)
        if match:
            op, register, literal = match.groups()
            constants[register] = (int(literal) << (16 if op == "lis" else 0)) & 0xffffffff
        match = re.fullmatch(r"ori (r\d+),(r\d+),(\d+)", instruction)
        if match:
            target, source, literal = match.groups()
            constants[target] = constants[source] | int(literal)
        match = re.fullmatch(r"stw (r\d+),80\(r1\)", instruction)
        if match:
            words.append(constants[match.group(1)])
    return words


def first_word(word, single=False, object_saved=False):
    high, low = word >> 16, word & 0xffff
    if object_saved:
        return ["mr r30,r3", f"lis r10,{high}", "addi r4,r1,80",
                f"ori r10,r10,{low}", "lwz r11,52(r30)",
                "addi r31,r11,364", "stw r10,80(r1)", "mr r3,r31",
                "bl 0x825f41e8"]
    common = [f"lis r10,{high}", "lwz r11,52(r3)", "addi r4,r1,80",
              f"ori r10,r10,{low}"]
    if single:
        return common + ["addi r3,r11,364", "stw r10,80(r1)", "bl 0x825f41e8"]
    return common + ["addi r31,r11,364", "mr r3,r31",
                     "stw r10,80(r1)", "bl 0x825f41e8"]


def next_word(word, patch=False, small=False):
    if small:
        return [f"li r11,{word}", "addi r4,r1,80", "mr r3,r31",
                "stw r11,80(r1)", "bl 0x825f41e8"]
    high, low = word >> 16, word & 0xffff
    result = [f"lis r11,{high}"]
    if patch:
        result.append("lwz r10,0(r31)")
    result += ["addi r4,r1,80", f"ori r11,r11,{low}", "mr r3,r31", "stw r11,80(r1)"]
    if patch:
        result += ["lwz r11,4(r31)", "rlwinm r11,r11,2,0,29", "add r11,r11,r10",
                   "lwz r10,-4(r11)", "rlwinm r9,r10,0,0,7", "addis r9,r9,256",
                   "rlwimi r9,r10,0,8,31", "stw r9,-4(r11)"]
    return result + ["bl 0x825f41e8"]


def recover(candidate):
    address = candidate["address"]
    instructions = [line.strip() for line in candidate["instructions"]]
    words = constant_words(instructions)
    single = address in SINGLE
    object_saved = address == "825B3338"
    patched = address == "8267A438"
    frame = 96 if single else 112
    expected = ["mflr r12", "stw r12,-8(r1)"]
    if object_saved:
        expected.append("std r30,-24(r1)")
    if not single:
        expected.append("std r31,-16(r1)")
    expected += [f"stwu r1,-{frame}(r1)"]
    expected += first_word(words[0], single, object_saved)
    for index, word in enumerate(words[1:], 1):
        expected += next_word(word, patch=patched and index == 5,
                              small=patched and index in (2, 3))
    if object_saved:
        expected += ["li r11,0", "stw r11,508(r30)"]
    expected += [f"addi r1,r1,{frame}", "lwz r12,-8(r1)", "mtlr r12"]
    if object_saved:
        expected.append("ld r30,-24(r1)")
    if not single:
        expected.append("ld r31,-16(r1)")
    expected.append("blr")
    if instructions != expected or not 1 <= len(words) <= 10:
        raise ValueError(f"Unreviewed instruction structure: {address}")
    return {"address": address, "source": candidate["generated_ppc_path"],
            "source_line": candidate["line"], "frame_size": frame,
            "words": [f"0x{word:08X}" for word in words],
            "patch_previous_index": 5 if patched else -1,
            "clear_object_offset": 508 if object_saved else 0}


def write_if_changed(path, text):
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8", newline="\n")


def main():
    candidates = json.loads((ROOT / "out/function-inventory/registered-extra-methods.json")
                            .read_text(encoding="utf-8"))
    selected = [entry for entry in candidates if entry["address"] in SINGLE | MULTIPLE | SPECIAL]
    if {entry["address"] for entry in selected} != SINGLE | MULTIPLE | SPECIAL:
        raise ValueError("Reviewed metadata-word set changed")
    recovered = sorted((recover(entry) for entry in selected), key=lambda e: e["address"])
    payload = {"schema_version": 1, "family": "registered_metadata_word",
               "source": "out/function-inventory/registered-extra-methods.json",
               "entry_count": len(recovered), "entries": recovered}
    write_if_changed(SEMANTICS / "registered_metadata_word_families.json",
                     json.dumps(payload, indent=2) + "\n")
    path = SEMANTICS / "src/registered_metadata_words.cpp"
    source = path.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ parameter markers changed")
    prefix, tail = source.split(BEGIN, 1)
    _, suffix = tail.split(END, 1)
    rows = [f"    {{0x{e['address']}u, {e['frame_size']}, {len(e['words'])}, "
            f"{e['patch_previous_index']}, {e['clear_object_offset']}, "
            "{" + ", ".join(word + "u" for word in e["words"]) + "}},"
            for e in recovered]
    write_if_changed(path, prefix + BEGIN + "\n" + "\n".join(rows) + "\n" + END + suffix)
    print(f"Recovered {len(recovered)} exact metadata-word sequences.")


if __name__ == "__main__":
    main()
