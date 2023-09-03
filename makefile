# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-std=c11 -g -O
LDLIBS=-lm -lrt -pthread


EXECS= archivio
OBJ = archivio.o xerrori.o utility.o

# primo target: gli eseguibili sono precondizioni

all: $(EXECS) 


# regola per la creazioni degli eseguibili utilizzando xerrori.o
%.out: %.o xerrori.o utility.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)


# regola per la creazione di file oggetto che dipendono da xerrori.h
%.o: %.c xerrori.h utility.h
	$(CC) $(CFLAGS) -c $<

archivio: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) 

 
# cancellazione dei file oggetto e degli eseguibili
clean: 
	rm -f *.o *.log $(EXECS)

test:
	./server.py 5 -r 2 -w 4 -v &
	timeout 3                             
	./client2 file1 file2         
	timeout 3
	./client1 file3               
	timeout 3
	pkill -INT -f server.py  

	
	
	
	
