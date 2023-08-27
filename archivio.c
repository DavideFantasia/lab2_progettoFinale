#include "xerrori.h"
#include "utility.h"

//tutte le costanti sono definite nel file 'utility.h'

void aggiungi(char *s);
int conta(char *s);

int distinct_strings = 0;
pthread_mutex_t ht_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex for the hashtable


/*
anche se di base, per accedere alla variabile globale dovremmo usare dei mutex per avvere un accesso esclusivo
in questo caso accederemo alla variabile solo in caso di gestione di segnali che bloccano il processo intero
quindi abbiamo la garanzia di un accesso non multiplo, per cui i mutex non sono necessari al contrario degl'altri thread
*/
void *signal_thread_body(){
    sigset_t mask;
    sigfillset(&mask);

    char *printMessage;
    int sigNum;

    int len, ret;

    while(true) {
        int e = sigwait(&mask,&sigNum);
        if(e!=0) perror("Errore sigwait");
        
        if( sigNum == SIGINT ){
            len = asprintf(&printMessage, "\n=== recevied SIGINT ===\n distinct strings: %d",distinct_strings);
            ret = write(STDERR_FILENO,printMessage,len);
            if(len != ret)   xtermina("errore write",FILE_POSITION); 
        }
        if( sigNum == SIGTERM ){
            len = asprintf(&printMessage, "\n=== recevied SIGTERM ===\n distinct strings: %d",distinct_strings);
            ret = write(STDOUT_FILENO,printMessage,len);
            if(len != ret)   xtermina("errore write",FILE_POSITION);
            
            pthread_exit(NULL);
        }
    }
    return NULL;
}

typedef struct{
    int pipe_fd; // the file descriptor of the pipe (capolet/caposc)
    pthread_mutex_t *hashtable_mutex; //mutex for accessing the ht in reading/writing
    pc_buffer_t *pc_buffer;// producer/consumer buffer    
}master_writer_arg_t;

void *Masterwriter_body(void *voidArg){
    
    master_writer_arg_t *arg = (master_writer_arg_t *)voidArg;
    pc_buffer_t *producer_buffer = arg->pc_buffer;
    
    int strLen=1;
    char str[MAX_SEQUENCE_LENGTH];
    char *token;
    int errCode=1;

    //qua non c'è bisogno di accesso esclusivo alla FIFO inquanto già garantito, essendo questo l'unico thread che va a leggervi
    while(errCode > 0){
        errCode = read(arg->pipe_fd,&strLen,sizeof(int)); //lettura lunghezza stringa
        if( errCode == -1) xtermina("\n === Read Error ===\n",FILE_POSITION);
        assert(strLen < MAX_SEQUENCE_LENGTH);

        errCode = read(arg->pipe_fd,str,strLen);
        if( errCode == -1) xtermina("\n === Read Error ===\n",FILE_POSITION);
        
        //printf("\n %s \n",str);

        // ##################### tokenizzazione della stringa letta ############################
        
        char endString = '\0';
        strcat(str, &endString);
        char *remaining = str; //string ptr for reentrance

        while( (token = strtok_r(remaining, ".,:; \n\r\t", &remaining)) != NULL){
            // invio del token sul buffer produttore/consumatore
           if (write_buffer(producer_buffer,token) != 0 ) xtermina("errore scrittura su buffer",FILE_POSITION);
        }

        memset(str,'\0',sizeof(str)); //pulizia stringa
    }
    //pulizia buffer per dire che si ha finito la lettura
    
    for(int i=0; i<PC_buffer_len; i++){
        xsem_wait(producer_buffer->sem_free_slots,FILE_POSITION); 
        producer_buffer->buffer[*(producer_buffer->index)++ & PC_buffer_len] = '\0';
        xsem_post(producer_buffer->sem_data_items, FILE_POSITION);
    }
    pthread_exit(NULL);
}

void *SlaveWriter_body(void *voidArg){
    master_writer_arg_t *arg = (master_writer_arg_t *)voidArg;
    
    pc_buffer_t *consumer_buffer = arg->pc_buffer;
    if(consumer_buffer == NULL) xtermina("\n--- buffer creation error ---\n",FILE_POSITION);

    char *token;
    
    //termino quando incontro stringhe vuote
    do{  
        char *token = read_buffer(consumer_buffer);
        if(token == NULL) break;
        printf("\n--- TOKEN: %s ---\n",token);
        aggiungi(token);
        memset(token,'\0',sizeof(token)); //pulizia stringa
    }while(true);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){

    if(argc != 3) xtermina("insufficient argument's numbers\n",FILE_POSITION);
    int readerThread_num = atoi(argv[1]);
    int writerThread_num = atoi(argv[2]);

    int capolet_fd = open("capolet",O_RDONLY);
    int caposc_fd = open("caposc",O_RDONLY);


    pthread_mutex_t hashtable_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex da usare per accedere alla hashtable

    //creazione della tabella hash
    int HashTable = hcreate(Num_elem);
    if(HashTable == 0)  xtermina("\n=== hashtable: creation error ===\n",FILE_POSITION);

    //blocco tutti i segnali del thread attuale in modo che sia solo il thread dei segnali a gestirli
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask , NULL);

    //creazione e avvio del thread dei segnali
    pthread_t signal_thread;
    xpthread_create(&signal_thread,NULL,&signal_thread_body,NULL,FILE_POSITION);

    // ################################################################
    // inizializzazione buffer prod cons (scrittore)
    
    pc_buffer_t caposc_buffer;
    pthread_mutex_t pc_mutex = PTHREAD_MUTEX_INITIALIZER;

    sem_t free_slot_writer;
    xsem_init(&free_slot_writer, 0,PC_buffer_len, FILE_POSITION);

    sem_t data_slot_writer;
    xsem_init(&data_slot_writer, 0,0, FILE_POSITION);

    int p_index = 0;
    
    //alloco PC_buffer_len elementi di tipo char* (stringhe)
    caposc_buffer.buffer = (char**)malloc(sizeof(char *) * PC_buffer_len);
    if(caposc_buffer.buffer == NULL) xtermina("errore malloc",FILE_POSITION);

    int err = pcBuffer_init(&caposc_buffer,&p_index,&pc_mutex,&free_slot_writer,&data_slot_writer);
    if( err != 0 ) xtermina("errore generazione buffer",FILE_POSITION);
    
    //slave writer
    pc_buffer_t writer_buffer;   
    int c_index = 0;

    writer_buffer.buffer = caposc_buffer.buffer;
    err = pcBuffer_init(&writer_buffer,&c_index,&pc_mutex,&free_slot_writer,&data_slot_writer);
    if( err != 0 ) xtermina("errore generazione buffer",FILE_POSITION);

    master_writer_arg_t slaveWriter_arg;
    slaveWriter_arg.hashtable_mutex = &hashtable_mutex;
    slaveWriter_arg.pipe_fd = caposc_fd;
    slaveWriter_arg.pc_buffer = &writer_buffer;

    //################################################################

    //inizializzazione thread caposcrittore e scrittore

    master_writer_arg_t masterWriter_arg;
    masterWriter_arg.hashtable_mutex = &hashtable_mutex;
    masterWriter_arg.pipe_fd = caposc_fd;
    masterWriter_arg.pc_buffer = &caposc_buffer;

    pthread_t masterWriter_thread;
    xpthread_create(&masterWriter_thread,NULL,&Masterwriter_body,&masterWriter_arg, FILE_POSITION);

    pthread_t slaveWriter_thread;
    xpthread_create(&slaveWriter_thread,NULL, &SlaveWriter_body, &slaveWriter_arg, FILE_POSITION);

    //##################### fase di chiusura ############################
    xpthread_join(signal_thread,NULL,FILE_POSITION);

    xpthread_join(slaveWriter_thread,NULL,FILE_POSITION);
    xpthread_join(masterWriter_thread,NULL,FILE_POSITION); //chiusura capo scrittore

    //TODO: qua attenderemo tutti i thread e puliremo tutta la memoria rimasta aperta (array, buffer, pipe, file etc)

    free(caposc_buffer.buffer);
    xsem_destroy(&free_slot_writer,FILE_POSITION);
    xsem_destroy(&data_slot_writer,FILE_POSITION);

    hdestroy();
    xclose(capolet_fd,FILE_POSITION);
    xclose(caposc_fd,FILE_POSITION);

    return 0;
}

void aggiungi(char *s){

    ENTRY *element_to_search = EntryCreate(s,1);
    xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
        ENTRY *searchResult = hsearch(*element_to_search, FIND);
    xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);

    if(searchResult == NULL){ //se la ricerca da nessun risultato
        xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
            searchResult = hsearch(*element_to_search, ENTER); //inseriamo l'elemento che stavamo cercando
            if(searchResult != NULL) distinct_strings ++; //incremento il contatore di stringhe distinte presenti nella hashmap
        xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);
    }else{
        assert(strcmp(element_to_search->key,searchResult->key)==0);
        xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
            int *data = (int *) searchResult->data;
            *data +=1;
        xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);
        free_entry(element_to_search); // questa non la devo memorizzare
    }
}

int conta(char *s){
    ENTRY *entry_to_search = EntryCreate(s,1);
    int retVal = 0;

    xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
        ENTRY *search_result = hsearch(*entry_to_search,FIND);
    xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);
    if(search_result == NULL) return 0; //se la entry non è presente
    else{
        xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
            assert( strcmp(entry_to_search->key, search_result->key) == 0); //verifico che i nomi combacino
            retVal = *((int *) search_result->data);
        xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);
        return retVal; //returno il valore associato all'indirizzo data (castato da void* a int*)
    }
}