#!/usr/bin/env python

classes = open("classes.txt").readlines()

output = 'l = ['
for _c in classes:
    output += '"' + _c.strip() +'",'

output = output[:-1] + "]"
print output
