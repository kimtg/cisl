import re
import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

pos = text.find('id="def_defmacro"')
if pos != -1:
    snippet = re.sub(r'<[^>]+>', ' ', text[pos:pos+2000])
    print(snippet[:1000])
