class C:
    pass

c = C()
c.a = 1
def f(c):
    n = 1000000
    while n:
        c.a
        n -= 1
f(c)
c.a = None
f(c)

