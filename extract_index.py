import re

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Let's see the index section at the end
idx_match = re.search(r'<h2[^>]*id="index"[^>]*>(.*)', text, re.DOTALL)
if idx_match:
    idx_content = idx_match.group(1)
    # find links or table items in index
    items = re.findall(r'<a\s+href="[^"]*#([^"]+)"[^>]*>([^<]+)</a>', idx_content)
    print(f"Total index entries: {len(items)}")
    with open('index_entries.txt', 'w', encoding='utf-8') as out:
        for href, name in items:
            out.write(f"{name.strip()} -> {href}\n")
    print("Saved index_entries.txt")
