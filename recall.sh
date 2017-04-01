#!/bin/bash

./darknet yolo recall cfg/deep_fashion.cfg ../snapshots/deep_fashion_7000.weights -c_fl_val val_small.txt -c_classes ../../DeepFashion/classes.txt
