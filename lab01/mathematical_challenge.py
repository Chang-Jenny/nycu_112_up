#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import base64
import hashlib
import time
import sys
import re
from pwn import *

def solve_pow(r):
    prefix = r.recvline().decode().split("'")[1]
    print(time.time(), "solving pow ...")
    solved = b''
    for i in range(1000000000):
        h = hashlib.sha1((prefix + str(i)).encode()).hexdigest()
        if h[:6] == '000000':
            solved = str(i).encode()
            print("solved =", solved)
            break
    print(time.time(), "done.")
    r.sendlineafter(b'string S: ', base64.b64encode(solved))

def solve_math(r):
    question = r.recvuntil(b'? ').decode()
    question = question.split(': ')[1][:-3]
    question = base64.b64decode(question).decode()
    print(question) # len = 249
    
    num = ""
    judge=[]
    
    for k in range(7):
        tmp = 5*k + 2*k
        num = ""
        for j in range(5):
            count=j*50 + 1 + tmp
            for i in range(5):
                num+=question[count+i]
        judge.append(num)
            
    keys = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '*', '-']
    values = [
        "┌───┐│   ││   ││   │└───┘",
        " ─┐    │    │    │   ─┴─ ",
        "┌───┐    │┌───┘│    └───┘",
        "┌───┐    │ ───┤    │└───┘",
        "│   ││   │└───┤    │    │",
        "┌────│    └───┐    │└───┘",
        "┌───┐│    ├───┐│   │└───┘",
        "┌───┐│   │    │    │    │",
        "┌───┐│   │├───┤│   │└───┘",
        "┌───┐│   │└───┤    │└───┘",
        "       │  ──┼──  │       ",
        "       •  ─────  •       ",
        "      ╲ ╱   ╳   ╱ ╲      ",
        "          ─────          "
    ]
    symbol = dict(zip(keys, values))
    
    equations = ""
    for i in range(len(judge)):
        for key, value in symbol.items():
            if judge[i] == value:
                equations+=key
    ans = int(eval(equations))
    print(F"equation: {equations} = {ans}")
    
    
    # r.sendline(base64.b64encode(str(ans)))
    r.sendline(str(ans).encode())
    # r.send(b'\n')
    
    
if __name__ == "__main__":
    r = remote('up.zoolab.org', 10681)
    solve_pow(r)
    
    for i in range(4):
        msg = r.recvline().decode()
    count = int(re.findall('[0-9]+', msg)[0])
    print(f"we need to solve {count} mathematical equations.")
    
    for i in range(count):
        solve_math(r)
        
    
    print(r.recvline().decode())
    # r.interactive()
    r.close()
    
# vim: set tabstop=4 expandtab shiftwidth=4 softtabstop=4 number cindent fileencoding=utf-8 :