import re

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

defs = re.findall(r'<div\s+id="([^"]+)"\s+class="definition-([^"]+)">\s*(.*?)\s*</div>', text, re.DOTALL)
print(f"Total definition divs found: {len(defs)}")

categories = {}
for id_, kind, content in defs:
    clean = re.sub(r'\s+', ' ', re.sub(r'<[^>]+>', ' ', content)).strip()
    categories.setdefault(kind, []).append((id_, clean))

with open('all_islisp_definitions.txt', 'w', encoding='utf-8') as out:
    for kind, items in categories.items():
        out.write(f"\n=== KIND: {kind} ({len(items)} items) ===\n")
        for id_, clean in items:
            out.write(f"  [{id_}] {clean}\n")

print("Saved all_islisp_definitions.txt")
