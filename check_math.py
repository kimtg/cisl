import re
import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

for name in ['f_quotient', 'f_div', 'f_mod', 'f_remainder', 'f_math_eq']:
    pos = text.find(f'id="{name}"')
    if pos != -1:
        print(f"=== {name} ===")
        snippet = re.sub(r'<[^>]+>', ' ', text[pos:pos+1500])
        print(snippet[:500])
