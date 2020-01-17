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
#include <dirent.h>

#define DIM 3
#define LENGHT DIM
#define OFFSET DIM /2
#define NUMBER_OF_MASK 3
#define EDGE_DETECT {{-1, -1, -1}, {-1, 8,  -1}, {-1, -1, -1}}
#define SHARPEN {{0, -1, 0}, {-1, 5,  -1}, {0, -1, 0}}
#define BOX_BLUR {{0.1111, 0.1111, 0.1111}, {0.1111, 0.1111,  0.1111}, {0.1111, 0.1111, 0.1111}}


typedef struct Color_t {
	float Red;
	float Green;
	float Blue;
} Color_e;

typedef struct  mask_t {
    char *name;
    float matrix[DIM][DIM];
}                mask_s;

char *directory_in, *directory_out;
mask_s selected_mask;

struct mask_t get_mask(const char *const name)
{
    const mask_s mask_array[NUMBER_OF_MASK] = {
            {
                    .name = "edgedetect",
                    .matrix = EDGE_DETECT,
            },
            {
                    .name = "sharpen",
                    .matrix = SHARPEN,
            },
            {
                    .name = "boxblur",
                    .matrix = BOX_BLUR,
            }
    };
    for (int i = 0; i < NUMBER_OF_MASK; ++i) {
        if (!strcmp(mask_array[i].name, name)) {
            printf("%s\n", mask_array[i].name);
            return mask_array[i];
        }
    }

    fprintf(stderr, "Invalid mask, edgedetect selected\n");
    return mask_array[0];
}

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

					c.Red += ((float) p->r) * selected_mask.matrix[a][b];
					c.Green += ((float) p->g) * selected_mask.matrix[a][b];
					c.Blue += ((float) p->b) * selected_mask.matrix[a][b];
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

void set_directory(char *origin, char **target) {
    *target = malloc(strlen(origin));
    strcpy(*target, origin);
}

void empty_directory(char *directory) {
    DIR *folder = opendir(directory);
    struct dirent *next_file;
    char filepath[512];

    while ((next_file = readdir(folder)) != NULL )
    {
        if ((strlen(directory) + strlen(next_file->d_name) + 1) < sizeof(filepath) / sizeof(char)) {
            sprintf(filepath, "%s/%s", directory, next_file->d_name);
            remove(filepath);
        }
    }
    closedir(folder);
}

void free_directory(char *directory) {
    free(directory);
}

int main(int argc, char** argv) {
    if(argc < 5) {
        fprintf(stderr, "Wrong call. Waiting arguments [in_directory] [out_directory] [thread_number] [algorithm_name]\n");
        fprintf(stderr, "Example : ./apply-effect \"./in/\" \"./out/\" 3 boxblur");
        return 1;
    }

    set_directory(argv[1], &directory_in);
    if(countBmpFilesInDir(directory_in) == 0) {
        fprintf(stderr, "Directory_in is empty.");
        exit(1);
    }
    set_directory(argv[2], &directory_out);

    empty_directory(directory_out);

    struct timeval begin, end;
    gettimeofday(&begin, NULL);

    Stack *images = open_bitmap_directory(directory_in);

    pthread_t threads_id[5];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int thread_number = atoi(argv[3]);

    if (thread_number <= 0 || thread_number > images->size) {
        fprintf(stderr, "Usage: Thread number must be greater than 0 & less than or equal to image number");
        return 1;
    }

    selected_mask = get_mask(argv[4]);

    for(int i = 0; i < thread_number; i++) {
        pthread_create(&threads_id[i], &attr, image_processor, (void *) images);
    }
    pthread_create(&threads_id[thread_number], NULL, image_saver, (void *) images);
    //on attends le consommateur
    pthread_join(threads_id[thread_number] ,NULL);

    gettimeofday(&end, NULL);
    double time_taken = (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec) * 1e-6;
    printf("Time to process directory : %.4lf secs\n", time_taken );
    stack_free(images);

    free_directory(directory_in);
    free_directory(directory_out);

	//Compare images to check we don't break algorithm
//    Image original = open_bitmap("../original/test_out.bmp");
//    printf("Equal ? %s\n", areSameImage(original, new_i) == true ? "true" : "false");
    return 0;
}