import socket

def DCP_genCmndBCH(buff, count):
    i = 0
    j = 0
    bch = 0 
    nBCHpoly = 0xB8
    fBCHpoly = 0xFF

    for i in range(0, count):
        bch ^= buff[i]
        for j in range(0, 8):
            if (bch & 1) == 1:
                bch = (bch >> 1) ^ nBCHpoly
            else:
                bch >>= 1
    bch ^= fBCHpoly
    return (bch)

def makeSocket():
    localIP     = "192.168.1.1"
    localPort = 55312
    UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    UDPServerSocket.bind((localIP, localPort))
    return UDPServerSocket

def send(UDPServerSocket, UDP_IP, UDP_PORT, ID):
    UDP_IP = "192.168.1.177"
    UDP_PORT = 8888

    buffer = bytearray([170, 252, ID, 3])
    bch = DCP_genCmndBCH(buffer, 4)
    buffer.append(bch)

    print(buffer[4])

    UDPServerSocket.sendto(buffer, (UDP_IP, UDP_PORT))

def listen(UDPServerSocket):
    buffersize = 1024
    bytesAddressPair = UDPServerSocket.recvfrom(buffersize)

    message = bytesAddressPair[0]

    address = bytesAddressPair[1]

    clientMsg = "Message from Client:{}".format(message)
    clientIP  = "Client IP Address:{}".format(address)
    
    print(clientMsg)
    print(clientIP)

mySocket = makeSocket()
send(mySocket, 0, 0, ID = 1)
listen(mySocket)

