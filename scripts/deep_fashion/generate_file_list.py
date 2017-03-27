#!/usr/bin/env python

import glob
import os

_parent = "../../../../DeepFashion/"

files = glob.glob(_parent+"images/*/*")
import random

random.shuffle(files)

files = [os.path.abspath(x) for x in files]

percent_train = 0.95
train_length = int(0.95*len(files))

open(_parent+"filelist_train.txt", "w").write("\n".join(files[:train_length]))
open(_parent+"filelist_val.txt", "w").write("\n".join(files[train_length:]))

#root@iccluster043:/mount/SDF/YOLO/darknet$ ./darknet yolo train cfg/deep_fashion.cfg ../../DeepFashion/darknet_weights/extraction.conv.weights -c_fl_train ../../DeepFashion/filelist_train.txt -c_dir_backup ../snapshots/ -c_classes ../../DeepFashion/classes.txt
