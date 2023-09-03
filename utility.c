#include "utility.h"

int pcBuffer_init(pc_buffer_t *buffer, 
                        int *index, 
                        pthread_mutex_t *buff_mutex,
                        sem_t *sem_free_slots,
                        sem_t *sem_data_items){

    if(buffer->buffer == NULL) return -1;
    buffer->index = index;
    buffer->mutex = buff_mutex;
    buffer->sem_data_items = sem_data_items;
    buffer->sem_free_slots = sem_free_slots;

    return 0;
}

char *read_buffer(pc_buffer_t *buffer){
    char *string;
    xsem_wait(buffer->sem_data_items,FILE_POSITION);
        xpthread_mutex_lock(buffer->mutex,FILE_POSITION);
            
            int index = *(buffer->index);
            string = buffer->buffer[index % PC_buffer_len];
            *(buffer->index)+= 1;

        xpthread_mutex_unlock(buffer->mutex,FILE_POSITION);
    xsem_post(buffer->sem_free_slots,FILE_POSITION);
    
    return string;
}

int write_buffer(pc_buffer_t *buffer,char *token){

    if(strlen(token) <= 0) return 1;

    char *copia = strdup(token);
    xsem_wait(buffer->sem_free_slots,FILE_POSITION); //aspetto si crei spazio libero
        //metto nella posizione index+1 il token (array circolare)
        int index = (*(buffer->index))++ % PC_buffer_len;
        buffer->buffer[ index ] = copia;
    xsem_post(buffer->sem_data_items, FILE_POSITION); //dico di aver occupato uno slot con dei dati
    return 0;    
}

//function that create an entry set to value
ENTRY *EntryCreate(char *key, int value){
    ENTRY *entry = malloc(sizeof(ENTRY));
    entry->key = strdup(key);

    entry->data = (int *)malloc(sizeof(int));
    *((int *)entry->data) = value; //accedo al valore contenuto all'indirizzo entry->data per modificarne il valore

    //returno la entry compilata
    return entry;
} 

void free_entry(ENTRY *entry){
    free(entry->key); free(entry->data); free(entry);
}
