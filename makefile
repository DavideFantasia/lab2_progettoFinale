# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-std=c11 -g -O
LDLIBS=-lm -lrt -pthread


EXECS= archivio
OBJ = archivio.o xerrori.o

# primo target: gli eseguibili sono precondizioni

all: $(EXECS) 


# regola per la creazioni degli eseguibili utilizzando xerrori.o
%.out: %.o xerrori.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)


# regola per la creazione di file oggetto che dipendono da xerrori.h
%.o: %.c xerrori.h
	$(CC) $(CFLAGS) -c $<

archivio: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) 

 
# cancellazione dei file oggetto e degli eseguibili
clean: 
	rm -f *.o $(EXECS)

	
	
	
	
