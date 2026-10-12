import sys, os
from PIL import Image
quelle, ziel, faktor = sys.argv[1], sys.argv[2], int(sys.argv[3])
for name in sorted(os.listdir(quelle)):
    if not name.endswith('.png'):
        continue
    if 'test_bh' not in name and 'bh' not in quelle and 'qemu' not in quelle:
        pass
    p = os.path.join(quelle, name)
    try:
        im = Image.open(p).convert('RGB')
    except Exception as e:
        print('fehler', name, e); continue
    if 'test_images' in quelle and not name.startswith('test_bh_karten'):
        continue
    w, h = im.size
    im2 = im.resize((w*faktor, h*faktor), Image.NEAREST)
    im2.save(os.path.join(ziel, name.replace('~','_')))
    print(name, w, h)
