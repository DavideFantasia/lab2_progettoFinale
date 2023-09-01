#define _POSIX_C_SOURCE 200809L //per il  dprintf()
#define _GNU_SOURCE
#include <stdio.h>
#include <search.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include "xerrori.h"
#include <arpa/inet.h>


#define INITIAL_TABLE_SIZE 1000000
#define PC_buffer_len 10
int tot = 0; //parole distinte totali

struct listaCircolare 
{
    pthread_mutex_t *lock; //lock per poter scrivere/leggere il buffer
    pthread_cond_t *not_full;
    pthread_cond_t *not_empty;
    int *consumatore; //indice thread consumatori
    int *produttore; //indice thread produttori
    int *disponibili; //dati presenti sul buffer non ancora letti
    char **array;
};

//termina un processo con eventuale messaggio d'errore
void termina(const char *messaggio) 
{
  if(errno==0)  fprintf(stderr,"== %d == %s\n",getpid(), messaggio);
  else fprintf(stderr,"== %d == %s: %s\n",getpid(), messaggio,
              strerror(errno));
  exit(1);
}


struct segnale
{
    sigset_t *segnali;
    pthread_t *caposcrittore;
    pthread_t *capolettore;
};



struct reader
{
    int *file_descriptor;
    struct listaCircolare *bufferslave;
};

struct writer
{
    struct listaCircolare *bufferslave;
};

struct master
{
    struct listaCircolare *buffermaster;
    char *name;
};

void distruggi_entry(ENTRY *e)
{
  free(e->key); 
  free(e->data);
  free(e);
}


// crea un oggetto di tipo entry
// con chiave s e valore n
ENTRY *crea_entry(char *stringa, int n)
{
  ENTRY *e = calloc(1, sizeof(ENTRY));
  if(e==NULL) termina("errore creazione");
  e->data = (int *) calloc(1, sizeof(int)); //casto ad intero il puntatotore che mi indica dove trovo la memoria allocata (di dimensione di un intero), così posso manipolare il valore al suo interno
  e->key = strdup(stringa); //alloca automaticamente la memoria necessaria per la nuova stringa, evitando la necessità di allocare manualmente la memoria utilizzando malloc e copiare la stringa con strcpy.
  if(e->key==NULL || e->data==NULL) termina("errore malloc");
  *((int *)e->data) = n;
  return e;
}


void aggiungi(char *s)
{
    ENTRY *e = crea_entry(s, 1);
    ENTRY *presente = hsearch(*e, FIND);
    if (presente == NULL)
    {
        presente = hsearch(*e,ENTER);
        if(presente==NULL) termina("tabella piena o errore");
        tot ++; //aumento il contatore globale
    }
    else
    {
        assert(strcmp(e->key,presente->key)==0);
        int *n = (int *)presente->data;
        *n += 1; //aumenta il contatore interno della entry
        distruggi_entry(e); //non la devo memorizzare
    }
}


int conta(char *s)
{ 
    ENTRY *e = crea_entry(s, 1);
    ENTRY *r = hsearch(*e,FIND);
    distruggi_entry(e);
    if(r==NULL) return 0; // se la stringa cercata non è presente 
    else return *((int *)r->data);
}


void *lettore(void *arg)
{
    struct reader *argv = (struct reader *)arg; //passo il puntatore alla struttura
    struct listaCircolare *buf = (argv->bufferslave);
    int *log_fd = (argv->file_descriptor);
    while (1) 
    {
        pthread_mutex_lock(buf->lock); //prende la lock
        while (*(buf->disponibili )== 0)
        {   
            pthread_cond_wait(buf->not_empty, buf->lock); //se non ho inserito elementi sul buffer, aspetto di ricevere not_empty
        }        
        char *str  = buf->array[*(buf->consumatore)];
        if(str == NULL) //devo terminare
        {
            pthread_cond_signal(buf->not_empty); //sveglia un altro thread lettore
            pthread_mutex_unlock(buf->lock);
            break;
        } 
        dprintf(*log_fd, "%s %d\n", str, conta(str)); //scrivo sul file di log degli scrittori
        buf->array[*(buf->consumatore)] = NULL; // Imposta l'elemento dell'array a NULL per indicare che non è più valido
        free(str);
        *(buf->consumatore)= *(buf->consumatore)+1;
        *(buf->disponibili) = *(buf->disponibili)-1; //Un elemento in meno da leggere
        if (*(buf->consumatore) > 9) *(buf->consumatore)= 0;
        pthread_cond_signal(buf->not_full); //Viene inviato un segnale al thread produttore
        pthread_mutex_unlock(buf->lock); //lascia la lock
    }
    return NULL;
}


void *scrittore(void *arg)
{
    struct writer *argv = (struct writer *)arg; //passo il puntatore alla struttura
    struct listaCircolare *buf = (argv->bufferslave);
    while (1)
    {
        pthread_mutex_lock(buf->lock); //prende la lock
        while (*(buf->disponibili )== 0 ) 
        {
            pthread_cond_wait(buf->not_empty, buf->lock); //se non sono stati inseriti elementi sul buffer, aspetta di ricevere not_empty
        }
        char *str  = buf->array[*(buf->consumatore)];
        if(str == NULL)
        {
            pthread_cond_signal(buf->not_empty); //sveglia un altro thread scrittore
            pthread_mutex_unlock(buf->lock);
            break;
        } 
        aggiungi(str);
        buf->array[*(buf->consumatore)] = NULL; // Imposta l'elemento dell'array a NULL per indicare che non è più valido
        free(str);
        *(buf->consumatore)= *(buf->consumatore)+1 ;
        *(buf->disponibili) = *(buf->disponibili)-1;
        if (*(buf->consumatore) > 9) *(buf->consumatore)= 0;
        pthread_cond_signal(buf->not_full);
        pthread_mutex_unlock(buf->lock);
    }
    return NULL;
}


void *capo(void *arg)
{
    struct master *argv = (struct master *)arg; //passo il puntatore alla struttura
    char *file = argv->name;
    struct listaCircolare *buf = (argv->buffermaster);
    int pipe;
    pipe = open(file, O_RDONLY); //apre la pipe
    if (pipe == -1) {
        perror("Errore nell'apertura della pipe");
        exit (1);
    }
    char b_letti[2048];
    int n = 0;
    int leggo=0;
    while((n =read(pipe, b_letti, sizeof(uint16_t))) > 0)
    {
        uint16_t lunghezza;
        memcpy(&lunghezza, b_letti, sizeof(uint16_t)); //nei primi due byte metto la lunghezza
        if (ntohs(lunghezza) > 2048)  break;
        leggo = read(pipe, b_letti,  ntohs(lunghezza)); //leggo nbytes dalla pipe 
        if (leggo ==1) continue; //non c'è nulla da scrivere
        if (leggo <= 0) break; //ho finito di leggere
        char *copia;
        copia = calloc((lunghezza +1) , sizeof(char));
        if (!copia) break ;// Errore: memoria insufficiente
        memcpy(copia, b_letti, ntohs(lunghezza)); 
        copia[lunghezza] = '\0'; 
        char *token = NULL;
        char *saveptr = NULL;
        token = strtok_r(copia, ".,:; \n\r\t", &saveptr); //Inizia a tokenizzare la riga ricevuta
        while(token != NULL)
        {
            pthread_mutex_lock(buf->lock); 
            while(*(buf->disponibili) == 10) 
            {
                pthread_cond_wait(buf->not_full, buf->lock);
            }
            char *copia = strdup(token);
            buf->array[*(buf->produttore)] = copia; //inserisco nel buffer
            token = strtok_r(NULL, ".,:; \n\r\t", &saveptr);
            *(buf->produttore)= *(buf->produttore)+1;
            *(buf->disponibili) = *(buf->disponibili)+1;
            if (*(buf->produttore) > 9) *(buf->produttore)= 0;
            pthread_cond_signal(buf->not_empty);
            pthread_mutex_unlock(buf->lock);  
        }
        token=NULL;
        free(copia);
        memset(b_letti, '\0', sizeof(b_letti));
    }
    pthread_mutex_lock(buf->lock);  
    for(int i = 0; i<10; i++)
    {
        buf->array[i] = NULL; //Avverte i thread consumatori di finire
    }
    *(buf->disponibili) = 10;
    pthread_cond_signal(buf->not_empty);
    pthread_mutex_unlock(buf->lock);  
    close(pipe); //chiudo la pipe
    return NULL;
}


void *signal_handler(void *arg)
{
    struct segnale *argv = (struct segnale *)arg;
    sigset_t *set = (sigset_t *)argv->segnali;
    int sig;
    pthread_t *caposcrittore = argv->caposcrittore;
    pthread_t *capolettore = argv->capolettore;
    while (1)
    {
        sigwait(set, &sig);
        if (sig == SIGINT)
        {
            fprintf(stderr, "Segnale SIGINT -> Stringhe distinte nella tabella hash: %d\n", tot);
        }
        else if (sig == SIGTERM)
        {
            //join capo lettore e join caposcrittore
            pthread_join(*capolettore, NULL);
            pthread_join(*caposcrittore, NULL);
            fprintf(stdout, "Segnale SIGTERM -> Stringhe distinte nella tabella hash: %d\n", tot);
            break;
        }
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Uso: %s <num_scrittori> <num_lettori>", argv[0]);
        return 1;
    }
    //Inizializzo 
    int num_writers = atoi(argv[1]);
    int num_readers = atoi(argv[2]);
    pthread_mutex_t msc= PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mlett = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t nfl = PTHREAD_COND_INITIALIZER;
    pthread_cond_t nel = PTHREAD_COND_INITIALIZER;
    pthread_cond_t nfs = PTHREAD_COND_INITIALIZER;
    pthread_cond_t nes = PTHREAD_COND_INITIALIZER;
    char **Buflett = (char **)calloc(10, sizeof(char *));
    char **Bufsc = (char **)calloc(10, sizeof(char *));
    int disp_lett = 0;
    int disp_sc = 0; 
    int ind_con_lett=0, ind_prod_lett=0, ind_con_sc=0, ind_prod_sc = 0; 
    //Creo le due liste circolari per le comunicazioni tra i vari thread
    struct listaCircolare bufsc;
    struct listaCircolare buflet;

    buflet.consumatore= &ind_con_lett ;
    buflet.produttore=&ind_prod_lett;
    bufsc.consumatore=&ind_con_sc;
    bufsc.produttore=&ind_prod_sc;
    bufsc.array = Bufsc;
    buflet.array =  Buflett;
    buflet.disponibili = &disp_lett;
    bufsc.disponibili = &disp_sc;
    buflet.lock = &mlett;
    bufsc.lock = &msc;
    buflet.not_empty = &nel;
    buflet.not_full = &nfl;
    bufsc.not_empty = &nes;
    bufsc.not_full = &nfs;
    //Dichiaro i thread e le rispettive strutture
    pthread_t caposcrittore;
    pthread_t capolettore;
    pthread_t signal_t;
    pthread_t scrittori[num_writers];
    pthread_t lettori[num_readers];
    sigset_t segnali;
    struct reader reader_arr;
    struct writer writer_arr;
    struct master cwriter;
    struct master creader;
    int file_descriptor = open("lettori.log", O_WRONLY | O_CREAT | O_APPEND, 0644); //apro il file in scrittura, se non esiste lo creo con i permessi(nel caso dovessi creare il file) owner e gruppo può scrivere/leggere gli altri solo leggere (-rw-rw-r--)

    if (file_descriptor == -1) exit(1);
    
  
    reader_arr.file_descriptor = &file_descriptor;
    reader_arr.bufferslave = &buflet;

    writer_arr.bufferslave = &bufsc;

    cwriter.buffermaster = &bufsc;
    cwriter.name = "caposc";

    creader.buffermaster = &buflet;
    creader.name = "capolet";

    int numero_elementi = INITIAL_TABLE_SIZE;
    int esito = hcreate(numero_elementi);
    if(esito==0 ) termina("Errore creazione HT");
    //Inizializzo un set di segnali
    sigemptyset(&segnali);
    sigaddset(&segnali, SIGTERM);
    sigaddset(&segnali, SIGINT);
    struct segnale seg;
    seg.segnali = &segnali;
    seg.capolettore = &capolettore;
    seg.caposcrittore = &caposcrittore;
    //creo i vari thread
    if (pthread_sigmask(SIG_BLOCK, &segnali, NULL) != 0) //imposto una maschera dei segnali per catturare i segnali specificati nella variabile "segnali"
    {
        perror("Errore thread segnali");
        exit(1);
    }

    if (pthread_create(&signal_t, NULL, signal_handler, (void *)&seg) != 0) // Creazione del signal handler
    {
        perror("Errore nella creazione del thread dei segnali");
        exit(1);
    }


    if (pthread_create(&capolettore, NULL, capo, (void*)&creader) != 0) //creazione capo lettore
    {
        perror("Errore nella creazione del thread capolettore");
        exit(1);
    }

    if ( pthread_create(&caposcrittore, NULL, capo, (void*)&cwriter)!= 0) //creazione capo scrittore
    {
        perror("Errore nella creazione del thread caposcrittore");
        exit(1);
    }

    for (int i = 0 ; i < num_writers; i++) 
    {
        if(pthread_create(&scrittori[i], NULL, &scrittore, &writer_arr) != 0)
        { 
            perror("errore creazione scrittori");
        }
    }   
    

    for (int i = 0 ; i < num_readers; i++) 
    {
        if(pthread_create(&lettori[i], NULL, &lettore, &reader_arr) != 0)
        {
            perror("errore creazione lettori");
        }
    }

    //inizio le join 
    //join dei lettori
    for (int i = 0; i < num_readers; i++)
    {
        pthread_join(lettori[i], NULL);
    }
    //join degli scrittori
    for (int i = 0; i < num_writers; i++)
    {
        pthread_join(scrittori[i], NULL);
    }

    //join thread segnali
    pthread_join(signal_t, NULL);

    //distruggo i vari mutex
    pthread_mutex_destroy(&msc);
    pthread_mutex_destroy(&mlett);
    pthread_cond_destroy(&nfl);
    pthread_cond_destroy(&nel);
    pthread_cond_destroy(&nfs);
    pthread_cond_destroy(&nes);

    free(Buflett);
    free(Bufsc);

    //distruggo la tabella hash
    hdestroy();
    return 1;
}