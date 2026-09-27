import re
import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

for name in ['def_defclass', 'def_defgeneric', 'def_defmethod', 'standard_method_combination', 'l_call_next_method']:
    pos = text.find(f'id="{name}"')
    if pos != -1:
        print(f"=== {name} ===")
        snippet = re.sub(r'<[^>]+>', ' ', text[pos:pos+1500])
        print(snippet[:500])
