#include "xerrori.h"
#include <search.h>

#define PC_buffer_len 10  //lunghezza del buffer produttori/consumatori
#define FILE_POSITION __LINE__,__FILE__
#define MAX_SEQUENCE_LENGTH 2048
#define Num_elem 1000000 //dimensione della tabella hash

#if 0
// ridefinizione funzione gettid() (non è sempre disponibile) 
#include <sys/syscall.h>   /* For SYS_xxx definitions */
pid_t gettid(void)
{
	#ifdef __linux__
	  // il tid è specifico di linux
    pid_t tid = (pid_t)syscall(SYS_gettid);
    return tid;
  #else
	  // per altri OS restituisco -1
	  return -1;
  #endif
}
#endif

typedef struct{
    char **buffer; //string array
    int *index;
    pthread_mutex_t *mutex;
    sem_t *sem_free_slots;
    sem_t *sem_data_items;  
}pc_buffer_t;

int pcBuffer_init(pc_buffer_t *buffer_notInit,int *index, pthread_mutex_t *buff_mutex, sem_t *sem_free_slots, sem_t *sem_data_items);

char *read_buffer(pc_buffer_t *buffer);
int write_buffer(pc_buffer_t *buffer,char *token);

//function that return a pointer to a compiled entry
ENTRY *EntryCreate(char *key, int value);

//free the entry and its inside
void free_entry(ENTRY *entry);