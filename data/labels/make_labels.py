import os

l = ["person","bicycle","car","motorcycle","airplane","bus","train","truck","boat","traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake","chair","couch","potted plant","bed","dining table","toilet","tv","laptop","mouse","remote","keyboard","cell phone","microwave","oven","toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush", "aeroplane", "bicycle", "bird", "boat", "bottle", "bus", "car", "cat", "chair", "cow", "diningtable", "dog", "horse", "motorbike", "person", "pottedplant", "sheep", "sofa", "train", "tvmonitor"]

l = ["Bomber","Blouse","Cardigan","Button-Down","Blazer","Top","Tee","Turtleneck","Tank","Halter","Jacket","Flannel","Jersey","Hoodie","Sweater","Parka","Capris","Peacoat","Poncho","Henley","Sweatpants","Skirt","Jeggings","Leggings","Sweatshorts","Jodhpurs","Gauchos","Trunks","Sarong","Cutoffs","Culottes","Caftan","Shorts","Jeans","Joggers","Chinos","Jumpsuit","Shirtdress","Coverup","Nightdress","Kaftan","Romper","Cape","Kimono","Dress","Robe"]

import shutil

for word in l:
    #os.system("convert -fill black -background white -bordercolor white -border 4 -font futura-normal-regular -pointsize 18 label:\"%s\" \"%s.png\""%(word, word))
    os.system("convert -size 100x50 xc:none -fill black -gravity center -background white -bordercolor white -border 4 -pointsize 18 -draw \"text 0,0  %s\"  %s.png"%(word, word))
    #shutil.copy("aeroplane.png", word.strip()+".png")
    #os.system("convert -font futura-normal-regular -pointsize 18 -bordercolor white -border 4 -gravity center     -draw \"text 0,0 '%s'\" %s.jpg" % (word, word))
