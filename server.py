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
 
def main(maxThreads, numLettori=3, numScrittori=3,withValgrind=False,host=HOST, port=PORT):

  #creating the FIFO if not existing
  if not os.path.exists(capolet_path):
    os.mkfifo(capolet_path,mode = 0o666)
  if not os.path.exists(caposc_path):
    os.mkfifo(caposc_path, mode = 0o666)

  #avvio del file archivio
  archivioString = ["./archivio", str(numLettori), str(numScrittori)]
  if withValgrind:
    print("avvio con valgrind\n")
    archivioString = ["valgrind", "--leak-check=full", "--show-leak-kinds=all", "--log-file=valgrind-%p.log"] + archivioString

  archivioProcess = subprocess.Popen(archivioString)

  capolet_fd = os.open(capolet_path,os.O_WRONLY, 0o666)
  #caposc_fd = os.open(caposc_path, os.O_WRONLY, 0o666)
  caposc_fd = 10

  if caposc_fd <= 0 : print("errore apertura caposc\n")
  if capolet_fd <= 0 : print("errore apertura capolet\n")


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
    print('\n\t=== turning server off ===')

    #closing the connection
    s.shutdown(socket.SHUT_RDWR)

    #deleting the named pipe
    #chiusura della pipe
    os.close(capolet_fd)
    #os.close(caposc_fd)

    os.unlink(caposc_path)
    os.unlink(capolet_path)

    #closing the archive
    archivioProcess.send_signal(signal.SIGTERM)

    print('\n\t=== server off ===')

    return 0
    
    ###########################################################################################
    #TODO:  connessione funzionante via FIFO, verificare la corretta apertura e scrittura su pipe
    ###########################################################################################
    

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

  #leggo strLen bytes, ovvero la stringa in ingresso
  data = recv_all(conn,strLen)

  #scrittura sulla Pipe mandando prima la dimensione della stringa e poi la stringa
  if os.write(capolet_fd,struct.pack("<i",strLen)) < 0:
    print("errore scrittura lunghezza n")
  
  bytes_written = os.write(capolet_fd,data)
  
  if bytes_written<0:
    print("errore scrittura dati\n")

  #scrittura sul file server.log le statistiche
  msgLog('A',bytes_written)
  


def handler_B(conn, caposc_fd):
  #connessione B: deve scrivere sulla FIFO 'caposc'
  print("connessione di tipo B")
  bytes_written = 0
  #ad ogni ciclo leggiamo la lunghezza della stringa e poi la stringa
  #se la lunghezza è zero, sappiamo di dover smettere la lettura per questa connessione
  while(True):
    #receving the lenght of the string
    data = recv_all(conn, 2)
    strLen = struct.unpack("!H",data)[0]
    assert strLen < Max_sequence_length, f"max sequence lenght: {Max_sequence_length}\nyour lenght: {strLen}"    
    #breaking condition
    if strLen == 0:
      print("\n\t=== EOF ===")
      break
        
    #reading the incoming string 
    print(f"dimensione della stringa: {strLen}\n")
    data = recv_all(conn,strLen)
    string = data.decode()

    bytes_written += len(data)
    print(f"\n\t=== in arrivo dal clientB la stringa: ===\n==={string}")

  msgLog('B',bytes_written)
 
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


