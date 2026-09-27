import re
import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

def show_def(id_name):
    pos = text.find(f'id="{id_name}"')
    if pos != -1:
        print(f"=== {id_name} ===")
        snippet = text[pos:pos+1500]
        snippet_clean = re.sub(r'<[^>]+>', ' ', snippet)
        print(snippet_clean[:600])

for name in ['s_block', 's_return_from', 's_tagbody', 's_go', 's_catch', 's_throw', 's_unwind_protect', 's_convert']:
    show_def(name)
