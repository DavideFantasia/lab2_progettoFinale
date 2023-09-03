## Indice
1. [Overview](#progetto-finale-del-corso-di-laboratorio-2)
2. [Eseguire il progetto](#eseguire-il-progetto)
3. [Server](#serverpy)
    * 3.1. [protocollo di rete](#protocollo-di-rete)
    * 3.2. [connessione A](#connessioni-di-tipo-a)
    * 3.3. [connessione B](#connessioni-di-tipo-b)
    * 3.4. [spegnimento](#spegnimento)
4. [Archivio](#archivioc)
    - 4.1. [parametri](#parametri)
    - 4.2. [capolettore](#capolettore)
    - 4.3. [caposcrittore](#caposcrittore)
    - 4.4. [signal thread](#thread-dei-segnali)
    - 4.5. [capolettore](#capolettore)
    - 4.6. [caposcrittore](#caposcrittore)
5. [scelte implementative](#scelte-implementative)

***

# Progetto finale del corso di laboratorio 2

Questo progetto di fine anno prevede un server scritto in python che gestisce due tipi di connessione (visualizzabili nei file _client1_ e _client2_) che passano al server una serie di stringhe, 
il server dovrà passare le stringhe all'eseguibile 'archivio' scritto in C che ha lo scopo di conservare tali stringhe in una hashmap.

## Eseguire il progetto

Per eseguire il progetto è necessario clonare questa repository in ambiente linux con il seguente comando

```shell
    $ git clone git@github.com:DavideFantasia/lab2_progettoFinale.git testdir
```

Una volta clonata la repository bisogna spostarsi nella cartella di destinazione scelta ed eseguire il comando **make** per compilare il progetto

```shell
    $ cd testdir 
    $ make
```

Il file _makefile_ che fornisce il comando make fornisce anche altri comandi quali il

- **make clean** : pulisce gli eseguibili e i file di log creati, elimandoli
- **make test** : compila ed esegue secondo indicazioni il progetto

⚠️ il comando make test potrebbe non funzionare bene in base all'ambiente utilizzato ⚠️

I seguenti comando sono quelli eseguiti dal comando **make test**

```shell
    $ ./server.py 5 -r 2 -w 4 -v &  # parte il server con 5 thread che 
                                    # a sua volta fa partire archivio
    $ ./client2 file1 file2         # scrive dati su archivio
    $ ./client1 file3               # interroga archivio
    $ pkill -INT -f server.py       # invia SIGINT a server.py
                                    # che a sua volta termina archivio
```

producono come risultati:
- **stdout** : ```nella tabella hash ci sono 2010 stringhe distinte```
- **server.log**: tipo di connessione in ingresso e numero di byte scritti al server
- **lettori.log** :
  - ```
    abbacchiamento 10
    abbacchiato 10
    abbacchiare 10
    zolla 0
    abaco 130
    welcome 1
    abaco 130
    bolla 0
    abbacinare 10
    ```

## server.py

Il server ha lo scopo di mettersi in ascolto dei client comunicando poi i dati ricevuti al file archivio. I parametri da passare al server sono:

```shell
    ./server.py <maxThreads> -r <numLettori> -w <numScrittori> -v -a <host> -p <port>
```

- ***\<maxThreads>***: è il numero massimo di thread creabili dal server, richiede interi maggiori di zero, è l'unico parametro **obbligatorio**
- ***-r \<numLettori>***: è il numero di thread creati dall'archivio in lettura, richiede interi maggiori di zero, **default**: 3 
- ***-w \<numScrittori>***: è il numero di thread creati dall'archivio in scrittura, richiede interi maggiori di zero, **default**: 3
- ***-v***: flag booleano per forzare l'avvio dell'archivio con il tool _valgrind_, che genera un file di log _valgrind-XXX.log_ dove sono visualizzabili tutte le informazioni riguardanti i leak di memoria dell'eseguibile.
- ***-a \<host>***: prende un indirizzo su cui creare il server, **default**: 127.0.0.1 (indirizzo locale)
- ***-p \<port>***: prende una porta su cui mettersi in ascolto, ***default***: 54105

### Protocollo di rete

Il protocollo di rete scelto per la comunicazione client/server è un classico protocollo di internet socket (__Internet Protocol v4 addresses__) di tipo TCP.
Il server apre quindi un socket TCP e si mette in ascolto di connessioni.

Ci sono due tipi di connessione, entrambe di tipo TCP a protocollo IPV4: 

### Connessioni di tipo A
Le connessioni di tipo A mandano una query al server che interrogherà l'hashtable, restituendo il risultato nel file lettori.log. Il risultato sarà nel formato: '`<string_to_search> <occurrences>\n`'.

Fra i file a disposizione nel progetto, il file _client1_ manda connesssioni di tipo *A* al server, prendendo come parametri:
- ***\<fileName>***: path di un file, contente le stringhe di cui verranno verificate le occorrenze nell'archivio
- ***-a \<host>***: prende un indirizzo a cui mandare dati, **default**: 127.0.0.1 (indirizzo locale)
- ***-p \<port>***: prende una porta a cui mandare dati, ***default***: 54105

Il client1 invierà al server come prima cosa un socket contenente il carattere 'A' e poi la lunghezza della stringa da mandare, seguita dalla stringa stessa, in questo modo il server può smistare la connessione verso un hadler adatto e leggere la stringa sapengo già la lunghezza. Inoltrerà queste informazioni sulla FIFO capolet.

Al bisogno di inviare più righe, il client chiuderà la connessione e la riavvierà per mandare un altra riga.

### Connessioni di tipo B
Le connessioni di tipo B interagiscono con l'hashtable inserendo nuovi elementi o aumentando le occorrenze se già presenti.  
Fra i file a disposizione nel progetto, il file _client2_ manda connesssioni di tipo *B* al server, prendendo come parametri:
- ***\<fileNames>***: path di un numero arbitrario di file, contenti le stringhe che verranno inserite nell'archivio
- ***-a \<host>***: prende un indirizzo a cui mandare dati, **default**: 127.0.0.1 (indirizzo locale)
- ***-p \<port>***: prende una porta a cui mandare dati, ***default***: 54105

Come il client1, il client2 invierà al server come prima cosa un socket contenente il carattere 'B' e poi la lunghezza della stringa da mandare, seguita dalla stringa stessa, in questo modo il server può smistare la connessione verso un hadler adatto e leggere la stringa sapengo già la lunghezza. Inoltrerà queste informazioni sulla FIFO caposc.

All'occorrenza, la connessione verrà chiusa quando un file verrà mandato completamente (riga per riga) e riaperta per inviare gli altri file nella lista.

### Spegnimento

Il modo più pulito e meglio gestito di spegnere il server e avviare quindi una fase di chiusura e pulizia interna è quello di mandare un segnale di SIGINT al server, usando la shortcut CTRL+C oppure usando il comando:
```shell
    pkill -INT -f server.py
```
Questo, una volta finita tutta la procedura di chiusura delle connessioni, delle PIPE, etc; stamperà su stdout il numero di stringhe distinte presenti nell'hashtable

***

## archivio.c

L'archivio ha lo scopo di leggere dalle FIFO `capolet` e `caposc` le stringhe e di spartire il lavoro attraverso uno schema produttore-consumatore per interrogare e interagire con l'hashtable.

### parametri

```shell
    ./archivio <r_num> <w_num>
```
Dove
- ***<r_num>*** : intero positivo che rappresenta il numero di thread consumatori (slave) che interrogano la hashtable
- ***<w_num>*** : intero positivo che rappresenta il numero di thread consumatori (slave) che scrivono nella hashtable

L'archivio è composto quindi da
-  _r_ e _w_ thread slave
- 1 thread produttore 'caposcrittore'
- 1 thread produttore 'capolettore'
- 1 thread gestore dei segnali

#### capolettore

Il thread capolettore è il master dei thread lettori, esso è incaricato di leggere dalla FIFO le stringhe, leggendo prima un intero, che rappresenta la lunghezza della stringa e poi la stringa intera.

La linea letta dalla Pipe viene _tokenizzata_ usando la funzione rientrante `strtok_r`, i token vengono successivamente spostati su un buffer produttore-consumatore a dimensione fissata di grandezza `PC_buffer_len = 10`, la scrittura su buffer è gestita da una funzione apposita, così come la lettura, reperibile nella libreria _utility_.
Le interazione con la hashtable verranno eseguite dai thread consumatori una volta letti i dati dal buffer

Il thread rimane attivo fintanto che la FIFO è aperta in scrittura e quindi fintanto che il server è attivo.

#### caposcrittore

Lavora in modo analogo al thread capolettore, ma legge dalla FIFO caposc e scrive su un buffer produttore-consumatore apposito per i thread scrittori.

#### thread dei segnali

Tutti i thread dell'archivio come prima cosa bloccano i segnali in ingresso per non doverli gestire, tutti apparte questo thread pensato appositamente per gestire in solitaria tutti i segnali.

Questo thread entra in attesa di segnali tramite `sigwait()` e gestisce in modo standard tutti i segnali che non sono _SIGINT_ e _SIGTERM_.
- SIGINT: stampa su ___stderr___ il numero attuale di stringhe distinte nella hashtable
- SIGTERM: segnale terminatore, attende la terminazione dei thread capi, stampa su ___stdout___ il numero di stringhe distinte e distrugge la hashtable, terminando, avviando così la fase di pulizia pre-chiusura dell'archivio

#### thread consumatori

- ***lettori***: leggono i token dal buffer produttori-consumatori per poi chiamare la funzione `conta()` per contare le occorrenze di una stringa nell'hashtable e stamparla sul file _lettori.log_. Questi thread terminano quando leggono la stringa **EOF**, usata come carattere terminatore dal thread produttore una volta chiusa la FIFO in scrittura

- ***scrittori***: leggono i token dal buffer produttori-consumatori per poi chiamare la funzione `aggiungi()`. Questi thread terminano quando leggono la stringa **EOF**, usata come carattere terminatore dal thread produttore una volta chiusa la FIFO in scrittura

## Scelte implementative

Di seguito, tutte le decisioni riguardanti la stesura del codice che meritano ulteriori spiegazioni.

- **accesso alla hashtable tramite mutex**: Per evitare di introdurre nel codice un altra struttura di sincronizzazione, complicando ancora una volta la struttura del progetto, si è preferito usare una struttura già introdotta, ovvero un mutex; in modo da accedere alla hashtable in maniera esclusiva sia in scrittura che in lettura. Questa modifica non influisce in maniera incisiva sull'ottimizzazione del codice rispetto all'uso di una conditional variable e per questo l'ho ritenuta una valida alternativa.

- **utility.c**: Per aumentare la leggibilità dell'infinito codice si è preferito separare le funzioni di appoggio dal codice principale, tutte le variabili globali, le struct e i prototipi di funzione sono stati quindi tolti dal file principale, tranne le funzioni ritenute 'principali' quali: 
    - `void aggiungi(char *s);`
    - `int conta(char *s);`
    - tutti i corpi dei thread

- **PTHREAD_MUTEX_INITIALIZER**: tutti i mutex sono stati inizializzati in modo statico per poter non doverli liberare a mano, riducendo così il numero di righe di codice e il carico mentale necessario per comprenderlo.

- **contatore globale di stringhe distinte e altre var. globali**: sebbene non sia una scelta implementativa che garantisce la signal-safeness, si è scelto di usare un contatore globale per diminuire il più possibile il numero di argomenti passati ai thread, in un tentativo di aumentare la leggibilità del codice. Per garantire un corretto funzionamento del tutto si è usato un mutex per potere accedere in maniera esclusiva (sia in lettura che in scrittura) al contatore globale e il contatore stesso è stato definito come `volatile sig_atomic_t`.

- **camelCase vs snake_case**: In un ottica di voler migliorare la leggibilità del progetto, mi sono ritrovato a circa 1/3 del progetto a switchare da una sintassi 'camelCase' ad una 'snake_case', dopo aver notato quando la 'snake_case' aiutasse la comprensione del testo, essendo che si omologava anche meglio alla sintassi della libreria '_xerrori.h_', mi scuso quindi per questo switch di sintassi notabile fra il server e l'archivio. 
