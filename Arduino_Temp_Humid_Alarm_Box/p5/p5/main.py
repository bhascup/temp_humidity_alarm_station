from flask import Flask, render_template
import socket
import threading as th
import time
from datetime import datetime
from cs50 import SQL
import smtplib

class RTU:
    def __init__(self, id, ip, port):
        self.id = id
        self.temp = 0
        self.humid = 0
        self.alarm = 0
        self.ip = ip
        self.port = port

    def send(self, UDPServerSocket):
        buffer = bytearray([170, 252, self.id, 3])
        bch = BCH(buffer, 4)
        buffer.append(bch)
        UDPServerSocket.sendto(buffer, (self.ip, self.port))

    def listen(self, UDPServerSocket):
        buffersize = 1024
        bytesAddressPair = UDPServerSocket.recvfrom(buffersize)
        message = bytesAddressPair[0]

        return message

    def setInfo(self, UDPServerSocket):
        self.send(UDPServerSocket)
        responce = self.listen(UDPServerSocket)

        temp = []
        for i in range(0, 7):
            temp.append(responce[i])
        if BCH(temp, 7) == responce[7]:
            self.temp = responce[3]
            self.humid = responce[4]
            alarmValue = responce[5]
            if alarmValue == 1:
                self.alarm = "Major Under"
            elif alarmValue == 2:
                self.alarm = "Minor Under"
            elif alarmValue == 3:
                self.alarm = "Comfortable"
            elif alarmValue == 4:
                self.alarm = "Minor Over"
            elif alarmValue == 5:
                self.alarm = "Major Over"


def BCH(buff, count):
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
    UDPServerSocket = socket.socket(
        family=socket.AF_INET, type=socket.SOCK_DGRAM)
    return UDPServerSocket


def pollAll(list, UDPServerSocket):
    while True:
        for rtu in list:
            priorAlarm = rtu.alarm
            rtu.setInfo(UDPServerSocket)
            if priorAlarm != rtu.alarm:
                
                sender = 'monty@python.com'
                receivers = ['ace@sendys.com']
                subject = "Alarm Triggered!"
                message = 'Subject: {} ID = {}\n\n{}\n Temperature = {}'.format(subject, rtu.id, rtu.alarm, rtu.temp)

                try:
                    smtpObj = smtplib.SMTP(port = 6000, host = 'localhost')
                    smtpObj.sendmail(sender, receivers, message)         
                    print ("Successfully sent email")
                except:
                    print ("Error: unable to send email")


        time.sleep(5)


def saveData(list, UDPServerSocket):
    db = SQL("sqlite:///history.db")
    while True:
        now = datetime.now()
        day = now.day
        month = now.month
        year = now.year
        hour = now.hour
        minute = now.minute

        if minute % 15 == 0:
            for rtu in list:
                rtu.setInfo(UDPServerSocket)
                db.execute("INSERT INTO History (unitID, temperature, humidity, day, month, year, hour, minute, alarmStatus) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)", rtu.id, rtu.temp, rtu.humid, day, month, year, hour, minute, rtu.alarm)
            
            time.sleep(800)



# creating list
ip_list = ["192.168.1.177"]
numberOfUnits = len(ip_list)

rtu_list = []
# Inits list of devices
for i in range(0, numberOfUnits):
    rtu_list.append(RTU(i + 1, ip_list[i], 8888))

mySocket = makeSocket()


S = th.Timer(0, pollAll, args=(rtu_list, mySocket))
S.start()


save = th.Timer(0, saveData, args=(rtu_list, mySocket))
save.start()


app = Flask(__name__)


@app.route("/")
def home():
    return render_template("home.html", rtus=rtu_list)


@app.route("/history")
def history():
    db = SQL("sqlite:///history.db")
    datapoints = db.execute("SELECT * FROM History ORDER BY unitID")
    return render_template("history.html", datapoints = datapoints)


if __name__ == "__main__":

    app.run(debug=False)
