#!/usr/bin env python

import uuid
from PIL import Image
import operator


f = open("list_category_cloth.txt", "r").readlines()[2:]

MAP = {}
for _idx, _line in enumerate(f):
    MAP[_idx] = _line.split()[0].strip()

import shutil
import os


## Build Labels Map
labels = open("list_bbox.txt", "r").readlines()[2:]
labelsMap = {}
for _label in labels:
    labelsMap[_label.split()[0]] = _label.split()[1:]

def convert(size, box):
    box = [float(x) for x in box]
    dw = 1./size[0]
    dh = 1./size[1]
    x = (box[0] + box[2])/2.0
    y = (box[1] + box[3])/2.0
    w = abs(box[0] - box[2])
    h = abs(box[1] - box[3])
    x = x*dw
    w = w*dw
    y = y*dh
    h = h*dh
    return (x,y,w,h)



f = open("list_category_img.txt", "r").readlines()[2:]

MAP_COUNT = 0
LABELS_MAP = {}

for _line in f:
    _line = _line.split()
    fileName = _line[0]
    categoryName = _line[1]
    FOLDER_NAME = MAP[int(categoryName)]

    try:
        foo = LABELS_MAP[MAP[int(categoryName)]]
    except:
        LABELS_MAP[MAP[int(categoryName)]] = MAP_COUNT
        MAP_COUNT += 1


    print FOLDER_NAME
    try:
        os.mkdir("images/"+FOLDER_NAME)
    except:
        pass

    try:
        os.mkdir("labels/"+FOLDER_NAME)
    except:
        pass

    targetFileName = "images/"+FOLDER_NAME+"/"+str(uuid.uuid4())+"_"+fileName.split("/")[-1]
    shutil.copy(fileName, targetFileName)

    im = Image.open(fileName)

    L = convert(im.size, labelsMap[fileName])
    L = [str(x) for x in L]
    target_labels_fileName = targetFileName.replace("images/", "labels/").replace(".jpg", ".txt").replace(".JPEG", ".txt")

    open(target_labels_fileName, "w").write(str(LABELS_MAP[MAP[int(categoryName)]])+" "+" ".join(L)+"\n")
    print fileName

print "Writing sorted class map to classes.txt"
sorted_labels_map = sorted(LABELS_MAP.items(), key=operator.itemgetter(1))
classes = []
for _x in sorted_labels_map:
    classes.append(_x[0])
open("classes.txt", "w").write("\n".join(classes))
