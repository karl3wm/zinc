#!/usr/bin/env python3

import sys
from common import arg_to_code_output

if len(sys.argv[1:]) == 0:
    raise Exception("no filenames provided")

usermsg = sys.stdin.read()

print('<|begin_of_text|><|start_header_id|>system<|end_header_id|>', end='\n\n')

print('''Environment: ipython
Tools: brave_search, wolfram_alpha
<|eot_id|><|start_header_id|>user<|end_header_id|>''', end='\n\n')

print(usermsg.strip() + '<|eot_id|><|start_header_id|>assistant<|end_header_id|>', end='\n\n')

for fn in sys.argv[1:]:
    code, output = arg_to_code_output(fn)
    print(f'<|python_tag|>{code}<|eom_id|><|start_header_id|>ipython<|end_header_id|>', end='\n\n')
    print(output, end='')
    print('<|eot_id|><|start_header_id|>assistant<|end_header_id|>', end='\n\n')
