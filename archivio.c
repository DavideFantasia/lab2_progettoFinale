/*
Il file archivio.c deve contenere il codice C di un programma multithread che gestisce la memorizzazione di stringhe 
in una tabella hash. La tabella hash deve associare ad ogni stringa un intero;
le operazioni che devono essere suportate dalla tabella hash sono:
    void aggiungi(char *s): 
        se la stringa s non è contenuta nella tabella hash deve essere inserita con valore associato uguale a 1.
        Se s è già contenuta nella tabella allora l'intero associato deve essere incrementato di 1.

    int conta(char *s): 
        restituisce l'intero associato ad s se è contenuta nella tabella, altrimenti 0.

Le operazioni sulla tabella hash devono essere svolte utilizzando le funzioni descritte su man hsearch. 
Si veda il sorgente main.c per un esempio. Si noti che la tabella hash è mantenuta dal sistema in una sorta di variabile globale
(infatti ne può esistere soltanto una).
Il programma archivio riceve sulla linea di comando due interi che indicano il numero w di thread scrittori
(che eseguono solo l'operazione aggiungi), e il numero r di thread lettori (che eseguono solo l'operazione conta). 
L'accesso concorrente di lettori e scrittori alla hash table deve essere fatto utilizzando le condition variables
usando lo schema che favorisce i lettori visto nella lezione 40 (o un altro schema più equo a vostra scelta).

Oltre ai thread lettori e scrittori, il programma archivio deve avere:

    un thread "capo scrittore" che distribuisce il lavoro ai thread scrittori mediante il paradigma produttore/consumatori

    un thread "capo lettore" che distribuisce il lavoro ai thread lettori mediante il paradigma produttore/consumatori

    un thread che gestisce i segnali mediante la funzione sigwait()
*/

#include "xerrori.h"

int main(int argc, char *argv[]){
    printf("=== Avvio corretto dell'archivio ===\n");

    if(argc!=3) termina("\nargomenti insufficienti\n");

    printf("\n\targomenti in arrivo ad %s:\n\t -r: %d\t-w: %d\n",argv[0], atoi(argv[1]),atoi(argv[2]));
    return 0;
}