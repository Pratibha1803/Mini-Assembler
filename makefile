
all:
	@gcc -o pratibhassem assembler.c
	@echo "Run ./pratibhassem <inputfile.asm>"

clean:
	@rm -f pratibhassem pratibhassem.lst
