# PROGETTO FINALE DI LAB2



## How to launch the program:

```
$ git clone git@github.com:user/progetto.git <dir>
$ cd <dir>
$ make
$ ./server.py <maxThread> -r <number of reader threads> -w <number of writer threads> -v <launching with valgrind>  &

$ ./client2 file1 file2         # scrive dati su archivio
$ ./client1 file3               # interroga archivio
$ pkill -INT -f server.py       # invia SIGINT a server.py
                                # che a sua volta termina archivio
```

### TODO:

bisogna ancora implementare l'archivio e il sistema di comunicazione via pipe
