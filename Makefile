pac_man : main.o gestoreGioco.o fantasma.o pacman.o missile.o
	gcc main.o gestoreGioco.o fantasma.o pacman.o missile.o -o pac_man -lncurses
	
main.o : main.c
	gcc -c main.c -o main.o

gestoreGioco.o : gestoreGioco.c gestoreGioco.h
	gcc -c gestoreGioco.c -o gestoreGioco.o

fantasma.o : fantasma.c fantasma.h
	gcc -c fantasma.c -o fantasma.o

pacman.o : pacman.c pacman.h
	gcc -c pacman.c -o pacman.o
	
missile.o : missile.c missile.h
	gcc -c missile.c -o missile.o
clean : 
	rm -f *.o
