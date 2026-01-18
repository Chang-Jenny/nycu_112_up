cmp eax, 0
jl not_var1
mov dword ptr [0x600000], 1

var2:
cmp ebx, 0
jl not_var2
mov dword ptr [0x600004], 1

var3:
cmp ecx, 0
jl not_var3
mov dword ptr [0x600008], 1

var4:
cmp edx, 0
jl not_var4
mov dword ptr [0x60000c], 1

not_var1:
mov dword ptr [0x600000], -1
jmp var2

not_var2:
mov dword ptr [0x600004], -1
jmp var3

not_var3:
mov dword ptr [0x600008], -1
jmp var4

not_var4:
mov dword ptr [0x60000c], -1