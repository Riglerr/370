from threading import Thread

def printInt( x ):
    print(x)
    return

t = Thread(target=printInt, args=(13))
s = Thread(target=printInt, args=(15))
t.start()
s.start()
t.join()
s.join()