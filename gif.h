#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Opens a gif file with some error handling
 * @return file* to gif, NULL if fail
 */
FILE* open_gif(char* filename);


struct gif_info{
    char header[7]; // should be either "gif87a" or "gif89a"

    uint16_t canvas_width; // may not be super important
    uint16_t canvas_height;

    // contained in packed field byte
    bool global_color_table_flag;
    uint8_t color_resolution; // 3 bits, color_resolution+1 is bits per color, not very important in modern gifs can ignore
    bool sort_flag; // can ignore, here for completeness
    uint16_t global_color_table_size; // 3 bits originally, number of entries in color table. each color entry will have 3 bytes for r, g, and b. color table isze is 2^(N+1)

    uint8_t background_color_index; // only matters if global_color_flag is true
    uint8_t pixel_aspect_ratio; // can ignore, here for completeness

    int32_t* global_color_table; // array of colors, will be of size global_color_table_size
    // remember to write a cleanup function for this

    long image_data_pos; // to store the file position where image data starts so we can easily loop back later

};

struct image_info{
    
    // graphics control extension, only in gif89, is optional
    uint8_t extension_introducer; // should always be 0x21
    uint8_t graphic_control_label; // should always be 0xf9

    uint8_t block_size; // in bytes

    // second packed field byte, first 3 are reserved (don't care about em)
    uint8_t disposal_method; // 3 bits
    bool user_input_flag;
    bool transparent_color_flag;

    uint16_t delay_time; // 2 bytes, unsigned little endian
    uint8_t transparent_color_index;
    
    uint8_t block_terminator; // should always be 0x00

    // image descriptor

    uint8_t image_separator; // should always be 0x2C

    uint16_t image_x; // start position, left-most of image is 0
    uint16_t image_y; // start position, top of image is 0

    uint16_t image_width; // remember is stored little endian
    uint16_t image_height; // remember is stored little endian

    //packed field
    bool local_color_table_flag;
    bool interlace_flag; // hopefully can ignore
    bool sort_flag;
    //2 reserved bits
    uint16_t local_color_table_size; // 3 bits, size is 2^(N+1) entries, each entry has 3 bytes

    int32_t* local_color_table; // will set to NULL probably if not present
};

struct image{
    int32_t* pixel_array; // flattened array, row major
    uint16_t x_start;
    uint16_t y_start;
    uint16_t image_width;
    uint16_t image_height;
    uint8_t disposal_method;
    uint32_t bg_color;
    uint16_t delay_timer;
};

void image_cleanup(struct image* i);
/**
 * @brief retrieves header, logical screen descriptor, and global color table info from file
 */
void get_gif_info(FILE* file, struct gif_info* info);


struct image get_next_image(FILE* file, struct gif_info g_info);

/**
 * @brief gets image info
 * @return 0 if succesful, 1 if was a comment or application (ignore), -1 if error, 2 if reached trailer
 */
int get_image_info(FILE* file, struct image_info* i_info);

/**
 * @brief cleans up gif_info struct
 */
void gif_info_cleanup(struct gif_info* info);

/**
 * @brief cleans up image_info struct
 */
void image_info_cleanup(struct image_info* info);


