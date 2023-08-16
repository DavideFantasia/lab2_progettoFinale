#!/usr/bin/env python3
# server che fornisce l'elenco dei primi in un dato intervallo 
# gestisce più clienti contemporaneamente usando i thread
# invia il byte inutile all'inizio della connessione
import argparse, sys, struct, socket, threading, concurrent.futures, logging, subprocess, time, signal

Description = "Server che gestisce le connessioni in ingresso"

# host e porta di default
HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
PORT = 54105  # Port to listen on (non-privileged ports are > 1023)
Max_sequence_length = 2048 #massima lunghezza di una sequenza che viene inviata attraverso un socket o pipe
 
def main(maxThreads, numLettori=3, numScrittori=3,withValgrind=False,host=HOST, port=PORT):

  #avvio del file archivio
  archivioString = ["./archivio", str(numLettori), str(numScrittori)]
  if withValgrind:
    print("avvio con valgrind\n")
    archivioString = ["valgrind", "--leak-check=full", "--show-leak-kinds=all", "--log-file=valgrind-%p.log"] + archivioString

  archivioProcess = subprocess.Popen(archivioString)

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
          print("In attesa di un client...")
          # mi metto in attesa di una connessione
          conn, addr = s.accept()
          # l'esecuzione di submit non è bloccante
          # fino a quando ci sono thread liberi
          executor.submit(connection_handler, conn,addr)
    except KeyboardInterrupt:
      pass
    print('\n\t=== turning server of ===')
    s.shutdown(socket.SHUT_RDWR)
    #TODO:  il server deve terminare l'esecuzione chiudendo il socket con l'istruzione shutdown
    # cancellando (con os.unlink) le FIFO caposc e capolet
    # e inviando il segnale SIGTERM al programma archivio
    

# gestione di una singola connessione con un client
def connection_handler(conn,addr): 
  with conn:  
    print(f"{threading.current_thread().name} contattato da {addr}")

    # ---- attendo un carattere da 1 byte del tipo di connessione
    data = recv_all(conn,1)
    assert len(data)==1
    connection_type  = data.decode()
    
    if connection_type == 'A':
      handler_A(conn)
    elif connection_type == 'B':
      handler_B(conn)
    else:
      print("errore nella ricezione del tipo di connessione\n")
      conn.close(socket.SHUT_RDWR)

  print(f"{threading.current_thread().name} finito con {addr}")
 


# riceve esattamente n byte e li restituisce in un array di byte
# il tipo restituto è "bytes": una sequenza immutabile di valori 0-255
# analoga alla readn che abbiamo visto nel C
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


def handler_A(conn):
  print("connessione di tipo A")
  data = recv_all(conn,2)
  strLen = struct.unpack("!H",data)[0]
  print(f"lunghezza della stringa: {strLen}")
  data = recv_all(conn,strLen)
  bytes_written = len(data)
  line = data.decode()
  #TODO: implementare la connessione via pipe
  msgLog('A',bytes_written)
  print(f"\t----Stringa arrivata: {line} ----\n")
  


def handler_B(conn):
  print("connessione di tipo B")
  bytes_written = 0
  #ad ogni ciclo leggiamo la lunghezza della stringa e poi la stringa
  #se la lunghezza è zero, sappiamo di dover smettere la lettura per questa connessione
  while(True):
    #receving the lenght of the string
    data = recv_all(conn, 2)
    strLen = struct.unpack("!H",data)[0]
        
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


