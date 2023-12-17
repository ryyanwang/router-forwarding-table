import random
import sys

def rangerd(l, u):
    return random.randint(l, u)

def generate(n):
    for i in range(n):
        time = 0.05 * i
        src = (rangerd(1, 254), rangerd(1, 254), rangerd(1, 254), rangerd(1, 254))
        src = [str(x) for x in src]
        dst = (rangerd(1, 254), rangerd(1, 254), rangerd(1, 254), rangerd(1, 254))
        dst = [str(x) for x in dst]
        ininterface = rangerd(1, 4)
        ttl = rangerd(0, 10)
        length = rangerd(1, 128)
        corrupt = rangerd(0, 9) < 2
        print(f'@{time:5.3f} {".".join(src):15s} {".".join(dst):15s} {ininterface} {ttl:3d} {length:3d} {"corrupt" if corrupt else ""}')

if __name__ == '__main__':
    if len(sys.argv) > 1:
        generate(int(sys.argv[1]))
    else:
        generate(100)
