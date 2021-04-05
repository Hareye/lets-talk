CFLAGS = -pthread -o lets-talk

default: lets-talk

lets-talk: lets-talk.o list.o
	gcc -g $(CFLAGS) lets-talk.o list.o

lets-talk.o: lets-talk.c list.c
	gcc -g -c lets-talk.c

list.o: list.c
	gcc -g -c list.c

valgrind:
	valgrind ./lets-talk 8000 localhost 8001

clean:
	rm -f lets-talk *.o