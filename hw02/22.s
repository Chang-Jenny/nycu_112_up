; ul+lu: convert the alphabet in CH from upper to lower or from lower to upper
mov cl, ch

cmp cl, 'A'
jl not_convert
cmp cl, 'Z'
jg lowercase

add al, 32

lowercase:
cmp cl, 'a'
jl not_convert
cmp cl, 'z'
jg not_convert

sub cl, 32
not_convert:
mov ch, cl

