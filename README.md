# PROGETTO FINALE DI LAB2



## How to launch the program:

```
$ git clone git@github.com:DavideFantasia/lab2_progettoFinale.git <dest_dir>
$ cd <dest_dir>
$ make
$ ./server.py <maxThread> -r <number of reader threads> -w <number of writer threads> -v <launching with valgrind>  &

$ ./client2 file1 file2         # scrive dati su archivio
$ ./client1 file3               # interroga archivio
$ pkill -INT -f server.py       # invia SIGINT a server.py
                                # che a sua volta termina archivio
```

1. lancio di archivio e creazione delle pipe: ✓
2. comunicazione via pipe dal server: ✓
3. cancellazione dei file caposc, capolet: ✓
4. terminazione del server: ✓

# TODO:

### Archivio:
1. Implementare il thread dei segnali, così da poter verificare il funzionamento dell'invio segnali dal server e poter concludere il server
2. Implmentare il thread capo lettore
3. Implementare il thread capo scrittore
4. Implementazione hashmap

