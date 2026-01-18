lea esi, [0x600000]
mov ecx, 10

L1:
mov edi, esi
mov ecx, 9
L2:
mov eax, dword ptr [edi]
mov ebx, dword ptr [edi+4]
cmp eax, ebx
jg bubble
add edi, 4
loop L2
sub esi, 4
loop L1

jmp exit

bubble:
mov edx, eax
mov eax, ebx
mov ebx, edx
mov dword ptr [edi], eax
mov dword ptr [edi+4], ebx
jmp L2

exit: