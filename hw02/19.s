mov rax, [0x600000]
mov rbx, [0x600008]
xchg rax, rbx

mov [0x600000], rax
mov [0x600008], rbx