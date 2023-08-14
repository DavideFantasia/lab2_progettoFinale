#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>    // permette di usare scanf printf etc ...
#include <stdlib.h>   // conversioni stringa/numero exit() etc ...
#include <stdbool.h>  // gestisce tipo bool (variabili booleane)
#include <assert.h>   // permette di usare la funzione assert
#include <string.h>   // confronto/copia/etc di stringhe
#include <errno.h>
#include <search.h>
#include <signal.h>
#include <unistd.h>  // per sleep 

#define Num_elem 1000000

void handler(int s)
{
  printf("Segnale %d ricevuto dal processo %d\n", s, getpid());
}



// messaggio errore e stop
void termina(const char *messaggio){
  if(errno!=0) perror(messaggio);
	else fprintf(stderr,"%s\n", messaggio);
  exit(1);
}

// crea un oggetto di tipo entry
// con chiave s e valore n
ENTRY *crea_entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if(e==NULL) termina("errore malloc entry 1");
  e->key = strdup(s); // salva copia di s
  e->data = (int *) malloc(sizeof(int));
  if(e->key==NULL || e->data==NULL)
    termina("errore malloc entry 2");
  *((int *)e->data) = n;
  return e;
}

void distruggi_entry(ENTRY *e)
{
  free(e->key); free(e->data); free(e);
}


int main(int argc, char *argv[]) 
{
   // definisce signal handler per USR1
  struct sigaction sa;
  sa.sa_handler = &handler;
  sigfillset(&sa.sa_mask); 
  sa.sa_flags = 0;
  sigaction(SIGTERM,&sa,NULL);
 
  
  // crea tabella hash
  int ht = hcreate(Num_elem);
  if(ht==0 ) termina("Errore creazione HT");

  // inserisce gli elementi sulla linea di comando
  for(int i=1;i<argc;i++) {
    ENTRY *e = crea_entry(argv[i], 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) { // la entry è nuova
      r = hsearch(*e,ENTER);
      if(r==NULL) termina("errore o tabella piena");
    }
    else {
      // la stringa è gia' presente incremento il valore
      assert(strcmp(e->key,r->key)==0);
      int *d = (int *) r->data;
      *d +=1;
      distruggi_entry(e); // questa non la devo memorizzare
    }
  }
  // esegue l'interrogazione della linea di comando incluso argv[0] 
  for(int i=0;i<argc;i++) {
    ENTRY *e = crea_entry(argv[i], 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) printf("%s -> non trovato\n", argv[i]);
    else printf("%s -> %d\n", argv[i], *((int *) r->data));
    distruggi_entry(e);
  }
  hdestroy();
  sleep(100); // attende (ma esce quando arriva un segnale) 
  puts("main completato");
  return 0;
}
