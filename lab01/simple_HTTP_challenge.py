from pwn import *
'''
* Connected to ipinfo.io (34.117.186.192) port 80
> GET /ip HTTP/1.1
> Host: ipinfo.io
> User-Agent: curl/8.4.0
> Accept: */*
> 
< HTTP/1.1 200 OK
< server: nginx/1.24.0
< date: Mon, 26 Feb 2024 05:42:27 GMT
< content-type: text/plain; charset=utf-8
< Content-Length: 15
< access-control-allow-origin: *
< x-envoy-upstream-service-time: 0
< via: 1.1 google
< strict-transport-security: max-age=2592000; includeSubDomains
< 
* Connection #0 to host ipinfo.io left intact
'''


def retrieve_ip_address():
    r = remote("ipinfo.io", 80)

    # Send HTTP GET request
    r.sendline(b"GET /ip HTTP/1.1")
    r.sendline(b"Host: ipinfo.io")
    r.sendline(b"Connection: close")
    r.sendline(b"User-Agent: curl/8.4.0")
    r.sendline(b"Accept: */*")
    r.sendline()

    # Recvive HTTP response
    response = r.recvall().decode()
    ip_address = response.rstrip().lstrip()
    print(ip_address)

    r.close()


if __name__ == "__main__":
    retrieve_ip_address()