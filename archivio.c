#include "xerrori.h"
#include "utility.h"

//tutte le costanti e le struct sono definite nel file 'utility.h'

void aggiungi(char *s);
int conta(char *s);

volatile sig_atomic_t distinct_strings = 0;
pthread_mutex_t distinct_strings_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t lettoriLog_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t ht_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex for the hashtable


void *signal_thread_body(void *voidArg){
    
    signal_arg_t *arg = (signal_arg_t *)voidArg;

    sigset_t mask;
    sigfillset(&mask);

    int sigNum;

    while(1) {

        int e = sigwait(&mask,&sigNum);
        if(e!=0) xtermina("Errore sigwait",FILE_POSITION);

        if( sigNum == SIGTERM ){
            xpthread_join(*(arg->masterWriter_thread),NULL,FILE_POSITION);
            xpthread_join(*(arg->masterReader_thread),NULL,FILE_POSITION);

            fprintf(stdout,"\nnella tabella hash ci sono %d stringhe distinte\n",distinct_strings);
            
            hdestroy();

            pthread_exit(NULL);
        }else if( sigNum == SIGINT ){
            fprintf(stderr,"\nnella tabella hash ci sono %d stringhe distinte\n",distinct_strings);
        }
    }
}

void *Master_body(void *voidArg){

    master_arg_t *arg = (master_arg_t *)voidArg;
    pc_buffer_t *producer_buffer = arg->pc_buffer;
    
    int strLen=1;
    char str[MAX_SEQUENCE_LENGTH];
    char *token;
    int errCode=1;
    char endString = '\0';

    //qua non c'è bisogno di accesso esclusivo alla FIFO inquanto già garantito, essendo questo l'unico thread che va a leggervi
    while(errCode > 0){
        errCode = read(arg->pipe_fd,&strLen,sizeof(int)); //lettura lunghezza stringa
        if( errCode == -1) xtermina("\n === Read Error ===\n",FILE_POSITION);
        assert(strLen < MAX_SEQUENCE_LENGTH);

        errCode = read(arg->pipe_fd,str,strLen);
        if( errCode == -1) xtermina("\n === Read Error ===\n",FILE_POSITION);

        // ##################### tokenizzazione della stringa letta ############################
        
        strcat(str, &endString);
        char *remaining = str; //string ptr for reentrance

        while( (token = strtok_r(remaining, ".,:; \n\r\t", &remaining)) != NULL){
            // invio della copia del token sul buffer produttore/consumatore
            if (write_buffer(producer_buffer,strdup(token)) != 0 ) 
                xtermina("errore scrittura su buffer",FILE_POSITION);
        }
        memset(str,'\0',sizeof(str)); //pulizia stringa
    }
    
    //pulizia buffer per dire ai thread scrittori che si ha finito la lettura
    int e;
    for(int i=0; i<PC_buffer_len; i++){
        e = write_buffer(producer_buffer,"EOF");
        if(e != 0) xtermina("errore scrittura buffer: EOF\n",FILE_POSITION);
    }
    
    pthread_exit(NULL);
}

void *SlaveWriter_body(void *voidArg){

    master_arg_t *arg = (master_arg_t *)voidArg;
    
    pc_buffer_t *consumer_buffer = arg->pc_buffer;
    if(consumer_buffer == NULL) xtermina("\n--- buffer creation error ---\n",FILE_POSITION);

    char *token;
    
    //termino quando incontro stringhe vuote
    while(1){
        char *token = read_buffer(consumer_buffer);
        
        if(strcmp(token, "EOF") == 0) break;
        if(token == NULL) break;

        aggiungi(token);

        memset(token,'\0',sizeof(token)); //pulizia stringa
    }
    pthread_exit(NULL);
}

void *SlaveReader_body(void *voidArg){
    
    slave_arg_t *arg = (slave_arg_t *)voidArg;
    
    pc_buffer_t *consumer_buffer = arg->pc_buffer;
    if(consumer_buffer == NULL) xtermina("\n--- buffer creation error ---\n",FILE_POSITION);

    char *token;
    int occurrences=0;
    char *printMessage;
    
    //termino quando incontro stringhe vuote
    while(1){
        char *token = read_buffer(consumer_buffer);
        
        if(strcmp(token, "EOF") == 0) break;        
        if(token == NULL ) break;
        
        occurrences = conta(token);
        xpthread_mutex_lock(&lettoriLog_mutex,FILE_POSITION);
            int err = asprintf(&printMessage,"%s %d\n",token,occurrences);
            err = write(arg->lettori_log,printMessage,strlen(printMessage));
        xpthread_mutex_unlock(&lettoriLog_mutex,FILE_POSITION);

        if(err != strlen(printMessage)){
            xtermina("errore write",FILE_POSITION);
        }
        

        memset(token,'\0',sizeof(token)); //pulizia stringa
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){

    if(argc != 3) xtermina("insufficient argument's numbers\n",FILE_POSITION);
    int r_num = atoi(argv[1]);
    int w_num = atoi(argv[2]);

    assert(r_num > 0);
    assert(w_num > 0);
    
    //blocco tutti i segnali del thread attuale in modo che sia solo il thread dei segnali a gestirli
    //tutti i thread creati dal main erediteranno questa sigmask
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask , NULL);

    //creazione e avvio del thread dei segnali con argomenti i thread capolet/caposc
    pthread_t signal_thread;
    pthread_t masterWriter_thread;
    pthread_t masterReader_thread;

    signal_arg_t signal_arg;
    signal_arg.masterWriter_thread = &masterWriter_thread;
    signal_arg.masterReader_thread = &masterReader_thread;


    xpthread_create(&signal_thread,NULL,&signal_thread_body,&signal_arg,FILE_POSITION);

    int capolet_fd = open("capolet",O_RDONLY);
    int caposc_fd = open("caposc",O_RDONLY);

    int lettori_log_fd = open("lettori.log",(O_WRONLY | O_CREAT | O_TRUNC),0666);
    
    //creazione della tabella hash
    int HashTable = hcreate(Num_elem);
    if(HashTable == 0)  xtermina("\n=== hashtable: creation error ===\n",FILE_POSITION);

    // ################################################################
    // ########################## CAPOSC ##############################
    //#################################################################
    // inizializzazione buffer prod cons (scrittore)
    
    pc_buffer_t caposc_buffer;
    pthread_mutex_t pc_writer_mutex = PTHREAD_MUTEX_INITIALIZER;

    sem_t free_slot_writer;
    xsem_init(&free_slot_writer, 0,PC_buffer_len, FILE_POSITION);

    sem_t data_slot_writer;
    xsem_init(&data_slot_writer, 0,0, FILE_POSITION);

    int writer_p_index = 0;
    
    //alloco PC_buffer_len elementi di tipo char* (stringhe)
    caposc_buffer.buffer = (char**)malloc(sizeof(char *) * PC_buffer_len);
    if(caposc_buffer.buffer == NULL) xtermina("errore malloc",FILE_POSITION);

    int err = pcBuffer_init(&caposc_buffer,&writer_p_index,&pc_writer_mutex,&free_slot_writer,&data_slot_writer);
    if( err != 0 ) xtermina("errore generazione buffer",FILE_POSITION);

    master_arg_t masterWriter_arg;
    masterWriter_arg.pipe_fd = caposc_fd;
    masterWriter_arg.pc_buffer = &caposc_buffer;
    

    //############### slave writer ################
    pc_buffer_t writer_buffer;   
    int writer_c_index = 0;

    writer_buffer.buffer = caposc_buffer.buffer; //gli slave dovranno avere lo stesso buffer del master
    err = pcBuffer_init(&writer_buffer,&writer_c_index,&pc_writer_mutex,&free_slot_writer,&data_slot_writer);
    if( err != 0 ) xtermina("errore generazione buffer",FILE_POSITION);

    master_arg_t slaveWriter_arg;
    slaveWriter_arg.pipe_fd = caposc_fd;
    slaveWriter_arg.pc_buffer = &writer_buffer;

    //################################################################

    //inizializzazione thread caposcrittore e scrittore
    xpthread_create(&masterWriter_thread,NULL,&Master_body,&masterWriter_arg, FILE_POSITION);

    //pthread_t slaveWriter_thread;
    pthread_t slaveWriters_threads[w_num];

    for(int i=0; i<w_num; i++){
        xpthread_create(&slaveWriters_threads[i],NULL, &SlaveWriter_body, &slaveWriter_arg, FILE_POSITION);
    }

    //#################################################################################
    //################################# capolet #######################################
    //#################################################################################
    
    //inizializzazione buffer

    pc_buffer_t capolet_buffer;
    pthread_mutex_t pc_reader_mutex = PTHREAD_MUTEX_INITIALIZER;

    sem_t free_slot_reader;
    xsem_init(&free_slot_reader, 0,PC_buffer_len, FILE_POSITION);

    sem_t data_slot_reader;
    xsem_init(&data_slot_reader, 0,0, FILE_POSITION);

    int reader_p_index = 0;
    
    //alloco PC_buffer_len elementi di tipo char* (stringhe)
    capolet_buffer.buffer = (char**)malloc(sizeof(char *) * PC_buffer_len);
    if(capolet_buffer.buffer == NULL) xtermina("errore malloc",FILE_POSITION);

    err = pcBuffer_init(&capolet_buffer,&reader_p_index,&pc_reader_mutex,&free_slot_reader,&data_slot_reader);
    if( err != 0 ) xtermina("errore generazione buffer",FILE_POSITION);

    //inizializzazione dell'argomento da passare al thread master
    master_arg_t masterReader_arg;
    masterReader_arg.pc_buffer = &capolet_buffer;
    masterReader_arg.pipe_fd = capolet_fd;

    //############### slave reader ################
    pc_buffer_t reader_buffer;   
    int reader_c_index = 0;

    reader_buffer.buffer = capolet_buffer.buffer; //gli slave dovranno avere lo stesso buffer del master
    err = pcBuffer_init(&reader_buffer,&reader_c_index,&pc_reader_mutex,&free_slot_reader,&data_slot_reader);
    if( err != 0 ) xtermina("errore generazione buffer",FILE_POSITION);

    slave_arg_t slaveReader_arg;
    slaveReader_arg.pipe_fd = capolet_fd;
    slaveReader_arg.lettori_log = lettori_log_fd;
    slaveReader_arg.pc_buffer = &reader_buffer;

    //inizializzazione thread capolettore e lettore
    xpthread_create(&masterReader_thread,NULL,&Master_body,&masterReader_arg, FILE_POSITION);

    pthread_t slaveReaders_threads[r_num];

    for(int i=0; i<r_num; i++){
        xpthread_create(&slaveReaders_threads[i],NULL, &SlaveReader_body, &slaveReader_arg, FILE_POSITION);
    }

    //###################################################################    
    //##################### fase di chiusura ############################
    //###################################################################    
    
    for(int i=0; i<w_num; i++){
        xpthread_join(slaveWriters_threads[i],NULL,FILE_POSITION);
    }
    for( int i=0; i<r_num; i++){
        xpthread_join(slaveReaders_threads[i],NULL,FILE_POSITION);
    }
    
    xpthread_join(signal_thread,NULL,FILE_POSITION);
    
    //##################### pulizia ############################
    xclose(capolet_fd,FILE_POSITION);
    xclose(caposc_fd,FILE_POSITION);
    xclose(lettori_log_fd,FILE_POSITION);

    //pulizia del buffer
    for(int i=PC_buffer_len-1; i>0; i--){
        free(caposc_buffer.buffer[i]);
        free(capolet_buffer.buffer[i]);
    }

    free(caposc_buffer.buffer);
    free(capolet_buffer.buffer);

    xsem_destroy(&free_slot_writer,FILE_POSITION);
    xsem_destroy(&data_slot_writer,FILE_POSITION);

    xsem_destroy(&free_slot_reader,FILE_POSITION);
    xsem_destroy(&data_slot_reader,FILE_POSITION);

    //essendo tutti i mutex inizializzati con PTHREAD_MUTEX_INITIALIZER non è necessario liberare la memoria manualmente
    return 0;
}

void aggiungi(char *s){
    if(s == NULL || (strcmp(s,"")==0 || strcmp(s," ")==0)) return;

    ENTRY *element_to_search = EntryCreate(s,1);
    xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
        ENTRY *searchResult = hsearch(*element_to_search, FIND);
    xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);

    if(searchResult == NULL){ //se la ricerca non da alcun risultato
        xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
            searchResult = hsearch(*element_to_search, ENTER); //inseriamo l'elemento che stavamo cercando
        xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);

        xpthread_mutex_lock(&distinct_strings_mutex,FILE_POSITION);
            if(searchResult != NULL) distinct_strings ++; //incremento il contatore di stringhe distinte presenti nella hashmap
        xpthread_mutex_unlock(&distinct_strings_mutex,FILE_POSITION);
        
    }else{
        assert(strcmp(element_to_search->key,searchResult->key)==0);
        xpthread_mutex_lock(&ht_mutex,FILE_POSITION);
            int *data = (int *) searchResult->data;
            *data +=1;
        xpthread_mutex_unlock(&ht_mutex,FILE_POSITION);
        free_entry(element_to_search); // questa non la devo memorizzare
    }
    return;
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