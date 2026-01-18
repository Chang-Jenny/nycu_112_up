mov eax, [0x600000]
not eax
add eax, 1
imul eax, [0x600004]
mov ebx, [0x600008]
add eax, ebx