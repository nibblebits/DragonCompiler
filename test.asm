section .data
; int a
a: dd 50
section .text
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
mov eax, 50
mov [a], eax
add esp, 16
pop ebp
ret
