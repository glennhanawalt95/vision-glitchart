#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "image.h"


// Indexing

int clamp_padding(image im, int x, int y, int c, int *xclamp, int *yclamp, int *cclamp)
{
    *xclamp = x;
    *yclamp = y;
    *cclamp = c;

    if(x < 0) *xclamp = 0;
    if(y < 0) *yclamp = 0;
    if(c < 0) *cclamp = 0;
    if(x >= im.w) *xclamp = im.w - 1;
    if(y >= im.h) *yclamp = im.h - 1; 
    if(c >= im.c) *cclamp = im.c - 1; 

    return (*xclamp == x) && (*yclamp == y) && (*cclamp == c);
}

int idx(image im, int x, int y, int c)
{
    return c*im.w*im.h + y*im.w + x;
}

float get_pixel(image im, int x, int y, int c)
{
    int xclamp, yclamp, cclamp;
    clamp_padding(im, x, y, c, &xclamp, &yclamp, &cclamp);
    return im.data[idx(im, xclamp, yclamp, cclamp)];
}

void set_pixel(image im, int x, int y, int c, float v)
{
    int xclamp, yclamp, cclamp;
    if(clamp_padding(im, x, y, c, &xclamp, &yclamp, &cclamp)){
        im.data[idx(im, x, y, c)] = v;
    }
}


// Map

typedef void (*fmap_yx) (image*, image*, int, int, void*);

typedef void (*fmap_cyx) (image*, image*, int, int, int, void*);

void map_yx(image *im1, image *im2, fmap_yx f, void *fmap_args)
{
    for(int y = 0; y < im1->h; y++){
        for(int x = 0; x < im1->w; x++){
            (*f)(im1, im2, x, y, fmap_args);
        }
    }
}

void map_cyx(image *im1, image *im2, fmap_cyx f, void *fmap_args)
{
    for(int c = 0; c < im1->c; c++){
        for(int y = 0; y < im1->h; y++){
            for(int x = 0; x < im1->w; x++){
                (*f)(im1, im2, x, y, c, fmap_args);
            }
        }
    }
}


// Copy

void fcopy_image(image *im1, image *im2, int x, int y, int c, void* args)
{
    float v = get_pixel(*im1, x, y, c);
    set_pixel(*im2, x, y, c, v);
}

image copy_image(image im)
{
    image copy = make_image(im.w, im.h, im.c);
    map_cyx(&im, &copy, &fcopy_image, NULL);
    return copy;
}


// RGB to Grayscale

static const float R_WEIGHT = 0.299;
static const float G_WEIGHT = 0.587;
static const float B_WEIGHT = 0.114;

void frgb_to_grayscale(image *im1, image *im2, int x, int y, void *args)
{
    float r = get_pixel(*im1, x, y, 0);
    float g = get_pixel(*im1, x, y, 1);
    float b = get_pixel(*im1, x, y, 2);
    float v = r * R_WEIGHT + g * G_WEIGHT + b * B_WEIGHT;
    set_pixel(*im2, x, y, 0, v);
}

image rgb_to_grayscale(image im)
{
    assert(im.c == 3);
    image gray = make_image(im.w, im.h, 1);
    map_yx(&im, &gray, &frgb_to_grayscale, NULL);
    return gray;
}


// Shift

typedef struct {
    int c;
    float v;
} cv_args; 

void fshift_image(image *im1, image *im2, int x, int y, void *args)
{
    int c = ((cv_args*)args)->c;
    float v = ((cv_args*)args)->v;
    float value = get_pixel(*im1, x, y, c);
    set_pixel(*im1, x, y, c, value + v);
}

void shift_image(image im, int c, float v)
{
    cv_args args = {c, v};
    map_yx(&im, NULL, &fshift_image, &args);
}


// Clamp

void fclamp_image(image *im1, image *im2, int x, int y, int c, void *args)
{
    float v = get_pixel(*im1, x, y, c);
    if(v > 1) v = 1;
    if(v < 0) v = 0;
    set_pixel(*im1, x, y, c, v);
}

void clamp_image(image im)
{
    map_cyx(&im, NULL, &fclamp_image, NULL);
} 


// These might be handy

float three_way_max(float a, float b, float c)
{
    return (a > b) ? ( (a > c) ? a : c) : ( (b > c) ? b : c) ;
}

float three_way_min(float a, float b, float c)
{
    return (a < b) ? ( (a < c) ? a : c) : ( (b < c) ? b : c) ;
}


// RGB to HSV

void frgb_to_hsv(image *im1, image *im2, int x, int y, void *args)
{
    float R = get_pixel(*im1, x, y, 0);
    float G = get_pixel(*im1, x, y, 1);
    float B = get_pixel(*im1, x, y, 2);
    
    float V = three_way_max(R, G, B);
    float C = V - three_way_min(R, G, B);
    float S = (V == 0 ? 0 : C / V);
    
    float h;
    if(C == 0){
        h = 0;
    } else if(V == R){
        h = (G - B) / C;
    } else if(V == G){
        h = (B - R) / C + 2;
    } else{ // V == b
        h = (R - G) / C + 4;
    }
    
    float H;
    if(h < 0){
        H = h / 6 + 1;
    } else{
        H = h / 6;
    }
    
    set_pixel(*im1, x, y, 0, H);
    set_pixel(*im1, x, y, 1, S);
    set_pixel(*im1, x, y, 2, V);
}


// HSV to RGB

void rgb_to_hsv(image im)
{
    assert(im.c == 3);
    map_yx(&im, NULL, &frgb_to_hsv, NULL);   
}

void fhsv_to_rgb(image *im1, image *im2, int x, int y, void *args)
{
    float H = 360 * get_pixel(*im1, x, y, 0);
    float S = get_pixel(*im1, x, y, 1);
    float V = get_pixel(*im1, x, y, 2);

    float C = V * S;
    float h = H / 60;
    float X = C*(1 - fabs(fmod(h, 2) - 1));

    float R, G, B; 
    if(0 <= h && h < 1){
        R = C;
        G = X;
        B = 0;
    } else if(1 <= h && h < 2){
        R = X;
        G = C;
        B = 0;
    } else if(2 <= h && h < 3){
        R = 0;
        G = C;
        B = X;
    } else if(3 <= h && h < 4){
        R = 0;
        G = X;
        B = C;
    } else if(4 <= h && h < 5){
        R = X;
        G = 0;
        B = C;
    } else{ // 5 <= H' <= 6
        R = C;
        G = 0;
        B = X;       
    }
 
    float m = V - C;   
    set_pixel(*im1, x, y, 0, R + m);
    set_pixel(*im1, x, y, 1, G + m);
    set_pixel(*im1, x, y, 2, B + m);
}

void hsv_to_rgb(image im)
{
    assert(im.c == 3);
    map_yx(&im, NULL, &fhsv_to_rgb, NULL);   
}


// EC: Scale

void fscale_image(image *im1, image *im2, int x, int y, void *args)
{
    int c = ((cv_args*)args)->c;
    float v = ((cv_args*)args)->v;
    float value = get_pixel(*im1, x, y, c);
    set_pixel(*im1, x, y, c, value * v);
}

void scale_image(image im, int c, float v)
{
    cv_args args = {c, v};
    map_yx(&im, NULL, &fscale_image, &args);
}
