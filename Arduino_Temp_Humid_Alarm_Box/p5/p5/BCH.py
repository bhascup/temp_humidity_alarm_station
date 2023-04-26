   
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


buffer = [0xAA, 0xFC, 0x01, 0x03]
toPrint = DCP_genCmndBCH(buffer, 4)
print(toPrint)