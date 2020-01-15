/*
	//gcc src/edge-detect.c src/bitmap.c -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all -pthread
	//UTILISER UNIQUEMENT DES BMP 24bits
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bitmap.h"
#include <stdint.h>
#include <string.h>
//#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#define DIM 3
#define LENGHT DIM
#define OFFSET DIM /2

const float KERNEL[DIM][DIM] = {{-1, -1,-1},
							   {-1,8,-1},
							   {-1,-1,-1}};

typedef struct Color_t {
	float Red;
	float Green;
	float Blue;
} Color_e;

char directory_in[30] = "../in/";
char directory_out[30] = "../out/";

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
        return false;
    }

    for (int height = 0; height < a.bmp_header.height; ++height) {
        for (int width = 0; width < a.bmp_header.width; ++width) {
            Pixel pa = a.pixel_data[height][width];
            Pixel pb = b.pixel_data[height][width];

            if(pa.b != pb.b || pa.g != pb.g || pa.i != pb.i || pa.r != pb.r) {
                return false;
            }
        }
    }

    return true;
}

//producer
void * image_processor(void *arg) {
    Stack *images = (Stack *) arg;

    while(true) {
        pthread_mutex_lock(&images->lock);
        if(is_empty(images) || peek(images).treated) {
            pthread_cond_signal(&images->can_consume);
            while(is_empty(images) || peek(images).treated) {
                if(is_empty(images)) {
                    pthread_mutex_unlock(&images->lock);
                    return NULL;
                }
                pthread_cond_wait(&images->can_produce, &images->lock);
            }
        }

        Working_Image working_image = pop(images);
        pthread_mutex_unlock(&images->lock);

        Image new_image;
        printf("Start to treat `%s`\n", working_image.image.name);
        apply_effect(&working_image.image, &new_image);
        working_image.image = new_image;
        working_image.treated = true;
        printf("Successfully treated `%s`\n", working_image.image.name);

        pthread_mutex_lock(&images->lock);
        push(images, working_image);
        pthread_mutex_unlock(&images->lock);
    }
}

//consumer
void * image_saver(void *arg) {
    Stack *images = (Stack *)arg;
    int images_remaining = images->size;

    while(images_remaining > 0) {
        pthread_mutex_lock(&images->lock);
        while(is_empty(images) || !peek(images).treated) {

            pthread_cond_wait(&images->can_consume, &images->lock);
        }

        while(!is_empty(images) && peek(images).treated) {
            Working_Image workingImage = pop(images);
            save_bitmap(workingImage.image, directory_out);
            images_remaining--;
            printf("`%s` saved; %d remaining\n", workingImage.image.name, images_remaining);
        }

        pthread_cond_broadcast(&images->can_produce);
        pthread_mutex_unlock(&images->lock);
    }
}

int main(int argc, char** argv) {
    struct timeval begin, end;
    gettimeofday(&begin, NULL);

    Stack *images = open_bitmap_directory(directory_in);

    pthread_t threads_id[5];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for(int i = 0; i < 4; i++) {
        pthread_create(&threads_id[i], &attr, image_processor, (void *) images);
    }
    pthread_create(&threads_id[4], NULL, image_saver, (void *) images);
    //on attends le consommateur
    pthread_join(threads_id[4] ,NULL);

//    Image new_i;
//    int images_to_treat = images->count;
//    for(int i = 0; i < images_to_treat; i++) {
//        Image image = pop(images).image;
//        fprintf(stderr, "%s\n", image.name);
//        apply_effect(&image, &new_i);
//        save_bitmap(new_i, directory_out);
//    }

    gettimeofday(&end, NULL);
    double time_taken = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) * 1e-6;
    printf("Time to process directory : %.4lf secs\n", time_taken );
    stack_free(images);

	//Compare images to check we don't break algorithm
//    Image original = open_bitmap("../original/test_out.bmp");
//    printf("Equal ? %s\n", areSameImage(original, new_i) == true ? "true" : "false");
    return 0;
}