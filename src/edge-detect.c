/*
	//gcc src/edge-detect.c src/bitmap.c -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all
	//UTILISER UNIQUEMENT DES BMP 24bits
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bitmap.h"
#include <stdint.h>
#include <string.h>

#define DIM 3
#define LENGHT DIM
#define OFFSET DIM /2
#define TRUE 1
#define FALSE 0

const float KERNEL[DIM][DIM] = {{-1, -1,-1},
							   {-1,8,-1},
							   {-1,-1,-1}};

typedef struct Color_t {
	float Red;
	float Green;
	float Blue;
} Color_e;


void apply_effect(Image* original, Image* new_i);
void apply_effect(Image* original, Image* new_i) {

	int w = original->bmp_header.width;
	int h = original->bmp_header.height;

	*new_i = new_image(w, h, original->bmp_header.bit_per_pixel, original->bmp_header.color_planes);

    /* Set image name */
    new_i->name = malloc(strlen(original->name));
    strcpy(new_i->name, original->name);

    for (int y = OFFSET; y < h - OFFSET; y++) {
		for (int x = OFFSET; x < w - OFFSET; x++) {
			Color_e c = { .Red = 0, .Green = 0, .Blue = 0};

			for(int a = 0; a < LENGHT; a++){
				for(int b = 0; b < LENGHT; b++){
					int xn = x + a - OFFSET;
					int yn = y + b - OFFSET;

					Pixel* p = &original->pixel_data[yn][xn];

					c.Red += ((float) p->r) * KERNEL[a][b];
					c.Green += ((float) p->g) * KERNEL[a][b];
					c.Blue += ((float) p->b) * KERNEL[a][b];
				}
			}

			Pixel* dest = &new_i->pixel_data[y][x];
			dest->r = (uint8_t)  (c.Red <= 0 ? 0 : c.Red >= 255 ? 255 : c.Red);
			dest->g = (uint8_t) (c.Green <= 0 ? 0 : c.Green >= 255 ? 255 : c.Green);
			dest->b = (uint8_t) (c.Blue <= 0 ? 0 : c.Blue >= 255 ? 255 : c.Blue);
		}
	}
}

char areSameImage(Image a, Image b) {
    if(a.bmp_header.width != b.bmp_header.width || a.bmp_header.height != b.bmp_header.height) {
        return FALSE;
    }

    for (int height = 0; height < a.bmp_header.height; ++height) {
        for (int width = 0; width < a.bmp_header.width; ++width) {
            Pixel pa = a.pixel_data[height][width];
            Pixel pb = b.pixel_data[height][width];

            if(pa.b != pb.b || pa.g != pb.g || pa.i != pb.i || pa.r != pb.r) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

int main(int argc, char** argv) {

	Image img = open_bitmap("../img/bmp_tank.bmp");
	Image new_i;
	apply_effect(&img, &new_i);

	//Compare images to check we don't break algorithm
    Image original = open_bitmap("../original/test_out.bmp");
    printf("Equal ? %s\n", areSameImage(original, new_i) == TRUE ? "True" : "False");

    save_bitmap(new_i, "../out/");
    return 0;
}