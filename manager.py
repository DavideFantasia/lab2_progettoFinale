#! /usr/bin/env python3
import subprocess, time, signal

# esegui main come processo in background
# p = subprocess.Popen(["./main", "ciao", "bao", "ciao", "zao"])
# come sopra ma lanciando anche valgrind
# la lista passata a Popen contiene la lina di comando da eseguire 
p = subprocess.Popen(["valgrind","--leak-check=full", 
                      "--show-leak-kinds=all", 
                      "--log-file=valgrind-%p.log", 
                      "main", "ciao", "bao", "ciao", "zao"])
print("Ho lanciato il processo:", p.pid)
print("Attendo 50 secondi (nel frattempo arriva l'output di main)")
try:
  time.sleep(50)
except KeyboardInterrupt:
  print("Attesa accorciata da SIGINT")
  
print("Se compare None il processo è ancora in esecuzione")
# p.poll() restituisce exit code oppure None se il processo è in esecuzione
print(p.poll())
# mando un segnale SIGTERM a main che lo farà terminare
p.send_signal(signal.SIGTERM)
time.sleep(1) # diamo il tempo al segnale di arrivare
# ora p.poll() restituisce l'exit code di main
print("Exit code di main o None se non è terminato:", p.poll())
