lea edi, [0x60000F]
mov ecx, 16

L1:
test ax, 1
jz zero_bit
mov byte ptr [edi], 49
jmp next_bit

zero_bit:
mov byte ptr [edi], 48
next_bit:
shr ax, 1
dec edi
loop L1