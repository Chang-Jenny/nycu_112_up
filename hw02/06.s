mov eax, [0x600000]
not eax
add eax, 1
mov ebx, [0x600004]
sub ebx, [0x600008]
add eax, ebx
mov [0x60000c], eax