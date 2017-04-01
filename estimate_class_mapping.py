#@!/usr/bin/env python
from operator import itemgetter
images_dir = "/mount/SDF/DeepFashion/images/"

import os
import glob

CLASSES = []
for _dir in os.listdir(images_dir):
    files = glob.glob(images_dir+_dir+"/*")
    for _file in files:
        label_index = open(_file.replace("images", "labels").replace(".jpg",".txt")).read().split()[0]
        print _dir, label_index
        CLASSES.append((int(label_index), _dir))
        break


CLASSES = sorted(CLASSES, key=itemgetter(0))
print CLASSES
s = ""
for _c in CLASSES:
    s+= _c[1]+"\n"


open("classes.txt", "w").write(s)
