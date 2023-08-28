#!/usr/bin/env python3
# server che fornisce l'elenco dei primi in un dato intervallo 
# gestisce più clienti contemporaneamente usando i thread
# invia il byte inutile all'inizio della connessione
import argparse, sys, struct, socket, threading, concurrent.futures, logging, subprocess, time, signal, os

Description = "Server che gestisce le connessioni in ingresso"

# host e porta di default
HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
PORT = 54105  # Port to listen on (non-privileged ports are > 1023)
Max_sequence_length = 2048 #massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe

caposc_path = "caposc" #path for the named Pipe (FIFO)
capolet_path = "capolet" #path for the named Pipe (FIFO)

#creating the mutexs for writing to the pipes
capolet_mutex =  threading.Lock()
caposc_mutex =  threading.Lock()

 
def main(maxThreads, numLettori=3, numScrittori=3,withValgrind=False,host=HOST, port=PORT):

  #creating the FIFO if not existing
  if not os.path.exists(capolet_path):
    os.mkfifo(capolet_path,mode = 0o666)
  if not os.path.exists(caposc_path):
    os.mkfifo(caposc_path, mode = 0o666)

  #avvio del file archivio
  archivioString = ["./archivio", str(numLettori), str(numScrittori)]
  if withValgrind:
    archivioString = ["valgrind", "--leak-check=full", "--show-leak-kinds=all", "--log-file=valgrind-%p.log"] + archivioString

  archivioProcess = subprocess.Popen(archivioString)

  capolet_fd = os.open(capolet_path,os.O_WRONLY, 0o666)
  caposc_fd = os.open(caposc_path, os.O_WRONLY, 0o666)

  if caposc_fd < 0 : print("errore apertura caposc\n")
  if capolet_fd < 0 : print("errore apertura capolet\n")


  #creazione del file di log
  logging.basicConfig(filename='server.log', level=logging.INFO, format='%(message)s')

  # creazione del server socket
  with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    try:  
      s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            
      s.bind((host, port))
      s.listen()
      with concurrent.futures.ThreadPoolExecutor(max_workers=maxThreads) as executor:
        while True:
          # mi metto in attesa di una connessione
          conn, addr = s.accept()
          # l'esecuzione di submit non è bloccante
          # fino a quando ci sono thread liberi
          executor.submit(connection_handler, conn,addr,capolet_fd,caposc_fd)
    except KeyboardInterrupt:
      pass

    # fase di chiusura

    #closing the connection
    s.shutdown(socket.SHUT_RDWR)

    #chiusura della pipe
    os.close(capolet_fd)
    os.close(caposc_fd)

    #deleting the named pipe
    os.unlink(caposc_path)
    os.unlink(capolet_path)

    #closing the archive's process
    #archivioProcess.send_signal(signal.SIGTERM)
    
    archivioProcess.terminate()
    archivioProcess.wait()

    return 0
    

# gestione di una singola connessione con un client
# smista le connessioni in ingresso in base al tipo di connessione chiamando l'handler associato
def connection_handler(conn,addr,capolet_fd,caposc_fd):
  with conn:  
    # ---- attendo un carattere da 1 byte del tipo di connessione
    data = recv_all(conn,1)
    assert len(data)==1
    connection_type  = data.decode()
    
    if connection_type == 'A':
      handler_A(conn,capolet_fd)
    elif connection_type == 'B':
      handler_B(conn,caposc_fd)
    else:
      print("errore nella ricezione del tipo di connessione\n")
      conn.close(socket.SHUT_RDWR)
  
  return 0


# riceve esattamente n byte e li restituisce in un array di byte
# il tipo restituto è "bytes": una sequenza immutabile di valori 0-255
# analoga alla readn
def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 1024))
    if len(chunk) == 0:
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks


def handler_A(conn, capolet_fd):
  #connessione A: deve scrivere sulla FIFO 'capolet'
  #dimensione della stringa, che viene spacchettata
  data = recv_all(conn,2)
  strLen = struct.unpack("!H",data)[0]

  #verifico che la stringa non superi la lunghezza massima
  assert strLen < Max_sequence_length, f"max sequence lenght: {Max_sequence_length}\nyour lenght: {strLen}"
  
  #se la stringa è vuota si skippa
  if strLen < 0:
    return 0

  #leggo strLen bytes, ovvero la stringa in ingresso
  data = recv_all(conn,strLen)

  
  #acquisizione del mutex per poter scrivere senza sovrapposizione sulla pipe
  #scrittura sulla Pipe mandando prima la dimensione della stringa e poi la stringa
  
  capolet_mutex.acquire()
  
  if os.write(capolet_fd,struct.pack("<i",strLen)) < 0:
    print("errore scrittura lunghezza n")
  
  bytes_written = os.write(capolet_fd,data)

  capolet_mutex.release()
  
  if bytes_written<0:
    print("errore scrittura dati\n")

  #scrittura sul file server.log le statistiche
  msgLog('A',bytes_written)

  return 0
  


def handler_B(conn, caposc_fd):
  #connessione B: deve scrivere sulla FIFO 'caposc'

  #ad ogni ciclo leggiamo la lunghezza della stringa e poi la stringa
  #se la lunghezza è zero, sappiamo di dover smettere la lettura per questa connessione
  while(True):

    bytes_written = 0
    #receving the lenght of the string
    data = recv_all(conn, 2)
    strLen = struct.unpack("!H",data)[0]

    assert strLen < Max_sequence_length, f"max sequence lenght: {Max_sequence_length}\nyour lenght: {strLen}"    
    #breaking condition
    if strLen == 0:
      break
        
    #reading the incoming string 
    data = recv_all(conn,strLen) #stringa codificata
    
    #scrittura sulla fifo caposc acquisendo il mutex per una scrittura esclusiva, evitando sovrascrizioni fra i thread
    caposc_mutex.acquire()
    
    if os.write(caposc_fd,struct.pack("<i",strLen)) == -1:
      print("\n=== errore scrittura strLen ===\n")
      exit(1)
    bytes_written = os.write(caposc_fd,data) 
    if bytes_written == -1:
      print("\n===ERRORE===")
      exit(1)
    caposc_mutex.release()
    
    msgLog('B',bytes_written)

  return 0
 

 
#funzione usata nelle connessioni con i client per scrivere su server.log i messaggi di log
def msgLog(connection_type,bytes_written):
  logger = logging.getLogger()  # Ottieni un'istanza del logger
  log_message = f"Connessione {connection_type}: {bytes_written} byte scritti"
  logger.info(log_message)


# questo codice viene eseguito solo se il file è eseguito direttamente
# e non importato come modulo con import da un altro file
if __name__ == '__main__':
  # parsing della linea di comando vedere la guida
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('maxThread',help='max number of thread', type = int)
  parser.add_argument("-r",help="numero di thread lettori", type=int, default=3)
  parser.add_argument("-w",help="numero di thread scrittori", type=int, default=3)
  parser.add_argument("-v", help="forza l'avvio di 'archivio' con valgrind", action="store_true")
  parser.add_argument('-a', help='host address', type = str, default=HOST)  
  parser.add_argument('-p', help='port', type = int, default=PORT) 
  args = parser.parse_args()
  if args.maxThread < 1:
    print("=== error ===\n\tserver.py requires 'maxThread' greater than zero\n")
    exit(1)
  main(args.maxThread,args.r,args.w,args.v,args.a,args.p)


