import struct


# generate "/bin/sh" in hexadecimal\
  # echo -n "/bin/sh" | xxd -p
  
data = b'\xe4\xaa\xf130\x7f'

recv = data.hex()
recv_reversed = [recv[i:i+2] for i in range(0, len(recv), 2)]
recv_reversed = recv_reversed[::-1]
addr = "".join(recv_reversed)
print(addr)


addr = struct.unpack("<Q", data.ljust(8, b'\x00'))[0]
print(hex(addr))

A = b'A'
print(f'{A}')


# syscall('SYS_read', 0, inject_start, inject_size)



