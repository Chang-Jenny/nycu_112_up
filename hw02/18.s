mov rax, 1
mov rbx, 0
mov rdx, 2

L:
mov rcx, rax

add rax, rax

add rax, rbx
add rax, rbx
add rax, rbx


mov rbx, rcx
inc rdx
cmp rdx, 23
jle L

; implement r(n) = 2*r(n-1) + 3*r(n-2), n=4
; r(0) = 0, r(1) = 1
; mov rax, 3
; call r
; jmp exit
; r:

; cmp rax, 0
; jle r_zero
; cmp rax, 1
; je r_one

; r(n-1)
; dec rax
; push rax
; call r
; pop rax
; mov rbx, rax

; r(n-2)
; dec rax
; push rax
; call r
; pop rax
; mov rcx, rax
; add rax, rbx
; add rax, rbx
; add rax, rcx
; add rax, rcx
; add rax, rcx
; ret

; r_one:
; mov rax, 1
; ret
; r_zero:
; xor rax, rax
; ret

; exit:
