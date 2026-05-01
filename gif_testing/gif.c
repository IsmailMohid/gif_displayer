#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "gif.h"

#define MAX_FILENAME_LENGTH 500



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

        info->global_color_table = malloc(sizeof(int32_t) * info->global_color_table_size); // allocate color table

        uint8_t* buf2 = malloc(3 * info->global_color_table_size); // allocate buffer to read data from file

        fread(buf2, 1, info->global_color_table_size * 3, file); // read color table to buffer

        // we could in theory write code that could fread directly into the color table and avoid this whole loop. bt this is way more readable and doesn't make me wanna kms tho so f that
        for(uint16_t i = 0; i < info->global_color_table_size; ++i){
            info->global_color_table[i] = (((uint32_t) buf2[3*i + 0] << 16) + ((uint32_t) buf2[3*i+1] << 8) + (buf2[3*i+2])) & 0x00FFFFFF ; // extract colors and zero out the most significant ones
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


