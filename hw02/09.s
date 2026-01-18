; A=65 add 32 a=97
lea esi, [0x600000]
lea edi, [0x600010]
mov ecx, 15

L1:
mov al, byte ptr [esi]
cmp al, 65
jl no_convert
cmp al, 90
jg no_convert
add al, 32

no_convert:
mov byte ptr [edi], al
inc esi
inc edi
loop L1