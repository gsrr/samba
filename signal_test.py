
import signal  
import time  
import os
  
  
def handler(a, b):  # define the handler  
    print("Signal Number:", a, " Frame: ", b)  
  
signal.signal(signal.SIGUSR1, handler)  # assign the handler to the signal SIGINT  
  
print os.getpid()
while 1:  
    print("Press ctrl + c")  # wait for SIGINT  
    time.sleep(10) 

