from threading import Thread
from time import sleep

poem = []

def main(file):
    for line in open(file):
        line = line.rstrip('\n')
        poem.append(line)

    _t1 = Thread(target=printArray, args=(poem,))
    _t2 = Thread(target=fuckWithArray, args=(poem,))
    _t1.start()
    _t2.start()
    _t1.join()
    _t2.join()


def printArray(arr):
    global pos
    for _x in range(0, len(arr)):
        pos = _x
        sleep(.3)
        print(poem[pos])

def fuckWithArray(arr):
    global pos
    for _x in range(0, len(arr)):
        pos = _x
        sleep(.2)

main('caged_bird.txt')