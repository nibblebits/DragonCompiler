global test

; test function
test:
push ebp
mov ebp, esp
sub esp, 16
mov eax, 30
neg eax
mov [ebp-4], eax
mov eax, [ebp-4]
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
