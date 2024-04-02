CC=gcc
CFFLAGS=-Wall
EXEC=jsh

all: $(EXEC)

%.o: %.c
	$(CC) $(CFFLAGS) -c $<

jsh: jsh.o analys.o job.o foncAnnexes.o
	$(CC) $(CFFLAGS) -o jsh jsh.o analys.o job.o foncAnnexes.o -lreadline

clean:
	rm -rf $(EXEC) *.o

exec: $(EXEC)
	./$(EXEC)

debug: $(EXEC)
	valgrind --leak-check=full ./$(EXEC)