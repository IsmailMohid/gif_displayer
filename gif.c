#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "gif.h"

#define MAX_DICT_SIZE 4096
#define MAX_FILENAME_LENGTH 500

void decompress_gif_to_array(FILE *fp, int min_code_size, uint8_t *out_buffer, int out_size);



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

int get_image_info(FILE* file, struct image_info* i_info){

    uint8_t first_byte;
    fread(&first_byte, 1, 1, file); // read first byte
    

    if(first_byte == 0x21){ // image has extension
        // now we check the 2nd byte to see what kind of exension it is
        uint8_t second_byte;
        fread(&second_byte, 1, 1, file); // read 2nd byte
        
        if(second_byte == 0xF9){ // graphic control extension
            i_info->graphic_control_label = second_byte;
            
            // read rest of extension block
            uint8_t buf[7];
            fread(buf, 1, 7, file);

            // process data

            i_info->block_size = buf[0];

            // process packed field byte
            uint8_t packed_field = buf[1];

            i_info->disposal_method = (packed_field >> 2) & 0b00000111;
            i_info->user_input_flag = (bool) ((packed_field >> 1) & 0b00000001);
            i_info->transparent_color_flag = (bool) (packed_field & 0b00000001);

            
            // process rest of data
            i_info->delay_time = buf[2] + ( (uint16_t)buf[3] << 8);

            i_info->transparent_color_index = buf[4];
            i_info->block_terminator = buf[5]; // don't really need this but ¯\_(ツ)_/¯

            if(buf[6] == 0x21){ // no image, just a plain text extension so don't care
                return 1;
            }
            //fseek(file, -1, SEEK_CUR); // set it back one just to keep it consistent

        }

        else if(second_byte == 0x01 || second_byte == 0xFF || second_byte == 0xFE ){ // plain text, application, or comment extension (don't care about any of these, ignore)
            i_info->graphic_control_label = second_byte; // to indicate block is whtever extension and we ignored it

            if(second_byte == 0x01 || second_byte == 0xFE){ // plain text or comment
                uint8_t block_size;
                fread(&block_size, 1, 1, file);
                while(block_size != 0){
                    fseek(file, block_size, SEEK_CUR); // skip forward block_size bytes
                    fread(&block_size, 1, 1, file); // read next block size
                }
                // now at the end of the extension block and can continue to next

            }
            else if(second_byte == 0xFF){ // application
                fseek(file, 17, SEEK_CUR); // just skip the block
            }

            return 1;
        }
        else{
            fprintf(stderr, "Error reading gif extension\n");
            return -1;
        }


    }
    else if(first_byte == 0x2C){ // no extension, just proceed
        i_info->extension_introducer = 0x00; // to indicate block is empty
        //fseek(file, -1, SEEK_CUR); // set it back one just to keep it consistent
    }

    else if(first_byte == 0x3B){ // reached end of gif file
        return 2;
    }

    else{
        fprintf(stderr, "Error reading gif blocks\n");
        return -1;
    }

    // now can read image descriptor
    uint8_t buf[9];
    fread(buf, 1, 9, file);

    i_info->image_separator = 0x2C;    

    i_info->image_x = buf[0] + ( (uint16_t) buf[1] << 8);
    i_info->image_y = buf[2] + ( (uint16_t) buf[3] << 8);
    i_info->image_width = buf[4] + ( (uint16_t) buf[5] << 8);
    i_info->image_height = buf[6] + ( (uint16_t) buf[7] << 8);

    // process packed field
    uint8_t packed_field = buf[8];
    
    i_info->local_color_table_flag = (packed_field >> 7) & 0b00000001;
    i_info->interlace_flag = (packed_field >> 6) & 0b00000001;
    i_info->sort_flag = (packed_field >> 5) & 0b00000001;
    
    uint16_t raw_color_table_size = packed_field & 0b00000111;
    i_info->local_color_table_size = 1L << (raw_color_table_size + 1L); // 2 ^(N+1)

    if(i_info->local_color_table_flag == true){

        i_info->local_color_table = malloc(sizeof(int32_t) * i_info->local_color_table_size); // allocate color table

        uint8_t* buf2 = malloc(3 * i_info->local_color_table_size); // allocate buffer to read data from file

        fread(buf2, 1, i_info->local_color_table_size * 3, file); // read color table to buffer

        // we could in theory write code that could fread directly into the color table and avoid this whole loop. bt this is way more readable and doesn't make me wanna kms tho so f that
        for(uint16_t i = 0; i < i_info->local_color_table_size; ++i){
            i_info->local_color_table[i] = (((uint32_t) buf2[3*i + 0] << 16) + ((uint32_t) buf2[3*i+1] << 8) + (buf2[3*i+2])) & 0x00FFFFFF ; // extract colors and zero out the most significant ones
        }


        free(buf2); // don't forget ! memory leaks are bad !
    }

    else{
        i_info->local_color_table = NULL;
    }

    return 0; // i_info has been populated successfully
}

struct image get_next_image(FILE* file, struct gif_info g_info){

    struct image_info i_info;
    int result =  get_image_info(file, &i_info);

    switch(result){
        case 0: // read successfully
            // just continue on
            break;
        
        case 1: // was an extension block, need to proceed to next image
            return get_next_image(file, g_info); // retry
            break;
        
        case -1: // error happened while reading image info
            fprintf(stderr, "Reading gif next image failed \n");
            struct image i = {0};
            exit(1);
            return i;
            break;
        
        case 2: // reached end of file, need to loop back
            fseek(file, g_info.image_data_pos, SEEK_SET);
            return get_next_image(file, g_info); // retry

            break;

        default: // should not be possible
            fprintf(stderr, "Reading gif didn't work \n");
            //struct image i = {0};
            exit(1);
            //return i;
            break;
    }

    struct image i;
    i.pixel_array = NULL;
    i.x_start = i_info.image_x;
    i.y_start = i_info.image_y;
    i.image_width = i_info.image_width;
    i.image_height = i_info.image_height;
    i.disposal_method = i_info.disposal_method;
    i.bg_color = g_info.global_color_table[g_info.background_color_index];
    i.delay_timer = i_info.delay_time; // perhaps could be bad if no graphics control block was present

    

    // fun time LZW decompression
    uint8_t min_code_size;
    fread(&min_code_size, 1, 1, file);

    /*
    uint32_t code_table_size = (1L << (min_code_size)) + 2;

    uint32_t clear_code = code_table_size - 2;
    uint32_t end_of_info_code = code_table_size - 1;

    uint32_t* code_table = malloc(sizeof(uint32_t) * code_table_size); 

    for(uint32_t i = 0; i < code_table_size; ++i){
        code_table[i] = i;
    }
    // now its just data blocks


    free(code_table);*/
    uint32_t buf_size = (uint32_t) i_info.image_width * i_info.image_height;
    uint8_t* buf = malloc(sizeof(uint8_t) * buf_size);

    decompress_gif_to_array(file, min_code_size, buf, buf_size);
    
    int32_t* color_table;

    if(i_info.local_color_table_flag == true){
        color_table = i_info.local_color_table;
    }
    else{
        color_table = g_info.global_color_table;
    }

    free(i.pixel_array); // make sure this is NULL on first initializatoin
    i.pixel_array = malloc(sizeof(uint32_t) * buf_size);
    
    for(uint32_t j = 0; j < buf_size; ++j){
        i.pixel_array[j] = color_table[buf[j]];
    }

    free(buf);

    return i;
}

void gif_info_cleanup(struct gif_info* info){
    free(info->global_color_table); // death to memory leaks
    info->global_color_table = NULL;
}

void image_info_cleanup(struct image_info* info){
    free(info->local_color_table); // the crusade never ends
    info->local_color_table = NULL;
}

void image_cleanup(struct image* i){
    free(i->pixel_array);
    i->pixel_array = NULL;
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




typedef struct {
    int prefix[MAX_DICT_SIZE];
    uint8_t suffix[MAX_DICT_SIZE];
    uint8_t stack[MAX_DICT_SIZE]; 
} LZWState;

/**
 * @param fp: Pointer to the open GIF file
 * @param min_code_size: The byte read right before the data blocks
 * @param out_buffer: Your pre-allocated uint8_t array
 * @param out_size: Total number of pixels expected (Width * Height)
 */
void decompress_gif_to_array(FILE *fp, int min_code_size, uint8_t *out_buffer, int out_size) {
    int clear_code = 1 << min_code_size;
    int eoi_code = clear_code + 1;
    int code_size = min_code_size + 1;
    int next_code = eoi_code + 1;

    unsigned long bit_buffer = 0;
    int bits_in_buffer = 0;
    uint8_t block_size = 0;
    int pixel_idx = 0;

    LZWState *state = malloc(sizeof(LZWState));
    
    // Initialize root dictionary
    for (int i = 0; i < clear_code; i++) {
        state->suffix[i] = i;
        state->prefix[i] = -1;
    }

    int old_code = -1;

    while (pixel_idx < out_size) {
        // --- 1. Bit-Slicing from Sub-blocks ---
        while (bits_in_buffer < code_size) {
            if (block_size == 0) {
                if (fread(&block_size, 1, 1, fp) != 1 || block_size == 0) goto cleanup;
            }
            uint8_t byte;
            fread(&byte, 1, 1, fp);
            bit_buffer |= ((unsigned long)byte << bits_in_buffer);
            bits_in_buffer += 8;
            block_size--;
        }

        int code = bit_buffer & ((1 << code_size) - 1);
        bit_buffer >>= code_size;
        bits_in_buffer -= code_size;

        // --- 2. Logic Control ---
        if (code == eoi_code) break;
        if (code == clear_code) {
            code_size = min_code_size + 1;
            next_code = eoi_code + 1;
            old_code = -1;
            continue;
        }

        // --- 3. Decompressing the Code ---
        int current = code;
        int stack_ptr = 0;

        // Special Case: Code not yet in dictionary
        if (current >= next_code) {
            if (old_code == -1) break; // Should never happen in valid GIF
            
            // Find first character of the old_code sequence
            int temp = old_code;
            while (state->prefix[temp] != -1) temp = state->prefix[temp];
            
            state->stack[stack_ptr++] = state->suffix[temp];
            current = old_code;
        }

        // Trace back the chain
        while (current != -1 && stack_ptr < MAX_DICT_SIZE) {
            state->stack[stack_ptr++] = state->suffix[current];
            current = state->prefix[current];
        }

        // Output to your uint8_t array
        uint8_t first_pixel = state->stack[stack_ptr - 1];
        while (stack_ptr > 0 && pixel_idx < out_size) {
            out_buffer[pixel_idx++] = state->stack[--stack_ptr];
        }

        // --- 4. Update Dictionary ---
        if (old_code != -1 && next_code < MAX_DICT_SIZE) {
            state->prefix[next_code] = old_code;
            state->suffix[next_code] = first_pixel;
            next_code++;

            if (next_code == (1 << code_size) && code_size < 12) {
                code_size++;
            }
        }
        old_code = code;
    }

    while (block_size > 0) {
        uint8_t dummy;
        fread(&dummy, 1, 1, fp);
        block_size--;
    }

    // 2. Consume any SUBSEQUENT sub-blocks until we hit the 0x00 terminator
    uint8_t next_block_size;
    while (fread(&next_block_size, 1, 1, fp) == 1 && next_block_size > 0) {
        fseek(fp, next_block_size, SEEK_CUR);
    }

cleanup:
    free(state);
}