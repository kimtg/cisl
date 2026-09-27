import re
from collections import Counter

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Let's inspect the syntax and definition blocks
# In ISLISP draft, definitions often look like:
# <h3 id="f_...">...</h3> or <div class="definition"> or similar
# Let's search for tags with id starting with f_, s_, m_, c_, etc.
ids = re.findall(r'id="([fsmedc]_[^"]+)"', text)
print(f"Total IDs matching [fsmedc]_*: {len(ids)}")

definitions = []
# Find patterns like id="something" and look at surrounding tags or headings
for m in re.finditer(r'<(h[2-4]|div|p|dt|b)[^>]*id="([a-zA-Z0-9_-]+)"[^>]*>(.*?)</\1>', text, re.DOTALL):
    tag, id_, inner = m.groups()
    clean = re.sub(r'<[^>]+>', ' ', inner).strip()
    definitions.append((id_, tag, clean))

with open('definitions_list.txt', 'w', encoding='utf-8') as out:
    for id_, tag, name in definitions:
        out.write(f"[{id_}] ({tag}) {name}\n")

print(f"Total definitions written: {len(definitions)}")
