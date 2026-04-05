section .data
value1 dd 11, 12
value2 dd 11, 12

section .bss
value3 resd 4
value4 resb 2

section .text
global start
start:
    mov ebx, eax       ; exit code = eax
    add ebx, eax       ; exit code = eax
    sub ebx, eax       ; exit code = eax
    mov eax, 1234567
    mov ebx, 1234567
label:    add eax, 1234567
    add ebx, 1234567
    sub eax, 1234567
    sub ebx, 1234567
    add eax, 127
    add ebx, 12
    sub eax, 17
    sub ebx, 17
    add eax, [ebx]
    sub eax, [ecx]
    mov ebx, [edx]
    add [ebx], eax
    sub [ecx], eax
    mov [edx], ebx
    add eax, [1234]
    sub eax, [12345]
    mov ebx, [3453634]
    add [1234], eax
    sub [12345], eax
    mov [3453634], ebx
	mov eax, [ebx + 1234]
	mov eax, [ebx - 1234]
	add eax, [ebx + 1234]
	add eax, [ebx - 1234]
	sub eax, [ebx + 1234]
	sub eax, [ebx - 1234]
	mov [ebx + 1234], eax
	mov [ebx - 1234], eax
	add [ebx + 1234], eax
	add [ebx - 1234], eax
	sub [ebx + 1234], eax
	sub [ebx - 1234], eax
	mov eax, [eax+ebx]
	add eax, [eax+ebx]
	sub eax, [eax+ebx]
	mov [eax+ebx], eax
	add [eax+ebx], eax
	sub [eax+ebx], eax
	mov eax, [eax+ebx*2]
	add eax, [eax+ebx*4]
	sub eax, [eax+ebx*8]
	mov [eax+ebx*1], eax
	add [eax+ebx*4], eax
	sub [eax+ebx*8], eax
	mov eax, [eax+ebx*2+32]
	add eax, [eax+ebx*4-32]
	sub eax, [eax+ebx*8+23424]
	mov [eax+ebx*1+232], eax
	add [eax+ebx*4-23], eax
	sub [eax+ebx*8+2323], eax
	cmp [eax+ebx*8+2323], eax
	mul eax
	div ebx
	inc eax
	dec ebx
	mul dword[eax]
	div dword[ebx]
	inc dword[eax]
	dec dword[ebx]
	cmp [eax+ebx*8+2323], eax
	je label
    ret
