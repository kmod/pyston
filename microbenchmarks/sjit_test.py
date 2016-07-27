def f(n):
    total = 0
    i = 1
    while i < n:
        i = i + 1
        j = 2
        while j * j <= i:
            if i % j == 0:
                break
            j = j + 1
        else:
            total = total + i
    print n, total

i = 200
while i:
    f(i)
    i = i - 1
f(200000)
