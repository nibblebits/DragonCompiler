global _start

section .data
section .text
_start:
    jmp main
    
; test function
test:
push ebp
mov ebp, esp
mov eax, [ebp+8]
pop ebp
ret
pop ebp
ret
; main function
main:
push ebp
mov ebp, esp
lea ebx, [test]
mov eax, 50
PUSH eax
mov eax, 60
PUSH eax
call [ebx]
add esp, 8
pop ebp
ret

