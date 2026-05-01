#include "gif.h"
#include <stdio.h>


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
        printf("Color %d: 0x%06X\n", i, gif_stats.global_color_table[i]);
    }

    


     

    // cleanup
    gif_info_cleanup(&gif_stats);
    fclose(file);

    return 0;
}