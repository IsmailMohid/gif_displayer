#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "gif.h"

#define MAX_FILENAME_LENGTH 500


int main(int argc, char *argv[]){
    if(argc != 2){
        fprintf(stderr, "ERROR: Invalid arguments\n");
        return 1;
    }

    FILE* file = open_gif(argv[1]);
    if(file == NULL){
        fprintf(stderr, "Gif open failed!\n");
        return -1;
    }

    struct gif_info gif_stats = {0};

    get_gif_info(file, &gif_stats);

    printf("Header: %s, width: %u, height: %u, color_flag: %u, color_res: %u, sort_flag: %u, color_table_size: %u, bg_color_index: %u, pix_rat: %u\n", gif_stats.header, gif_stats.canvas_width, gif_stats.canvas_height, gif_stats.global_color_table_flag, gif_stats.color_resolution, gif_stats.sort_flag, gif_stats.global_color_table_size, gif_stats.background_color_index, gif_stats.pixel_aspect_ratio);

    for(int i = 0; i < gif_stats.global_color_table_size; ++i){
        printf("Color %d: 0x%06X\n", i, color_to_hex(gif_stats.global_color_table[i]));
    }

    


     

    // cleanup
    gif_info_cleanup(&gif_stats);
    fclose(file);

    return 0;
}


static inline uint32_t color_to_hex(struct color col){
    return ((uint32_t)col.red << 16) + ((uint32_t)col.green << 8) + (col.blue);
}

void get_gif_info(FILE* file, struct gif_info* info){
    uint8_t buf[13];

    fread(buf, 1, 13, file);

    // get header
    for(int i = 0; i < 6; ++i){
        info->header[i] = (char) buf[i];
    }
    info->header[6] = '\0';

    // get canvas width and height
    info->canvas_width = buf[6] + ((uint16_t)buf[7] << 8);
    info->canvas_height = buf[8] + ((uint16_t)buf[9] << 8); 

    // decompose packed field byte
    uint8_t packed_field = buf[10];

    info->global_color_table_flag = (bool) ((packed_field >> 7) & 0b00000001);
    
    uint8_t raw_color_res = (packed_field >> 4) & 0b00000111; // just get the bits
    info->color_resolution = raw_color_res + 1; // num bits per color

    info->sort_flag = (bool) ((packed_field >> 3) & 0b00000001); // don't actually need to shift it ig, could just do && 0b00001000

    uint8_t raw_color_table_size = packed_field & 0b00000111;
    info->global_color_table_size = 1L << (raw_color_table_size + 1L); // 2 ^(N+1)

    info->background_color_index = buf[11];

    info->pixel_aspect_ratio = buf[12];

    // now we need the global color table
    if(info->global_color_table_flag == true){

        info->global_color_table = malloc(sizeof(struct color) * info->global_color_table_size); // allocate color table

        uint8_t* buf2 = malloc(3 * info->global_color_table_size); // allocate buffer to read data from file

        fread(buf2, 1, info->global_color_table_size * 3, file); // read color table to buffer

        // we could in theory write code that could fread directly into the color table and avoid this whole loop. bt this is way more readable and doesn't make me wanna kms tho so f that
        for(uint16_t i = 0; i < info->global_color_table_size; ++i){
            info->global_color_table[i].red = buf2[3*i + 0];
            info->global_color_table[i].green = buf2[3*i + 1];
            info->global_color_table[i].blue = buf2[3*i + 2];
        }


        free(buf2); // don't forget ! memory leaks are bad !
    }

    else{
        info->global_color_table = NULL;
    }

    info->image_data_pos = ftell(file); // store where the header and friends stuff ends

}

void gif_info_cleanup(struct gif_info* info){
    free(info->global_color_table); // death to memory leaks
    info->global_color_table = NULL;
}

FILE* open_gif(char* filename){
    size_t filename_len = strnlen(filename, MAX_FILENAME_LENGTH + 1);

    if(filename_len == MAX_FILENAME_LENGTH + 1){
        fprintf(stderr, "Filename too long!\n");
        return NULL;
    }

    if(filename[filename_len - 4] != '.' || filename[filename_len - 3] != 'g' || filename[filename_len - 2] != 'i' || filename[filename_len - 1] != 'f'){
        fprintf(stderr, "Not a .gif file!\n");
        return NULL;
    }


    FILE* file = fopen(filename, "r");
    
    if(file == NULL){
        perror("failed to open file");
        return NULL;
    }

    return file;
}


