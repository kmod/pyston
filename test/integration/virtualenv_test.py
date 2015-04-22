import os
import sys
import subprocess
import shutil

VIRTUALENV_SCRIPT = os.path.dirname(__file__) + "/virtualenv/virtualenv.py"

if os.path.exists("test_env"):
    print "Removing the existing 'test_env/' directory"
    subprocess.check_call(["rm", "-rf", "test_env"])
    # shutil follows symlinks to directories, and deletes whatever those contain.
    # shutil.rmtree("test_env")

args = [sys.executable, VIRTUALENV_SCRIPT, "-p", sys.executable, "test_env"]
print "Running", args
subprocess.check_call(args)
