CC=gcc
CFLAGS=-I.
DEPS = parse.h y.tab.h log.h process_request.h
OBJ = y.tab.o lex.yy.o parse.o log.o process_request.o lisod.o
FLAGS = -g -Wall

default:lisod

lex.yy.c: lexer.l
	flex $^

y.tab.c: parser.y
	yacc -d $^

%.o: %.c $(DEPS)
	$(CC) $(FLAGS) -c -o $@ $< $(CFLAGS)

lisod: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *~ *.o lisod log process_request lex.yy y.tab