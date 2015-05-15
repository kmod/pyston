# expected: fail

class StrLike(object):
    def __add__(self, rhs):
        raise TypeError("invalid type")

class CanStrRadd(object):
    def __radd__(self, lhs):
        return "added!"

try:
    print StrLike().__add__(CanStrRadd()) # errors
except TypeError as e:
    print e
try:
    print StrLike() + CanStrRadd() # errors
except TypeError as e:
    print e
try:
    print "".__add__(CanStrRadd()) # errorrs
except TypeError as e:
    print e
print "" + CanStrRadd() # works!
