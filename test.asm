global test

; test function
test:
push ebp
mov ebp, esp
sub esp, 16
mov eax, 90
mov [ebp-4], eax
mov eax, [ebp-4]
push eax
mov eax, [ebp+12]
pop ecx
xchg ecx, eax
add eax, ecx
add esp, 16
pop ebp
ret
add esp, 16
pop ebp
ret
