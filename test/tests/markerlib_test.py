import _markerlib
marker = _markerlib.compile("extra == 'certs'")
print marker()
print _markerlib.compile("extra is None")()
