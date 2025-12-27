import argparse
import os
import struct

from elftools.elf.elffile import ELFFile
from elftools.elf.enums import ENUM_EI_CLASS


class ELFLoader:
    def __init__(self, filename):
        self.filename = filename
        self.sections = {}
        self.segments = []
        self.data_layout = 8
        self._load_metadata()

    def _load_metadata(self):
        with open(self.filename, "rb") as elf_file:
            elf = ELFFile(elf_file)

            data_layout = elf["e_ident"]["EI_CLASS"]
            self.data_layout = 8 if ENUM_EI_CLASS[data_layout] == 2 else 4

            # Load section info
            for section in elf.iter_sections():
                self.sections[section.name] = {
                    "name": section.name,
                    "offset": section["sh_offset"],
                    "vaddr": section["sh_addr"],
                    "size": section["sh_size"],
                    "type": section["sh_type"],
                }

            # Load segment info (for offset <-> VA)
            for segment in elf.iter_segments():
                if segment["p_type"] == "PT_LOAD":
                    self.segments.append(
                        {
                            "offset": segment["p_offset"],
                            "vaddr": segment["p_vaddr"],
                            "filesz": segment["p_filesz"],
                            "memsz": segment["p_memsz"],
                        }
                    )

    def offset_to_va(self, offset):
        # Try LOAD segments first (true runtime VAs)
        for seg in self.segments:
            if seg["offset"] <= offset < seg["offset"] + seg["filesz"]:
                return seg["vaddr"] + (offset - seg["offset"])

        # Fall back to static section mapping
        for sec in self.sections.values():
            if sec["offset"] <= offset < sec["offset"] + sec["size"]:
                return sec["vaddr"] + (offset - sec["offset"])

        raise ValueError(f"Offset 0x{offset:x} not in any segment or section")

    def va_to_offset(self, va):
        for seg in self.segments:
            if seg["vaddr"] <= va < seg["vaddr"] + seg["filesz"]:
                return seg["offset"] + (va - seg["vaddr"])

        for sec in self.sections.values():
            start = sec["vaddr"]
            end = start + sec["size"]
            if start <= va < end:
                return sec["offset"] + (va - start)

        raise ValueError(f"VA 0x{va:x} not in any segment or section")

    def get_section_va(self, name):
        sec = self.sections.get(name)
        if not sec:
            raise KeyError(f"Section {name} not found")
        return sec["vaddr"]

    def get_section_by_va(self, va):
        for sec in self.sections.values():
            start = sec["vaddr"]
            end = start + sec["size"]
            if start <= va < end:
                return sec
        return None

    def get_section_by_name(self, name):
        for section in self.sections.values():
            if section["name"] == name:
                return section
        return None

    def get_sections_by_type(self, ty):
        sections = []
        for section in self.sections.values():
            if section["type"] == ty:
                sections.append(section)
        return sections

    def get_base_address(self):
        if not self.segments:
            raise RuntimeError("No LOAD segments found")
        return min(seg["vaddr"] for seg in self.segments)


def xtea_encipher(v, key, num_rounds=12, delta=42874864):
    v0, v1 = v
    sum_ = 0
    mask = 0xFFFFFFFF
    for _ in range(num_rounds):
        v0 = (v0 + ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum_ + key[sum_ & 3]))) & mask
        sum_ = (sum_ + delta) & mask
        v1 = (
            v1 + ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum_ + key[(sum_ >> 11) & 3]))
        ) & mask
    return [v0, v1]


def parse_zyrox_tables(filename):
    tables = {}
    current_table_id = None
    collected_entries = []

    with open(filename, "r") as tables_file:
        for line in tables_file:
            line = line.strip()
            if not line:
                continue

            if line.startswith("@table"):
                if current_table_id is not None:
                    tables[current_table_id] = collected_entries

                parts = line.split()
                current_table_id = int(parts[1])
                collected_entries = []

            else:
                nums = list(map(int, line.split()))
                if len(nums) != 6:
                    raise ValueError(f"Malformed entry: {line}")

                entry = {"key": nums[0:4], "delta": nums[4], "rounds": nums[5]}
                collected_entries.append(entry)

        if current_table_id is not None:
            tables[current_table_id] = collected_entries

    return tables


parser = argparse.ArgumentParser(description="Set input/output paths and platform.")

parser.add_argument("--android", action="store_true", help="Target is Android")
parser.add_argument("--in", dest="in_file", required=True, help="Input file path")
parser.add_argument(
    "--out",
    dest="out_file",
    help="Output file path (defaults to input path if not given)",
)
parser.add_argument(
    "--tables",
    type=str,
    default="zyrox_tables.txt",
    help="Name or path of the tables file (default: zyrox_tables.txt)",
)

args = parser.parse_args()

is_android = args.android
in_file = args.in_file
out_file = args.out_file if args.out_file else in_file

zyrox_tables_info = parse_zyrox_tables(args.tables)

lib = ELFLoader(in_file)

relocations = lib.get_sections_by_type("SHT_RELA") + lib.get_sections_by_type("SHT_REL")

with open(in_file, "rb") as f:
    elf_data = bytearray(f.read())

ptr_size = lib.data_layout
is_32bit = ptr_size == 4
magic_bytes = int(0xDEADBEEF).to_bytes(4, byteorder="little")

if ptr_size > len(magic_bytes):
    magic_bytes += bytes(ptr_size - len(magic_bytes))  # pad to the right

SIG = magic_bytes * 3
header_size = len(SIG) + ptr_size * 1  # id

ptr_fmt = "<Q" if lib.data_layout == 8 else "<I"

rel_entry_size = ptr_size * (2 if is_32bit else 3)  # rel on arm32 and rela on arm64


def read_ptr(offset):
    return struct.unpack(ptr_fmt, elf_data[offset : offset + ptr_size])[0]


def get_zyrox_tables(lookup_section):
    section = lib.get_section_by_name(lookup_section)
    if section is None:
        return None
    sh_offset = section["offset"]
    sh_end = section["size"] + sh_offset
    tables = []
    while sh_offset + header_size < sh_end:
        if elf_data[sh_offset : sh_offset + len(SIG)] == SIG:
            sh_offset += len(SIG)
            tid = read_ptr(sh_offset)
            sh_offset += ptr_size
            table = {
                "sh_offset": sh_offset - header_size,
                "table_start": sh_offset,
                "id": tid,
                "size": len(zyrox_tables_info[tid]) * (2 if is_32bit else 1),
            }
            tables.append(table)
            sh_offset += ptr_size * table["size"]
        else:
            sh_offset += 1
    return tables


zyrox_tables = get_zyrox_tables(".data")

if zyrox_tables is None or len(zyrox_tables) == 0:
    zyrox_tables = get_zyrox_tables(".rodata")

if zyrox_tables is None or len(zyrox_tables) == 0:
    print("zyrox tables not found, is this binary even made by Zyrox?")
    exit()


def find_relocation_entry_offset(
    sig,
):  # sig is target address + value 8 padded with 7 bytes on 64-bit
    for rel in relocations:
        rel_data = elf_data[rel["offset"] : rel["size"] + rel["offset"]]
        _offset = 0
        while _offset + rel_entry_size <= len(rel_data):
            if (
                rel_data[
                    _offset : _offset + rel_entry_size - (0 if is_32bit else ptr_size)
                ]
                == sig
            ):
                return _offset + rel["offset"]
            _offset += rel_entry_size
    return None


ptr_sig = ptr_size.to_bytes(ptr_size, byteorder="little")

rel_sig = 0x17 if is_32bit else (0x403 if is_android else 8)

for zyrox_table in zyrox_tables:
    table_start = lib.offset_to_va(zyrox_table["table_start"])
    table_size = zyrox_table["size"]
    table_id = zyrox_table["id"]
    table_info = zyrox_tables_info[table_id]
    rel_offset = table_start - ptr_size
    offset = lib.va_to_offset(table_start - ptr_size)
    table_header = zyrox_table["sh_offset"]
    elf_data[table_header : table_header + header_size] = os.urandom(header_size)
    elf_data[offset : offset + ptr_size] = bytes(ptr_size)
    print("is_32bit:", is_32bit)
    if is_32bit:
        _r = range(0, table_size, 2)
    else:
        _r = range(table_size)
    rel_patch_set = False
    index = 0
    for i in _r:
        dest = i * ptr_size + table_start
        rel_entry_sig = dest.to_bytes(ptr_size, "little") + rel_sig.to_bytes(
            ptr_size, byteorder="little"
        )
        entry_relocation = find_relocation_entry_offset(rel_entry_sig)
        if entry_relocation is None:
            print(
                f"no relocation found for entry at file offset: 0x{lib.va_to_offset(dest):X}, va: 0x{dest:X}"
            )
            exit()
        # now we have relocation file offset >.<
        if is_32bit:
            target = read_ptr(lib.va_to_offset(dest))
        else:
            target = read_ptr(entry_relocation + ptr_size * 2)

        block_info = table_info[index]
        index += 1

        key = block_info["key"]
        delta = block_info["delta"]
        rounds = block_info["rounds"]

        mask = 0xFFFFFFFF

        low = target & mask
        high = (target >> 32) & mask

        v0, v1 = xtea_encipher([low, high], key, rounds, delta)

        new = ((v1 & mask) << 32) | (v0 & mask)

        print("encrypted:", hex(new), "at:", hex(dest), "targeting:", hex(target))
        print(f'    key: {" ".join([hex(i) for i in key])}')
        print(f"    delta={hex(delta)}, rounds={rounds}")

        dest_offset = lib.va_to_offset(dest)

        # inject encrypted data in .data section
        elf_data[dest_offset : dest_offset + ptr_size * (2 if is_32bit else 1)] = (
            new.to_bytes(8, "little")
        )

        # patch relocation destination to 0x0, emit our debug encrypt info
        if not is_32bit:
            elf_data[
                entry_relocation + ptr_size * 2 : entry_relocation + ptr_size * 3
            ] = bytes(
                ptr_size
            )  # patch relocator pointer
        if not rel_patch_set:
            elf_data[entry_relocation : entry_relocation + ptr_size] = (
                rel_offset.to_bytes(ptr_size, byteorder="little")
            )
            rel_patch_set = True
        else:
            elf_data[entry_relocation : entry_relocation + ptr_size] = (
                rel_offset - ptr_size
            ).to_bytes(ptr_size, byteorder="little")

with open(out_file, "wb") as elf_out:
    elf_out.write(elf_data)
