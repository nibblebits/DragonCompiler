; int array
array: times 80 db 0 
; main function
main:
push ebp
mov ebp, esp
sub esp, 16
mov eax, 3
mov [ebp-4], eax
mov eax, 10
push eax
mov ebx, [array]
mov eax, [ebp-4]
mov ecx, 4
mul ecx
add ebx, eax
pop eax
mov [ebx], eax
add esp, 16
pop ebp
ret
