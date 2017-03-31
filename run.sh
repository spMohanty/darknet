#!/bin/bash


unbuffer ./darknet yolo train cfg/deep_fashion.cfg ../../DeepFashion/darknet_weights/extraction.conv.weights -c_fl_train ../../DeepFashion/filelist_train.txt -c_fl_val ../../DeepFashion/filelist_val.txt -c_dir_backup ../snapshots -c_classes ../../DeepFashion/classes.txt | tee results.txt
