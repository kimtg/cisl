import re

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Let's search for case sensitivity in textual representation / lexemes
for section in ['lexemes', 'separators', 'textual_representation', 'symbol_class']:
    pos = text.find(f'id="{section}"')
    if pos != -1:
        print(f"=== {section} ===")
        snippet = text[pos:pos+1500]
        snippet_clean = re.sub(r'<[^>]+>', ' ', snippet)
        print(snippet_clean[:500])
