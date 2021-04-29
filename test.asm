global test
extern _main

; test function
test:
push ebp
mov ebp, esp
mov eax, 70
push eax
mov eax, 20
push eax
mov eax, [ebp+8]
pop ecx
xchg ecx, eax
add eax, ecx
pop ecx
xchg ecx, eax
add eax, ecx
mov [ebp+8], eax
pop ebp
ret
