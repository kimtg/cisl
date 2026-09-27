import re

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

pos = text.find('id="predefined_classes"')
if pos != -1:
    snippet = text[pos:pos+4000]
    snippet_clean = re.sub(r'<[^>]+>', ' ', snippet)
    print(snippet_clean[:2000])
