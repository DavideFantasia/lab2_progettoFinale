#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>    // permette di usare scanf printf etc ...
#include <stdlib.h>   // conversioni stringa/numero exit() etc ...
#include <stdbool.h>  // gestisce tipo bool (variabili booleane)
#include <assert.h>   // permette di usare la funzione assert
#include <string.h>   // confronto/copia/etc di stringhe
#include <errno.h>
#include <search.h>


#define Num_elem 1000000


// suggerimenti per la gestione di una linked list 
// che memorizza tutti i blocchi memorizzati nella tabella hash

// da guardare dopo che avete capito il funzionamento di main.c

// ogni oggetto di tipo ENTRY oltre all'intero associato alla
// stringa deve mantenere un puntatore ad un'altra ENTRY
// manteniamo l'intero e il puntatore in un unica struct coppia
typedef struct {
  int valore;    // numero di occorrenze della stringa 
  ENTRY *next;  
} coppia;


// orribile variabile globale per memorizzare la testa
// della lista delle entry che sono dentro la tabella hash
ENTRY *testa_lista_entry = NULL;


// messaggio errore e stop
void termina(const char *messaggio){
  if(errno!=0) perror(messaggio);
	else fprintf(stderr,"%s\n", messaggio);
  exit(1);
}

// crea un oggetto di tipo entry
// con chiave s e valore n e next==NULL
ENTRY *crea_entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if(e==NULL) termina("errore malloc entry 1");
  e->key = strdup(s); // salva copia di s
  e->data = malloc(sizeof(coppia));
  if(e->key==NULL || e->data==NULL)
    termina("errore malloc entry 2");
  // inizializzo coppia
  coppia *c = (coppia *) e->data; // cast obbligatorio
  c->valore  = n;
  c->next = NULL;
  return e;
}

// dealloca la memoria usata dalla ENTRY *e 
void distruggi_entry(ENTRY *e)
{
  free(e->key); free(e->data); free(e);
}


int main(int argc, char *argv[]) 
{
  
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
      // ho inserito *e nella tabella hash la metto anche
      // in cima alla lista delle entry inserite
      coppia *c = (coppia *) e->data;
      // salvo la vecchia lista dentro c->next
      c->next = testa_lista_entry;
      // e diventa la testa della lista
      testa_lista_entry = e;
    }
    else {
      // la stringa è gia' presente incremento il valore
      assert(strcmp(e->key,r->key)==0);
      coppia *c = (coppia *) r->data;
      c->valore +=1;
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
  puts("main_linked completato");
  return 0;
}

/* Si noti che quando si esegue questo programma con valgrind i blocchi
   che non sono deallocati (la parte della deallocazione dovete scriverla 
   voi) non figurano più come `lost`, ma `still_reachable` in quanto ancora
   raggingibili partendo dalla variabile globale  testa_lista_entry
*/
