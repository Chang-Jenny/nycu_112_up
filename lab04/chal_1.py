import random
from pwn import *


HOST = "up.zoolab.org"
PORT = 10931


if __name__ == "__main__":
  conn = remote(HOST, PORT)
  while True:
    recv = conn.recv(timeout=1).decode("utf-8")
    if not recv:
      break
    print(recv, end="")
  while True:
    rand = random.randint(0, 1)
    if rand == 0:
      conn.sendline(b'R')
    else:
      conn.sendline(b'flag')
      recv = conn.recvline().decode("utf-8")
      if "FLAG" in recv:
        print(f"---------------------------------------------\nI got it: {recv}",
              end='---------------------------------------------\n')
        break
  conn.interactive()
  conn.close()