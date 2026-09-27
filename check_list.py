import re
import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

for name in ['f_member', 'f_assoc', 'f_elt', 'f_subseq', 'f_map_into']:
    pos = text.find(f'id="{name}"')
    if pos != -1:
        print(f"=== {name} ===")
        snippet = re.sub(r'<[^>]+>', ' ', text[pos:pos+1500])
        print(snippet[:500])
