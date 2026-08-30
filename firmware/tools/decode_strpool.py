import re, sys
raw = open('src/strpool.c','r',encoding='utf-8').read()
start = raw.find('strpool_blob')
start = raw.find('"', start)
end = raw.find('};', start)
seg = raw[start:end]
blob = b''
for s in re.findall(r'"([^"]*)"', seg):
    i = 0
    while i < len(s):
        if s[i] == '\\' and s[i+1] == 'x':
            blob += bytes([int(s[i+2:i+4],16)])
            i += 4
        else:
            blob += s[i].encode('latin1')
            i += 1
print('blob len:', len(blob))
s = blob.decode('gbk', errors='replace')
i = 0
frags = []
while i < len(s):
    if s[i] != '\x00' and (s[i].isprintable() or s[i] in '％：:%., \t'):
        j = i
        while j < len(s) and s[j] != '\x00' and (s[j].isprintable() or s[j] in '％：:%., \t') and j-i < 40:
            j += 1
        f = s[i:j].strip()
        if len(f) >= 2:
            frags.append((i, f))
        i = j
    else:
        i += 1
for off, f in frags:
    print('0x%04x: %r' % (off, f))
