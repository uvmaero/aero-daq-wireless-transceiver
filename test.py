import serial
import time

port='/dev/ttyUSB0'

TX_INTERVAL = 2

tx_time = time.time()

i=0

with serial.Serial(port, 115200, timeout=0.1) as s:
    while True:
        line = s.readline()
        if line != b'':
            print(line)

        if time.time() > (tx_time + TX_INTERVAL):
            print("Sending test message ")
            a = f'test #{i}\r\n'
            s.write(bytearray(a, 'utf-8'))
            tx_time = time.time()
            i+=1