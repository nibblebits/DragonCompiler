section .data
; int z
z: dd 10
; char a
a: times 20 db 0 
section .text
; main function
main:
push ebp
mov ebp, esp
mov eax, [z]
mov eax, [a+eax]
mov dword [z], eax
pop ebp
ret
