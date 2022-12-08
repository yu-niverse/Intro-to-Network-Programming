import os
from subprocess import PIPE, Popen
import subprocess
import difflib
from concurrent.futures import ThreadPoolExecutor
import argparse
import concurrent.futures

parser = argparse.ArgumentParser(
    'Sample demo script\n   To ensure your zip format is correct, please make sure to run this script before submission\n'
)

parser.add_argument('-p', dest='port', type=int, default=8888)
parser.add_argument('-s',
                    '--submission',
                    dest='submission',
                    type=str,
                    default='')
parser.add_argument('-t', '--testcase', dest='testcase', type=str, default='')
args = parser.parse_args()

if args.submission:
    assert args.submission.endswith('.zip'), 'Please run with a zip file'
    filename = args.submission.split('.')[0]
    subprocess.run(['unzip', '-o', args.submission, '-d', filename])
    subprocess.run(['bash', 'build.sh'], cwd=filename)

    BUILD_DIR = f'{filename}/build'
else:
    BUILD_DIR = 'build'

SERVER_YOUR = f'{BUILD_DIR}/server'
SERVER_PORT = args.port

TESTCASES_SRC = "testcases"
TESTCASES_YOUR = os.path.join(BUILD_DIR, 'testcases_your')
TESTCASES_CORRECT = 'testcases_correct'
TESTCASES_DIFF = os.path.join(BUILD_DIR, 'testcases_diff')

os.makedirs(TESTCASES_YOUR, exist_ok=True)
os.makedirs(TESTCASES_DIFF, exist_ok=True)
testcases_src = [args.testcase] if args.testcase else os.listdir(TESTCASES_SRC)


def run_testcase(filename):
    print(f'Running {filename}')
    try:
        proc = Popen(SERVER_YOUR)
    except FileNotFoundError as e:
        print(
            f'{e}\n   Please make sure your server is in {BUILD_DIR}, U might forget to run build.sh'
        )
        exit(1)
    # To ensure server has started listening
    while True:
        p = subprocess.run(['lsof', '-i', f':{SERVER_PORT}'],
                           stdout=PIPE,
                           stderr=PIPE)
        if p.stdout:
            break

    subprocess.run([
        'python3', 'client.py', '--filename', f'{filename}', '--dst',
        f'{TESTCASES_YOUR}'
    ])
    proc.terminate()


def diff_file(filename):
    ta_answer = None
    your_answer = None

    src_file = os.path.join(TESTCASES_CORRECT, filename)
    with open(src_file, 'r') as f:
        ta_answer = f.readlines()

    dst_file = os.path.join(TESTCASES_YOUR, filename)
    with open(dst_file, 'r') as f:
        your_answer = f.readlines()

    assert ta_answer and your_answer
    dif = difflib.context_diff(ta_answer,
                               your_answer,
                               fromfile=src_file,
                               tofile=dst_file)
    dif_list = list(dif)
    if not dif_list:
        return 1

    diff_file = os.path.join(TESTCASES_DIFF, filename)
    with open(diff_file, 'w+') as f:
        f.writelines(iter(dif_list))
    return 0


for filename in testcases_src:
    run_testcase(filename)
    print(f'{filename} finished')

with ThreadPoolExecutor() as executor:
    future_to_filename = {
        executor.submit(diff_file, filename): filename
        for filename in testcases_src
    }
    total_pass_testcases = 0
    for future in concurrent.futures.as_completed(future_to_filename):
        filename = future_to_filename[future]
        try:
            score = future.result()
            if score == 0:
                print(f'testcases/{filename} is not correct')
            else:
                total_pass_testcases += score

        except Exception as e:
            print(e)

    print(f'PASS {total_pass_testcases}/{len(testcases_src)}')
