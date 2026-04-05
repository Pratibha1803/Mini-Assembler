# Mini-Assembler

- Developed a lightweight mini assembler in C for Intel 32-bit architecture, demonstrating low-level programming and machine code execution. 
- Implemented direct opcode handling and execution via function pointer casting, showcasing systems programming and hardware-level understanding.

## Minimal two-pass x86-32 NASM-like assembler producing a listing file.
Supports: sections (.data/.bss/.text), dd/resd/resb, labels, global,
mov/add/sub/cmp/mul/div/inc/dec/je/ret with full r/m32 memory operand forms:
  - r32, [r/m32]  (e.g., add eax, [ebx]; sub eax, [ecx]; cmp eax, [mem])
  - [r/m32], r32
  - r/m32, r32
  - r/m32, imm32
  - [disp32] absolute
  - Optional "dword" size specifier before memory (defaults to dword)

-------------------------------------------------------------------------

### Run :

make

./pratibhassem input.asm

-------------------------------------------------------------------------

### Clean :

make clean

-------------------------------------------------------------------------

### Example : 

user/myassembler$ make
Run ./pratibhassem <inputfile.asm>

user/myassembler$ ./pratibhassem input.asm 

section .data
00000000 0B0000000C000000        value1 dd 11, 12

00000008 0B0000000C000000        value2 dd 11, 12


section .bss
00000000 <res 00000000h>

section .text
00000000         section .text

00000000         global start

00000000         start:

00000000 89C3            mov ebx, eax       ; exit code = eax

00000002 01C3            add ebx, eax       ; exit code = eax

00000004 29C3            sub ebx, eax       ; exit code = eax

00000006 B887D61200            mov eax, 1234567

0000000B BB87D61200            mov ebx, 1234567

00000010 0587D61200        label:    add eax, 1234567

00000015 81C387D61200            add ebx, 1234567

0000001B 2D87D61200            sub eax, 1234567

00000020 81EB87D61200            sub ebx, 1234567

00000026 83C07F            add eax, 127

00000029 83C30C            add ebx, 12

0000002C 83E811            sub eax, 17

0000002F 83EB11            sub ebx, 17

00000032 0303            add eax, [ebx]

00000034 2B01            sub eax, [ecx]

00000036 8B1A            mov ebx, [edx]

00000038 0103            add [ebx], eax

0000003A 2901            sub [ecx], eax

0000003C 891A            mov [edx], ebx

0000003E 0305D2040000            add eax, [1234]

00000044 2B0539300000            sub eax, [12345]

0000004A 8B1DC2B23400            mov ebx, [3453634]

00000050 0105D2040000            add [1234], eax

00000056 290539300000            sub [12345], eax

0000005C 891DC2B23400            mov [3453634], ebx

00000062 8B83D2040000        	mov eax, [ebx + 1234]

00000068 8B832EFBFFFF        	mov eax, [ebx - 1234]

0000006E 0383D2040000        	add eax, [ebx + 1234]

00000074 03832EFBFFFF        	add eax, [ebx - 1234]

0000007A 2B83D2040000        	sub eax, [ebx + 1234]

00000080 2B832EFBFFFF        	sub eax, [ebx - 1234]

00000086 8983D2040000        	mov [ebx + 1234], eax

0000008C 89832EFBFFFF        	mov [ebx - 1234], eax

00000092 0183D2040000        	add [ebx + 1234], eax

00000098 01832EFBFFFF        	add [ebx - 1234], eax

0000009E 2983D2040000        	sub [ebx + 1234], eax

000000A4 29832EFBFFFF        	sub [ebx - 1234], eax

000000AA 8B0418        	mov eax, [eax+ebx]

000000AD 030418        	add eax, [eax+ebx]

000000B0 2B0418        	sub eax, [eax+ebx]

000000B3 890418        	mov [eax+ebx], eax

000000B6 010418        	add [eax+ebx], eax

000000B9 290418        	sub [eax+ebx], eax

000000BC 8B0458        	mov eax, [eax+ebx*2]

000000BF 030498        	add eax, [eax+ebx*4]

000000C2 2B04D8        	sub eax, [eax+ebx*8]

000000C5 890418        	mov [eax+ebx*1], eax

000000C8 010498        	add [eax+ebx*4], eax

000000CB 2904D8        	sub [eax+ebx*8], eax

000000CE 8B445820        	mov eax, [eax+ebx*2+32]

000000D2 034498E0        	add eax, [eax+ebx*4-32]

000000D6 2B84D8805B0000        	sub eax, [eax+ebx*8+23424]

000000DD 898418E8000000        	mov [eax+ebx*1+232], eax

000000E4 014498E9        	add [eax+ebx*4-23], eax

000000E8 2984D813090000        	sub [eax+ebx*8+2323], eax

000000EF 3984D813090000        	cmp [eax+ebx*8+2323], eax

000000F6 F7E0        	mul eax

000000F8 F7F3        	div ebx

000000FA 40        	inc eax

000000FB 4B        	dec ebx

000000FC F720        	mul dword[eax]

000000FE F733        	div dword[ebx]

00000100 FF00        	inc dword[eax]

00000102 FF0B        	dec dword[ebx]

00000104 3984D813090000        	cmp [eax+ebx*8+2323], eax

0000010B 0F8400000000        	je label
label
00000111 C3            ret

