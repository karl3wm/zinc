import io
import os

import sys
import subprocess

subprocess_objs = []

def arg_to_code_output(arg):
    if arg.startswith('`') and arg.endswith('`'):
        # Run shell command
        code = f'import subprocess\nsubprocess.run({repr(arg[1:-1])}, shell=True)'
    elif arg.startswith('http://') or arg.startswith('https://') or ('.' in arg and not os.path.exists(arg)):
        # Request web URL
        code = f'import requests\nprint(requests.get({repr(arg)}).text)'
    else:
        # Assume it's a filename
        code = f'print(open({repr(arg)}).read())'

    prevs = {x: eval(x) for x in ['sys.stdout', 'subprocess.Popen._get_handles']}

    pipe_read, pipe_write = os.pipe()
    pipe_read = os.fdopen(pipe_read, 'r')
    pipe_write = os.fdopen(pipe_write, 'w')
    sys.stdout = pipe_write
    subprocess.Popen._get_handles = (
        lambda self, stdin, stdout, stderr:
            prevs['subprocess.Popen._get_handles'](self, stdin, pipe_write if stdout is None else stdout, stderr)
    )

    #def output_capturing_subprocess_get_handles(self, stdin, stdout, stderr):
    #    if stdout is None:
    #        subprocess_objs.append(self)
    #        stdout = subprocess.PIPE
    #    return prevs['subprocess.Popen._get_handles'](self, stdin, stdout, stderr)

    try:
        with pipe_read:
            with pipe_write:
                exec(code)
            output = pipe_read.read()
        return code, output

    finally:
        for prev, val in prevs.items():
            prev, attr = prev.rsplit('.', 1)
            setattr(eval(prev), attr, val)
