"""tools/patches.py's SLIDE launch-height guard without game files: each patch site takes its exact guest word and
refuses a wrong opcode.

The words below are the 1.9.0 stores at 0x7000c92938 / 0x7000c9293c (task 01: `str s0, [x8, #0x20bc]` and
`str s0, [x8, #0x20c0]`). A tiny fake NSO holds just those words, so no extracted main module is needed.

Usage: uv run --no-project --with lz4 --with capstone --with unicorn python tools/test_launch_guard_site.py
"""
import struct
import sys
from types import SimpleNamespace

import patches

CHECKS = 0

# (patch name in patches.PATCHES, guest address, instruction word it must replace)
SITES = (
    ('slide_launch_clear', 0x7000c92938, 0xbd20bd00),
    ('slide_launch_clear_second', 0x7000c9293c, 0xbd20c100),
)
WRONG = 0xd503201f  # nop: a deliberately wrong opcode at the same addresses


def check(cond, what):
    global CHECKS
    if not cond:
        sys.exit(f'FAIL: {what}')
    CHECKS += 1


def fake_gen(address, word):
    """Just enough of gen for patches._word: read one little-endian word at the site."""
    return SimpleNamespace(base=address, nso=SimpleNamespace(image=struct.pack('<I', word)))


def main():
    for name, address, word in SITES:
        code = patches.PATCHES[name](fake_gen(address, word), address)
        check('rt_slide_launch_clear' in code, f'{name}: emitted C references rt_slide_launch_clear')
        check(f'{address:#x}' in code, f'{name}: emitted C marks the guest site')
        if name == 'slide_launch_clear':
            check('rt_slide_launch_clear(c->x[8], (uint32_t)c->v[0]);' in code,
                  f'{name}: passes the guest actor and s0 value to the helper')
        try:
            patches.PATCHES[name](fake_gen(address, WRONG), address)
            check(False, f'{name}: a wrong opcode is refused')
        except AssertionError as e:
            check(hex(WRONG) in str(e), f'{name}: wrong opcode refused with its word in the message')
    print(f'ok: {CHECKS} checks')


if __name__ == '__main__':
    main()
