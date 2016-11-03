import random

random.seed(12345)

def randchr():
    return chr(int(random.random() * 26) + ord('a'))
def randstr(n):
    return ''.join([randchr() for i in xrange(n)])

d = {}
for i in xrange(10):
    d[randstr(5)] = i
print d
print d.items()
