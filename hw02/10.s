mov eax, [0x600000]
add eax, [0x600004]
mov ebx, [0x600008]
imul eax, ebx
mov [0x60000c], eax