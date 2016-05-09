#include "network.h"
#include "detection_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"

#ifdef OPENCV
#include "opencv2/highgui/highgui_c.h"
#endif

/* Change class number here */
#define CLASSNUM 2

/* Change class names here */
// char *voc_names[] = {"stopsign", "yeildsign"};
// image voc_labels[CLASSNUM];

char *voc_names[1024];//"ape"};
int i_num_cl = 1;
image voc_labels[1024];

void train_yolo(  char *cfgfile,
                  char *weightfile,
                  const char * c_fl_train,
                  const char * c_dir_backup
                )
{
    srand(time(0));
    data_seed  = time(0);
    // read configuration and print main settings to terminal
    char *base = basecfg(cfgfile);
    printf("%s\n", base);

    // read network from config file
    float avg_loss = -1;
    network net = parse_network_cfg(cfgfile);
    // load pre-trained weights if available
    if(weightfile){
        load_weights(&net, weightfile);
    }
    // print relevant information of training to terminal
    printf("Learning Rate: %g, Momentum: %g, Decay: %g\n", net.learning_rate, net.momentum, net.decay);

    int imgs   = net.batch*net.subdivisions;
    int i_iter = *net.seen/imgs;
    data train, buffer;


    // get last layer of the network to compute loss thereof
    // -> this only supports networks with a single output layer
    layer l  = net.layers[net.n - 1];

    int side       = l.side;
    int classes    = l.classes;
    float jitter   = l.jitter;

    list *plist    = get_paths( c_fl_train );
    char **paths   = (char **)list_to_array(plist);

    load_args args = {0};
    args.w         = net.w;
    args.h         = net.h;
    args.paths     = paths;
    args.n         = imgs;
    args.m         = plist->size;
    args.classes   = classes;
    args.jitter    = jitter;
    args.num_boxes = side;
    args.d         = &buffer;
    args.type      = REGION_DATA;
    //
    args.c_ending  = (char *)malloc(strlen(net.c_ending_gt_files)+1);
    strcpy(args.c_ending,net.c_ending_gt_files);

    int i_snapshot_iteration = net.i_snapshot_iteration;

    // prepare data into chunks
    pthread_t load_thread = load_data_in_thread(args);

    // start training for net.max_batches "iterations"
    clock_t time;
    while(get_current_batch(net) < net.max_batches)
    {
        i_iter += 1;

        time=clock();
        // wait until all images for the current thread are loaded
        pthread_join(load_thread, 0);
        train = buffer;
        load_thread = load_data_in_thread(args);

        printf("Loaded next batch: %lf seconds\n", sec(clock()-time));

        // apply forward and backward pass to all images within the current batch (stored in net.batch)
        time=clock();
        float loss = train_network(net, train);

        // average loss over last iterations for visualization / monitoring to avoid heavily zigzagging curves
        if (avg_loss < 0)
        {
            avg_loss = loss;
        }
        avg_loss = avg_loss*.9 + loss*.1;

        printf("%d: %f, %f avg, %f rate, %lf seconds, %d images\n", i_iter, loss, avg_loss, get_current_rate(net), sec(clock()-time), i_iter*imgs);

        // snapshot weights after every xxx "iterations"
        if( ( i_iter%i_snapshot_iteration ) ==0 )
        {
            char buff[256];
            sprintf(buff, "%s/%s_%d.weights", c_dir_backup, base, i_iter);
            save_weights(net, buff);
        }
        //free dynamic memory
        free_data(train);
    }
    // we're done, save final weight estimates and say good bye...
    char buff[256];
    sprintf(buff, "%s/%s_final.weights", c_dir_backup, base);
    save_weights(net, buff);
}

void convert_yolo_detections(float *predictions, int classes, int num, int square, int side, int w, int h, float thresh, float **probs, box *boxes, int only_objectness)
{
    int i,j,n;
    //int per_cell = 5*num+classes;
    for (i = 0; i < side*side; ++i){
        int row = i / side;
        int col = i % side;
        for(n = 0; n < num; ++n){
            int index = i*num + n;
            int p_index = side*side*classes + i*num + n;
            float scale = predictions[p_index];
            int box_index = side*side*(classes + num) + (i*num + n)*4;
            boxes[index].x = (predictions[box_index + 0] + col) / side * w;
            boxes[index].y = (predictions[box_index + 1] + row) / side * h;
            boxes[index].w = pow(predictions[box_index + 2], (square?2:1)) * w;
            boxes[index].h = pow(predictions[box_index + 3], (square?2:1)) * h;
            for(j = 0; j < classes; ++j){
                int class_index = i*classes;
                float prob = scale*predictions[class_index+j];
                probs[index][j] = (prob > thresh) ? prob : 0;
            }
            if(only_objectness){
                probs[index][0] = scale;
            }
        }
    }
}

void print_yolo_detections(FILE **fps, char *id, box *boxes, float **probs, int total, int classes, int w, int h)
{
    int i, j;
    for(i = 0; i < total; ++i){
        float xmin = boxes[i].x - boxes[i].w/2.;
        float xmax = boxes[i].x + boxes[i].w/2.;
        float ymin = boxes[i].y - boxes[i].h/2.;
        float ymax = boxes[i].y + boxes[i].h/2.;

        if (xmin < 0) xmin = 0;
        if (ymin < 0) ymin = 0;
        if (xmax > w) xmax = w;
        if (ymax > h) ymax = h;

        for(j = 0; j < classes; ++j){
            if (probs[i][j]) fprintf(fps[j], "%s %f %f %f %f %f\n", id, probs[i][j],
                    xmin, ymin, xmax, ymax);
        }
    }
}

void validate_yolo(char *cfgfile, char *weightfile)
{
    network net = parse_network_cfg(cfgfile);
    if(weightfile){
        load_weights(&net, weightfile);
    }
    set_batch_network(&net, 1);
    fprintf(stderr, "Learning Rate: %g, Momentum: %g, Decay: %g\n", net.learning_rate, net.momentum, net.decay);
    srand(time(0));

    char *base = "results/comp4_det_test_";
    list *plist = get_paths("data/voc.2007.test");
    //list *plist = get_paths("data/voc.2012.test");
    char **paths = (char **)list_to_array(plist);

    layer l = net.layers[net.n-1];
    int classes = l.classes;
    int square = l.sqrt;
    int side = l.side;

    int j;
    FILE **fps = calloc(classes, sizeof(FILE *));
    for(j = 0; j < classes; ++j){
        char buff[1024];
        snprintf(buff, 1024, "%s%s.txt", base, voc_names[j]);
        fps[j] = fopen(buff, "w");
    }
    box *boxes = calloc(side*side*l.n, sizeof(box));
    float **probs = calloc(side*side*l.n, sizeof(float *));
    for(j = 0; j < side*side*l.n; ++j) probs[j] = calloc(classes, sizeof(float *));

    int m = plist->size;
    int i=0;
    int t;

    float thresh = .001;
    int nms = 1;
    float iou_thresh = .5;

    int nthreads = 2;
    image *val = calloc(nthreads, sizeof(image));
    image *val_resized = calloc(nthreads, sizeof(image));
    image *buf = calloc(nthreads, sizeof(image));
    image *buf_resized = calloc(nthreads, sizeof(image));
    pthread_t *thr = calloc(nthreads, sizeof(pthread_t));

    load_args args = {0};
    args.w = net.w;
    args.h = net.h;
    args.type = IMAGE_DATA;

    for(t = 0; t < nthreads; ++t){
        args.path = paths[i+t];
        args.im = &buf[t];
        args.resized = &buf_resized[t];
        thr[t] = load_data_in_thread(args);
    }
    time_t start = time(0);
    for(i = nthreads; i < m+nthreads; i += nthreads){
        fprintf(stderr, "%d\n", i);
        for(t = 0; t < nthreads && i+t-nthreads < m; ++t){
            pthread_join(thr[t], 0);
            val[t] = buf[t];
            val_resized[t] = buf_resized[t];
        }
        for(t = 0; t < nthreads && i+t < m; ++t){
            args.path = paths[i+t];
            args.im = &buf[t];
            args.resized = &buf_resized[t];
            thr[t] = load_data_in_thread(args);
        }
        for(t = 0; t < nthreads && i+t-nthreads < m; ++t){
            char *path = paths[i+t-nthreads];
            char *id = basecfg(path);
            float *X = val_resized[t].data;
            float *predictions = network_predict(net, X);
            int w = val[t].w;
            int h = val[t].h;
            convert_yolo_detections(predictions, classes, l.n, square, side, w, h, thresh, probs, boxes, 0);
            if (nms) do_nms_sort(boxes, probs, side*side*l.n, classes, iou_thresh);
            print_yolo_detections(fps, id, boxes, probs, side*side*l.n, classes, w, h);
            free(id);
            free_image(val[t]);
            free_image(val_resized[t]);
        }
    }
    fprintf(stderr, "Total Detection Time: %f Seconds\n", (double)(time(0) - start));
}

void validate_yolo_recall(char *cfgfile, char *weightfile)
{
    network net = parse_network_cfg(cfgfile);
    if(weightfile){
        load_weights(&net, weightfile);
    }
    set_batch_network(&net, 1);
    fprintf(stderr, "Learning Rate: %g, Momentum: %g, Decay: %g\n", net.learning_rate, net.momentum, net.decay);
    srand(time(0));

    char *base = "results/comp4_det_test_";
    list *plist = get_paths("data/voc.2007.test");
    char **paths = (char **)list_to_array(plist);

    layer l = net.layers[net.n-1];
    int classes = l.classes;
    int square = l.sqrt;
    int side = l.side;

    int j, k;
    FILE **fps = calloc(classes, sizeof(FILE *));
    for(j = 0; j < classes; ++j){
        char buff[1024];
        snprintf(buff, 1024, "%s%s.txt", base, voc_names[j]);
        fps[j] = fopen(buff, "w");
    }
    box *boxes = calloc(side*side*l.n, sizeof(box));
    float **probs = calloc(side*side*l.n, sizeof(float *));
    for(j = 0; j < side*side*l.n; ++j) probs[j] = calloc(classes, sizeof(float *));

    int m = plist->size;
    int i=0;

    float thresh = .001;
    int nms = 0;
    float iou_thresh = .5;
    float nms_thresh = .5;

    int total = 0;
    int correct = 0;
    int proposals = 0;
    float avg_iou = 0;

    for(i = 0; i < m; ++i){
        char *path = paths[i];
        image orig = load_image_color(path, 0, 0);
        image sized = resize_image(orig, net.w, net.h);
        char *id = basecfg(path);
        float *predictions = network_predict(net, sized.data);
        convert_yolo_detections(predictions, classes, l.n, square, side, 1, 1, thresh, probs, boxes, 1);
        if (nms) do_nms(boxes, probs, side*side*l.n, 1, nms_thresh);

        char *labelpath = find_replace(path, "images", "labels");
        labelpath = find_replace(labelpath, "JPEGImages", "labels");
        labelpath = find_replace(labelpath, ".jpg", ".txt");
        labelpath = find_replace(labelpath, ".JPEG", ".txt");
        labelpath = find_replace(labelpath, ".png", ".txt");	

        int num_labels = 0;
        box_label *truth = read_boxes(labelpath, &num_labels);
        for(k = 0; k < side*side*l.n; ++k){
            if(probs[k][0] > thresh){
                ++proposals;
            }
        }
        for (j = 0; j < num_labels; ++j) {
            ++total;
            box t = {truth[j].x, truth[j].y, truth[j].w, truth[j].h};
            float best_iou = 0;
            for(k = 0; k < side*side*l.n; ++k){
                float iou = box_iou(boxes[k], t);
                if(probs[k][0] > thresh && iou > best_iou){
                    best_iou = iou;
                }
            }
            avg_iou += best_iou;
            if(best_iou > iou_thresh){
                ++correct;
            }
        }

        fprintf(stderr, "%5d %5d %5d\tRPs/Img: %.2f\tIOU: %.2f%%\tRecall:%.2f%%\n", i, correct, total, (float)proposals/(i+1), avg_iou*100/total, 100.*correct/total);
        free(id);
        free_image(orig);
        free_image(sized);
    }
}

void test_yolo(  char *cfgfile,
                 char *weightfile,
                 char *c_filename,
                 float thresh,
                 const bool b_draw_detections,
                 const bool b_write_detections,
                 const char * c_dest,
                 const float f_nms_threshold
               )
{

    network net = parse_network_cfg(cfgfile);
    if(weightfile){
        load_weights(&net, weightfile);
    }
    detection_layer l = net.layers[net.n-1];
    set_batch_network(&net, 1);
    srand(2222222);
    clock_t time;
    char buff[256];
    char *input = buff;
    int j;

    box *boxes = calloc(l.side*l.side*l.n, sizeof(box));
    float **probs = calloc(l.side*l.side*l.n, sizeof(float *));
    for(j = 0; j < l.side*l.side*l.n; ++j)
    {
        probs[j] = calloc(l.classes, sizeof(float *));
    }

    int index;
    index = 0;
    //
    while(1){
        if(c_filename){
            strncpy(input, c_filename, 256);
        } else {
            printf("Enter Image Path: ");
            fflush(stdout);
            input = fgets(input, 256, stdin);
            if(!input) return;
            strtok(input, "\n");
        }

        // load image from given filename
        image im = load_image_color(input,0,0);

        // adapt image to network input size
        image sized = resize_image(im, net.w, net.h);
        float *X = sized.data;

        // do actual prediction
        time=clock();
        float *predictions = network_predict(net, X);
        printf("%s Predicted in %f seconds.\n", input, sec(clock()-time));

        // convert results to original image size
        convert_yolo_detections(predictions, l.classes, l.n, l.sqrt, l.side, 1, 1, thresh, probs, boxes, 0);

        // apply non-maximum suppresion to filter overlapping responses
        if (f_nms_threshold)
        {
            do_nms_sort(boxes, probs, l.side*l.side*l.n, l.classes, f_nms_threshold);
        }


        // draw detections into image and show or write result
        if ( b_draw_detections )
        {
            //draw_detections(im, l.side*l.side*l.n, thresh, boxes, probs, voc_names, voc_labels, i_num_cl);
            draw_detections(im, l.side*l.side*l.n, thresh, boxes, probs, voc_names, 0, i_num_cl);
            show_image(im, "predictions");
            save_image(im, "predictions");

            show_image(sized, "resized");
        }


        // save bounding boxes to separate text file
        if ( b_write_detections )
        {
            FILE *fout_box    = fopen(c_dest, "a");
            int ibox;
            int numbox = l.side*l.side*l.n;

            for(ibox = 0; ibox < numbox; ++ibox)
            {
                int class = max_index(probs[ibox], l.classes);
                float prob = probs[ibox][class];
                if ( prob < thresh )
                    continue;
                box b = boxes[ibox];

                int left  = (b.x-b.w/2.)*im.w;
                int right = (b.x+b.w/2.)*im.w;
                int top   = (b.y-b.h/2.)*im.h;
                int bot   = (b.y+b.h/2.)*im.h;

                if(left < 0) left = 0;
                if(right > im.w-1) right = im.w-1;
                if(top < 0) top = 0;
                if(bot > im.h-1) bot = im.h-1;

                fprintf(fout_box, "%s %d %d %d %d %f class %d \n", input, left, top, right-left, bot-top, prob, class );
                //printf("%s %d %d %d %d %f\n", input, left, right, top, bot, prob );
            }
            fclose(fout_box);

            index++;
        }

         //free memory
        free_image(im);
        free_image(sized);
#ifdef OPENCV
        if ( b_draw_detections )
        {
        cvWaitKey(0);
        cvDestroyAllWindows();
        }
#endif
        // break interactive detection loop if no new filename was entered
        if (c_filename)
        {
            break;
        }
    }
}

void test_yolo_on_filelist(  char *cfgfile,
                             char *weightfile,
                             char *c_filelist,
                             float thresh,
                             const bool b_draw_detections,
                             const bool b_write_detections,
                             const char * c_dest,
                             const float f_nms_threshold
                           )
{

    // step 1 - read network and load pre-trained weights
    // this needs to be done only once
    //
    network net = parse_network_cfg(cfgfile);
    if(weightfile){
        load_weights(&net, weightfile);
    }
    detection_layer l = net.layers[net.n-1];
    set_batch_network(&net, 1);
    //
    // step 2 - prepare everything for detection
    srand(2222222);
    clock_t time;

    int j;
    // allocate memory for prediction results
    box *boxes = calloc(l.side*l.side*l.n, sizeof(box));
    float **probs = calloc(l.side*l.side*l.n, sizeof(float *));
    for(j = 0; j < l.side*l.side*l.n; ++j)
    {
        probs[j] = calloc(l.classes, sizeof(float *));
    }


    FILE * fp_filelist;
    char * line = NULL;
    size_t len = 0;
    ssize_t i_line_length;

    fp_filelist = fopen( c_filelist, "r" );
    if (fp_filelist == NULL)
    {
        printf("Filelist is not readable!");
        return;
    }

    int index;
    index = 0;
    // now read line by line, i.e., filename after filename for all test images
    while ( (i_line_length = getline(&line, &len, fp_filelist)) != -1)
    {
        // read filename
        char c_filename [i_line_length];
        // remove newline character which is read returned by getline, too
        memcpy(c_filename, line+ 0 /* Offset */, i_line_length-1 /* Length */);
        // correctly end the char array in c syntax
        c_filename [i_line_length-1] = '\0';

        // load image from given filename
        image im = load_image_color(c_filename,0,0);

        // adapt image to network input size
        image sized = resize_image(im, net.w, net.h);
        float *X = sized.data;

        // do actual prediction
        time=clock();
        float *predictions = network_predict(net, X);
	
        printf("%s predicted in %f seconds.\n", c_filename, sec(clock()-time));

        // convert results to original image size
        convert_yolo_detections(predictions, l.classes, l.n, l.sqrt, l.side, 1, 1, thresh, probs, boxes, 0);
        // apply non-maximum suppresion to filter overlapping responses
        if (f_nms_threshold)
        {
            do_nms_sort(boxes, probs, l.side*l.side*l.n, l.classes, f_nms_threshold);
        }


        // draw detections into image and show or write result
        if ( b_draw_detections )
        {
            //draw_detections(im, l.side*l.side*l.n, thresh, boxes, probs, voc_names, voc_labels, i_num_cl);
            draw_detections(im, l.side*l.side*l.n, thresh, boxes, probs, voc_names, 0, i_num_cl);
            show_image(im, "predictions");
            save_image(im, "predictions");

            show_image(sized, "resized");
        }


        // save bounding boxes to separate text file
        if ( b_write_detections )
        {
            FILE *fout_box    = fopen(c_dest, "a");
            int ibox;
            int numbox = l.side*l.side*l.n;

            for(ibox = 0; ibox < numbox; ++ibox)
            {
                int class = max_index(probs[ibox], l.classes);
                float prob = probs[ibox][class];
                if ( prob < thresh )
                    continue;
                box b = boxes[ibox];

                int left  = (b.x-b.w/2.)*im.w;
                int right = (b.x+b.w/2.)*im.w;
                int top   = (b.y-b.h/2.)*im.h;
                int bot   = (b.y+b.h/2.)*im.h;

                if(left < 0) left = 0;
                if(right > im.w-1) right = im.w-1;
                if(top < 0) top = 0;
                if(bot > im.h-1) bot = im.h-1;

                fprintf(fout_box, "%s %d %d %d %d %f class %d \n", c_filename, left, top, right-left, bot-top, prob, class );
                //printf("%s %d %d %d %d %f\n", input, left, right, top, bot, prob );
            }
            fclose(fout_box);

            index++;
        }

        //free memory
        free_image(im);
        free_image(sized);
#ifdef OPENCV
        if ( b_draw_detections )
        {
        cvWaitKey(0);
        cvDestroyAllWindows();
        }
#endif
    }
    
    fclose( fp_filelist );
    if (line)
    {
        free(line);
    }    
}



/*
#ifdef OPENCV
image ipl_to_image(IplImage* src);
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc_c.h"

void demo_swag(char *cfgfile, char *weightfile, float thresh)
{
network net = parse_network_cfg(cfgfile);
if(weightfile){
load_weights(&net, weightfile);
}
detection_layer layer = net.layers[net.n-1];
CvCapture *capture = cvCaptureFromCAM(-1);
set_batch_network(&net, 1);
srand(2222222);
while(1){
IplImage* frame = cvQueryFrame(capture);
image im = ipl_to_image(frame);
cvReleaseImage(&frame);
rgbgr_image(im);

image sized = resize_image(im, net.w, net.h);
float *X = sized.data;
float *predictions = network_predict(net, X);
draw_swag(im, predictions, layer.side, layer.n, "predictions", thresh);
free_image(im);
free_image(sized);
cvWaitKey(10);
}
}
#else
void demo_swag(char *cfgfile, char *weightfile, float thresh){}
#endif
 */

void demo_yolo(char *cfgfile, char *weightfile, float thresh, int cam_index, char* filename);
#ifndef GPU
void demo_yolo(char *cfgfile, char *weightfile, float thresh, int cam_index, char* filename)
{
    fprintf(stderr, "Darknet must be compiled with CUDA for YOLO demo.\n");
}
#endif

void run_yolo(int argc, char **argv)
{

    char * c_list_with_classnames = find_char_arg(argc, argv, "-c_classes", "./data/classnames.txt");

    FILE * fp_classlist;
    char * line = NULL;
    size_t len = 0;
    ssize_t i_line_length;

    fp_classlist = fopen( c_list_with_classnames, "r" );
    if (fp_classlist == NULL)
    {
        printf("Classlist is not readable!");
        return;
    }

    int i_cl;
    i_cl = 0;
    // now read line by line, i.e., filename after filename for all test images
    while ( (i_line_length = getline(&line, &len, fp_classlist)) != -1)
    {
        // read filename
        char c_classname [i_line_length];
        // remove newline character which is read returned by getline, too
        memcpy(c_classname, line+ 0 /* Offset */, i_line_length-1 /* Length */);
        // correctly end the char array in c syntax
        c_classname [i_line_length-1] = '\0';

        voc_names[i_cl] = c_classname;
//        try
//        {
//            char buff[256];
//            sprintf(buff, "data/labels/%s.png", c_classname);
//            voc_labels[i_cl] = load_image_color(buff, 0, 0);
//        }
//        catch (...)
//        {
            printf("Found no image for class %s - using unknown instead\n", c_classname);
            char buff[256];
            sprintf(buff, "data/labels/unknown.png");
            voc_labels[i_cl] = load_image_color(buff, 0, 0);
//        }

        i_cl = i_cl + 1;
    }
    i_num_cl = i_cl+1;

    float thresh  = find_float_arg(argc, argv, "-thresh", .2);
    int cam_index = find_int_arg(argc, argv, "-c", 0);

    if(argc < 4){
        fprintf(stderr, "usage: %s %s [train/test/valid] [cfg] [weights (optional)]\n", argv[0], argv[1]);
        return;
    }

    char * c_cfg = argv[3];
    char * c_weights = (argc > 4) ? argv[4] : 0;
    if(0==strcmp(argv[2], "test"))
    {
        bool b_draw_detections     = find_bool_arg(argc, argv, "-draw", false);
        bool b_write_detections    = find_bool_arg(argc, argv, "-write", false);
        char * c_dest              = find_char_arg(argc, argv, "-dest", "./bboxes.txt");
        float f_nms_threshold      = find_float_arg(argc, argv, "-nms", 0.5);
        char * c_filename          = (argc > 5) ? argv[5]: 0;

        test_yolo(c_cfg, c_weights, c_filename, thresh, b_draw_detections, b_write_detections, c_dest, f_nms_threshold );
    }
    if(0==strcmp(argv[2], "test_on_filelist"))
    {
        bool b_draw_detections     = find_bool_arg(argc, argv, "-draw", false);
        bool b_write_detections    = find_bool_arg(argc, argv, "-write", false);
        char * c_dest              = find_char_arg(argc, argv, "-dest", "./bboxes.txt");
        float f_nms_threshold      = find_float_arg(argc, argv, "-nms", 0.5);
        char * c_filelist          = (argc > 5) ? argv[5]: 0;

        test_yolo_on_filelist( c_cfg, c_weights, c_filelist, thresh, b_draw_detections, b_write_detections, c_dest, f_nms_threshold );
    }
    else if(0==strcmp(argv[2], "train"))
    {
        char * c_fl_train              = find_char_arg(argc, argv, "-c_fl_train", "./filelist_train.txt");
        char * c_dir_backup            = find_char_arg(argc, argv, "-c_dir_backup", "./");

        train_yolo(c_cfg, c_weights, c_fl_train, c_dir_backup);
    }
    else if(0==strcmp(argv[2], "valid"))
    {
        validate_yolo(c_cfg, c_weights);
    }
    else if(0==strcmp(argv[2], "recall"))
    {
        validate_yolo_recall(c_cfg, c_weights);
    }
    else if(0==strcmp(argv[2], "demo_cam"))
    {
        demo_yolo(c_cfg, c_weights, thresh, cam_index, "NULL");
    }
    else if(0==strcmp(argv[2], "demo_vid"))
    {
      char * c_filename          = (argc > 5) ? argv[5]: 0;
      
      demo_yolo(c_cfg, c_weights, thresh, -1, c_filename);    
    }
}
