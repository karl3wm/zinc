#!/usr/bin/env python3

import sys
from common import arg_to_code_output

if len(sys.argv[1:]) == 0:
    raise Exception("no filenames provided")

usermsg = sys.stdin.read()

print('<|begin_of_text|><|start_header_id|>user<|end_header_id|>', end='\n\n')

print(usermsg.strip(), end='')
for fn in sys.argv[1:]:
    code, output = arg_to_code_output(fn)
    print('\n\n', end='')
    print('**' + fn + '**')
    print('```')
    print(output, end='')
    print('```', end='')
print('<|eot_id|><|start_header_id|>assistant<|end_header_id|>', end='\n\n')

