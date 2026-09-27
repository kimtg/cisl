import re

with open('doc/Programming Language ISLISP Working Draft 23.0.html', 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Let's search for "defun" or "defclass" or "+" or "flet" in the HTML to see how they are marked up
m = re.search(r'(<[^>]+id="[^"]*defun[^"]*"[^>]*>.*?</(div|p|h[1-5]))', text, re.DOTALL | re.IGNORECASE)
if m:
    print("Found defun match:")
    print(m.group(0)[:500])
else:
    print("No direct id match for defun")
    # let's search for text "defun"
    pos = text.find('defun')
    if pos != -1:
        print("Surrounding text for defun:")
        print(text[pos-100:pos+300])
