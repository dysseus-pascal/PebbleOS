# Holt die groessten Bytes aus einer .reso-Pickle-Datei, ohne sie auszufuehren (pickletools).
import pickletools, sys
def daten(pfad):
    d = open(pfad,'rb').read()
    best = b''
    for op, arg, pos in pickletools.genops(d):
        if isinstance(arg, (bytes, bytearray)) and len(arg) > len(best):
            best = bytes(arg)
    return best
if __name__ == '__main__':
    b = daten(sys.argv[1])
    open(sys.argv[2],'wb').write(b)
    print(len(b), b[:12].hex())
