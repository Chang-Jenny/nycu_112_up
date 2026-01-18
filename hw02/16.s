; 26 = 2^4 + 2+3 + 2^1
mov eax, [0x600000]
shl eax, 4
mov ebx, [0x600000]
shl ebx, 3
mov ecx, [0x600000]
shl ecx, 1
add eax, ebx
add eax, ecx
mov [0x600004], eax