#!/usr/bin/env python3
"""
Prepare app.toml and a trimmed app.hex for ledgerctl install.

The Ledger SDK embeds pre-built install params at the end of the hex file
(between _envram_data and _nvram_end). ledgerctl constructs and appends its
own params, so the pre-embedded ones must be stripped first.  Without this,
the data_length sent in CREATE_APP is too large and the firmware rejects it.

Usage: prepare_load.py <dir> <target_id> <icon>
  dir       - directory containing app.elf and app.hex (from the .tgz)
  target_id - hex target ID string, e.g. 0x33100004
  icon      - path to the icon PNG file
"""

import sys
import os
import shutil
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from intelhex import IntelHex

def get_symbols(elf_path):
    with open(elf_path, 'rb') as f:
        elf = ELFFile(f)
        return {
            s.name: s['st_value']
            for sec in elf.iter_sections()
            if isinstance(sec, SymbolTableSection)
            for s in sec.iter_symbols()
        }

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <dir> <target_id> <icon>")
        sys.exit(1)

    d, tid, icon_src = sys.argv[1], sys.argv[2], sys.argv[3]
    icon_name = os.path.basename(icon_src)

    syms = get_symbols(os.path.join(d, 'app.elf'))
    nvm_data    = syms['_nvram_data']    # start of NVM data section
    envram_data = syms['_envram_data']   # end of NVM data / start of embedded params
    data_size   = envram_data - nvm_data # 512 bytes typically

    # Strip the pre-embedded params from the hex so ledgerctl can append its own
    ih = IntelHex(os.path.join(d, 'app.hex'))
    ih2 = IntelHex()
    ih2.puts(ih.minaddr(), ih.tobinstr(ih.minaddr(), envram_data - 1))
    ih2.start_addr = ih.start_addr
    trimmed_hex = os.path.join(d, 'app_trimmed.hex')
    ih2.write_hex_file(trimmed_hex)

    # Copy icon into temp dir so the toml path resolves correctly
    shutil.copy(icon_src, os.path.join(d, icon_name))

    toml = f"""name = "Mavryk Wallet"
version = "1.0.0"

["{tid}"]
binary = "app_trimmed.hex"
flags = "0x800"
dataSize = {data_size}
apiLevel = 25
icon = "{icon_name}"

["{tid}".derivationPath]
curves = ["secp256k1", "prime256r1", "ed25519"]
paths = ["44'/1969'"]
"""
    with open(os.path.join(d, 'app.toml'), 'w') as f:
        f.write(toml)

    print(f"data_size={data_size}, trimmed hex ends at {hex(envram_data)}")

if __name__ == '__main__':
    main()
