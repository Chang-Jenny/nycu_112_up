mov eax, [0x600004]
neg eax
mov ecx, [0x600008]
cdq
idiv ecx ; remainder = edx
mov ebx, edx

mov eax, [0x600000]
imul eax, -5

cmp ebx, 0
je idiv_by_zero
cdq
idiv ebx
jmp store_result

idiv_by_zero:
mov eax, 0
store_result:
mov [0x60000c], eax