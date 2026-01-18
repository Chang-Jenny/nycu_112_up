mov eax, [0x600000]
mov edx, [0x600004]
neg edx
imul edx

mov ecx, [0x600008]
sub ecx, ebx
cmp ecx, 0
je idiv_by_zero

cdq
idiv ecx
jmp store_result

idiv_by_zero:
mov eax, 0

store_result:
mov [0x600008], eax