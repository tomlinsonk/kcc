

import hashlib
import os
import signal
import subprocess
import time

# Thanks, https://stackoverflow.com/a/59056796
def get_hash(fname):
    with open(fname, "rb") as f:
        file_hash = hashlib.blake2b()
        while chunk := f.read(8192):
            file_hash.update(chunk)

    return file_hash.hexdigest()



if __name__ == '__main__':
    prev_hash = None

    while True:
        new_hash = get_hash('input.txt')
        if new_hash == prev_hash:
            time.sleep(0.01)
            continue

        prev_hash = new_hash

        os.system('clear')

        with subprocess.Popen('./kcc', stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True) as p:
            for line in p.stdout:
                print(line, end='')

            p.wait()
            if p.returncode == -signal.SIGSEGV:
                print('😳 segfault') 
            elif p.returncode != 0:
                print(f'😵 failed with return code {p.returncode}')