assembler.c — minimal two-pass x86-32 NASM-like assembler producing a listing file.
Supports: sections (.data/.bss/.text), dd/resd/resb, labels, global,
mov/add/sub/cmp/mul/div/inc/dec/je/ret with full r/m32 memory operand forms:
  - r32, [r/m32]  (e.g., add eax, [ebx]; sub eax, [ecx]; cmp eax, [mem])
  - [r/m32], r32
  - r/m32, r32
  - r/m32, imm32
  - [disp32] absolute
  - Optional "dword" size specifier before memory (defaults to dword)

-------------------------------------------------------------------------

Run :

make
./pratibhassem input.asm

-------------------------------------------------------------------------

Clean :

make clean
