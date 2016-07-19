import os, sys, subprocess, shutil
sys.path.append(os.path.dirname(__file__) + "/../lib")

from test_helper import create_virtenv, run_test

ENV_NAME = "funcsigs_test_env_" + os.path.basename(sys.executable)
SRC_DIR = os.path.abspath(os.path.join(ENV_NAME, "src"))
PYTHON_EXE = os.path.abspath(os.path.join(ENV_NAME, "bin", "python"))

pkg = ["funcsigs==1.0.2"]
create_virtenv(ENV_NAME, pkg)
FUNCSIGS_DIR = os.path.abspath(os.path.join(SRC_DIR, "funcsigs"))

def test_error(meth_name):
    s = """
import funcsigs
try:
    print funcsigs.signature(%s)
    # assert 0
except ValueError as e:
    print e
    """ % meth_name
    s = s.strip()

    # subprocess.check_call(['gdb', "--args", PYTHON_EXE, '-c', s])
    subprocess.check_call([PYTHON_EXE, '-c', s])

def test_works(meth_name):
    s = """
import funcsigs
print funcsigs.signature(%s)
    """ % meth_name
    s = s.strip()

    subprocess.check_call([PYTHON_EXE, '-c', s])

test_error("file.__init__")
test_error("open('/dev/null').__init__")
test_error("type")
test_error("all")
test_error("min")
test_error("sum")
test_error('"".count')

test_works("lambda x, *args: 1")

# print type(type.__dict__['__call__'])
# print type(type.__call__)
# print type(all.__call__)
