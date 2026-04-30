#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <drm.h>

void flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data){
    printf("Flip finished! Frame: %u at %u.%u seconds %d\n", frame, sec, usec, fd);
    data = data;
}


int main(){
    int fd;

    // store all info about resources from gpu
    drmModeRes* res;

    // store info about connectors (like hdmi or whatever i think) available
    drmModeConnector *conn;

    // mode is like the display resolutions and refresh rates possible, we want the first available one
    drmModeModeInfo mode;

    // store crtc we're using
    uint32_t crtc_id;
    // store connector we're using
    uint32_t conn_id;
    // store frame buffers we're using
    uint32_t fb_id[2];

    // for creating the dumb buffer and storing its info
    struct drm_mode_create_dumb create1 = {0};
    struct drm_mode_create_dumb create2 = {0};

    // for mapping the dumb buffer to memory
    struct drm_mode_map_dumb map = {0};
    uint32_t *map_ptr1;
    uint32_t *map_ptr2;


    fd = open("/dev/dri/card1", O_RDWR | __O_CLOEXEC);
    if(fd < 0){
        perror("Couldn't open /dev/dri/card1");
        return 1;
    }

    res = drmModeGetResources(fd);
    // crtc converts buffer to signal
    crtc_id = res->crtcs[0];

    // find the first connected connector
    for(int i = 0; i < res->count_connectors; i++){
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if(conn->connection == DRM_MODE_CONNECTED){
            conn_id = conn->connector_id;
            mode = conn->modes[0]; // take first available mode
            break;
        }
        drmModeFreeConnector(conn); // not using this one
    }

    // set dumb buffer resolution according to mode we found available
    create1.width = mode.hdisplay;
    create1.height = mode.vdisplay;
    create1.bpp = 32; // bits per pixel, just saying to 8 bits for red, 8 bits for green, 8 bits for blue, then 8 bits for alpha/padding
    
    // guessing some sort of wrapper for ioctl, send a request to create a dumb buffer using the settings we prepared
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create1);
    // when we create the dumb buffer, some fields get populated
    // handle -- kernel makes a dumb buffer in memory (could be vram or kernel memory depending on hardware) and gives us basically a file descriptor to interact with it
    // pitch -- number of bytes long a row of pixels is, isn't always just width * bytes_per_pixel bc alignment and padding
    // depth -- number of bits to represet color data in a pixel, usually 24 or 32 (24 if no alpha, 32 if alpha)

    // ----- difference between dumb buffer and frame buffer -----
    // dumb buffer is the actual data in either vram or system memory that stores the flat array of pixels. that's it, no fancy tricks
    // frame buffer points to a dumb buffer and attaches some metadata like the resolution, pixel format, depth, pitch, etc. lives in kernel memory
    
    // create 2nd dumb buffer
    create2.width = mode.hdisplay;
    create2.height = mode.vdisplay;
    create2.bpp = 32;
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create2);


    // create a framebuffer from dumb buffer
    drmModeAddFB(fd, mode.hdisplay, mode.vdisplay, 24, create1.bpp, create1.pitch, create1.handle, &(fb_id[0]));
    // fb_id is like another file descriptor to point to the frame buffer data
    drmModeAddFB(fd, mode.hdisplay, mode.vdisplay, 24, create2.bpp, create2.pitch, create2.handle, &(fb_id[1]));

    map.handle = create1.handle;
    // ask graphics card to map my dumb buffer to this map
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

    /* addr is 0 because we don't care where in memory my map goes to
       create1.size is saying to map as many bytes as the dumb buffer is big
       PROT_READ and PROT_WRITE are flags to say that i want to be able to both read and write to this memory (may not need read)
       MAP_SHARED is flag to say that anything i write to the map should reflect in the GPU buffer
       fd is the device's memory that you're talking to

       offset is complicated
       when we did the previous Ioctl, we told the gpu we intend to map the dumb buffer we made (it knows which buffer from the handle)
       to memory in my process memory. It gives you a fake offset value so that when you pass that offset value in to the mmap function
       it knows which dumb buffer to map

    */
    map_ptr1 = mmap(0, create1.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);

    // create 2nd buffer
    map.handle = create2.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    map_ptr2 = mmap(0, create2.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);

    // now we can draw by starting at map_ptr
    for(unsigned long long i = 0; i < (create1.size / 4); i++){ // need to divide by 4 because each pixel is 4 bytes wide
        map_ptr1[i] = 0xFF00FF; // just make that ts purple
    }

    for(unsigned long long i = 0; i < (create2.size / 4); i++){
        map_ptr2[i] = 0x00FF00; // make that ts green
    }

    // now that dumb buffer is populated, we can tell it to initialize crtc stuff and draw the frame
    drmModeSetCrtc(fd, crtc_id, fb_id[0], 0, 0, &conn_id, 1, &mode);

    printf("displaying frame 1 now goin to sleep for 2 seconds... \n");
    sleep(2);

    drmEventContext ev = {
        .version = 2,
        .page_flip_handler = flip_handler
    };
    

    if(drmModePageFlip(fd, crtc_id, fb_id[1], DRM_MODE_PAGE_FLIP_EVENT, NULL) != 0){
        perror("Failed to flip");
        return 1;
    }

    printf("Waiting for flip\n");
    drmHandleEvent(fd, &ev);

    printf("displaying frame 2 now goin to sleep for 2 seconds... \n");
    sleep(2);

    // cleanup
    munmap(map_ptr1, create1.size);
    munmap(map_ptr2, create2.size);
    drmModeRmFB(fd, fb_id[0]);
    drmModeRmFB(fd, fb_id[1]);
    drmModeFreeResources(res);
    close(fd);

    return 0;
}