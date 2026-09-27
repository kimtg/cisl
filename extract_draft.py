import re

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Find all section headers
sections = re.findall(r'<h([2-4])[^>]*id="([^"]+)"[^>]*>(.*?)</h\1>', text)
print(f"Total sections found: {len(sections)}")
with open('sections.txt', 'w', encoding='utf-8') as out:
    for level, id_, title in sections:
        title_clean = re.sub(r'<[^>]+>', '', title).strip()
        out.write(f"H{level} [{id_}] {title_clean}\n")

print("Saved to sections.txt")
