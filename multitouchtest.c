// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Multipoint touchscreen tester with DRM/KMS backend for display
 *
 * Copyright (c) 2024 Subhajit Ghosh <subhajit.ghosh@tweaklogic.com>
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <linux/version.h>
#include <linux/input.h>

#define PI                      3.14159265
#define DIA                     200
#define MAX_TOUCH               4
#define MAX_INPUT_DEV           20
#define COLOR_RED               0xF8FF0000
#define COLOR_GREEN             0xF800FF00
#define COLOR_BLUE              0xF80000FF
#define COLOR_WHITE             0xF8FFFFFF
#define PATH_TOUCH_DEV          "/dev/input/event"
#define DISPLAY_UPDATE_DELAY    100000

int stop_exit;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

uint32_t color[] = { COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE };

typedef struct _event_resources {
    int fd;
    int dev;
} event_resources_t;

typedef struct _slot_info {
    int tracking_id;
    int btn_touch;
    int pos_x;
    int pos_y;
    int removed;
} slot_info_t;

typedef struct _shape {
    void *ptr;
    int x1;
    int y1;
    uint32_t color;
} shape_t;

typedef struct _drm_resourrces {
    int fd;
    void *mem[2];
    void *fb_mem[2];
    char *module;

    drmModeResPtr resources;
    uint32_t crtc_id;
    uint32_t connector_id;

    drmModeConnectorPtr connector;
    drmModeModeInfo mode;
    uint32_t width;
    uint32_t height;
    uint32_t pitch[2];

    struct drm_mode_create_dumb bo_create[2];
    struct drm_mode_map_dumb bo_map[2];
    int bo_index;

    uint32_t handles[4];
    uint32_t pitches[4];
    uint32_t offsets[4];
    uint32_t fb_id[2];
} drm_resources_t;

struct _drm_index {
    uint32_t crtc_id;
    uint32_t connector_id;
} drm_index[10];

drm_resources_t *drm_resources;
event_resources_t *event_resources;
shape_t circle[MAX_TOUCH];
slot_info_t slot[MAX_TOUCH];

char *drm_module_list[] = {
    "stm",
    "vc4",
    "i915",
    "amdgpu",
    "radeon",
    "nouveau",
    "vmwgfx",
    "omapdrm",
    "exynos",
    "tilcdc",
    "msm",
    "sti",
    "tegra",
    "imx-drm",
    "rockchip",
    "atmel-hlcdc",
    "fsl-dcu-drm",
    "virtio_gpu",
    "mediatek",
    "meson",
    "pl111",
};

void catch(int signum)
{
    printf("Caught signal, exiting...\n");
    stop_exit = 1;
    pthread_cond_signal(&cond1);
}

int create_circle(int dia)
{
    int i;
    int y;
    int x1;
    int x2;
    int c;
    int d;
    double val = PI/180;
    double m = 0;
    int center = dia/2;
    unsigned int (*buff)[dia];

    for (i=0;i<MAX_TOUCH;i++) {
        if (circle[i].ptr == NULL) {
            circle[i].color = color[i];
            circle[i].x1 = dia;
            circle[i].y1 = dia;
            circle[i].ptr = malloc(dia * dia * sizeof(uint32_t));
            if (circle[i].ptr == NULL)
                return -1;
            memset(circle[i].ptr, 0, (dia * dia * sizeof(uint32_t)));
            buff = circle[i].ptr;

            for (c=0;c<720;c++) {
                y = center + (center * sin(m*val));
                x1 = center + (center * cos(m*val));
                x2 = center + (center * cos((m+180)*val));
                m += 0.5;
                for(d=x2;d<x1;d++) {
                    if (d >= 0 && y >= 0 && d < circle[i].x1 && y < circle[i].y1)
                        buff[y][d] = circle[i].color | 0xF8000000;
                }
            }
        }
    }
    return 0;
}

void destroy_circle()
{
    int i;

    for (i=0;i<MAX_TOUCH;i++) {
        if (circle[i].ptr != NULL) {
            free(circle[i].ptr);
            circle[i].ptr = NULL;
        }
    }
}

int draw_shape(void *fb, uint32_t width, uint32_t height,
    uint32_t pitch, const shape_t *shape, uint32_t touch_x,
    uint32_t touch_y)
{
    unsigned int pitch_4b = pitch/4;
    unsigned int (*buff)[pitch_4b] = fb;
    unsigned int (*shape_buff)[shape->x1] = shape->ptr;
    int shape_center_x = (shape->x1)/2;
    int shape_center_y = (shape->y1)/2;
    int shape_x0 = 0;
    int shape_y0 = 0;
    int fb_x0;
    int fb_x1;
    int fb_y0;
    int fb_y1;
    int i;
    int j;
    int k;

    fb_x0 = touch_x - shape_center_x;
    if (fb_x0 < 0) {
        fb_x0 = 0;
        shape_x0 = shape_center_x - touch_x;
    }

    fb_x1 = touch_x + shape_center_x;
    if (fb_x1 > width)
        fb_x1 = width;
   
    fb_y0 = touch_y - shape_center_y;
    if (fb_y0 < 0) {
        fb_y0 = 0;
        shape_y0 = shape_center_y - touch_y;
    }

    fb_y1 = touch_y + shape_center_y;
    if (fb_y1 > height)
        fb_y1 = height;

    for (i=fb_y0; i<fb_y1; i++,shape_y0++) {
        for (j=fb_x0,k=0; j<fb_x1; j++,k++) {
            if (i<height && j<width)
                buff[i][j] = buff[i][j] | shape_buff[shape_y0][shape_x0+k];
        }
    }

    return 0;
}

int drm_init(int idx)
{
    int crtc_id;
    int connector_id;
    int ret;
    int i;

    drm_resources = malloc(sizeof(drm_resources_t));

    if (drm_resources == NULL)
        return -1;

    for (i=0;i<sizeof(drm_module_list);i++) {
        drm_resources->fd = drmOpen(drm_module_list[i], NULL);
        if (drm_resources->fd >= 0)
            break;
    }

    if (drm_resources->fd < 0) {
        perror("Failed to open drm device");
        return -1;
    }
    drm_resources->module = drm_module_list[i];

    connector_id = drm_index[idx].connector_id;
    crtc_id = drm_index[idx].crtc_id;

    printf("Using Connector ID:%d, CRTC ID:%d\n", connector_id, crtc_id);

    /* Get resources */
    drm_resources->resources = drmModeGetResources(drm_resources->fd);
    drm_resources->crtc_id = crtc_id;
    drm_resources->connector_id = connector_id;

    /* Get the first connector paramaters */
    drm_resources->connector = drmModeGetConnector(drm_resources->fd,
        drm_resources->connector_id);
    drm_resources->mode = drm_resources->connector->modes[0];
    drm_resources->width = drm_resources->mode.hdisplay;
    drm_resources->height = drm_resources->mode.vdisplay;

    for (i=0;i<2;i++) {
        /* Create a buffer object */
        memset(&drm_resources->bo_create[i], 0, sizeof(drm_resources->bo_create[i]));
        drm_resources->bo_create[i].bpp = 32;
        drm_resources->bo_create[i].width = drm_resources->width;
        drm_resources->bo_create[i].height = drm_resources->height;
        ret = drmIoctl(drm_resources->fd, DRM_IOCTL_MODE_CREATE_DUMB,
            &drm_resources->bo_create[i]);
        if (ret) {
            perror("Failed to create dumb buffer.");
            return -1;
        }

        drm_resources->pitch[i] = drm_resources->bo_create[i].pitch;

        /* Create a mapping of the buffer */
        memset(&drm_resources->bo_map[i], 0, sizeof(drm_resources->bo_map[i]));
        drm_resources->bo_map[i].handle = drm_resources->bo_create[i].handle;
        ret = drmIoctl(drm_resources->fd, DRM_IOCTL_MODE_MAP_DUMB, &drm_resources->bo_map[i]);
        if (ret) {
            perror("Failed to create dumb buffer.");
            return -1;
        }

        drm_resources->mem[i] =
                mmap(0, drm_resources->bo_create[i].size, PROT_READ |
                        PROT_WRITE, MAP_SHARED,
                        drm_resources->fd, drm_resources->bo_map[i].offset);
        if (drm_resources->mem[0] == MAP_FAILED) {
            perror("Failed to mmap memmory.");
            drmClose(drm_resources->fd);
        }

        drm_resources->fb_mem[i] = drm_resources->mem[i];

        drm_resources->handles[0] = drm_resources->bo_create[i].handle;
        drm_resources->pitches[0] = drm_resources->bo_create[i].pitch;
        drm_resources->offsets[0] = 0;

        ret = drmModeAddFB2(drm_resources->fd, drm_resources->width,
                drm_resources->height, DRM_FORMAT_XRGB8888,
                drm_resources->handles, drm_resources->pitches,
                drm_resources->offsets, &drm_resources->fb_id[i], 0);
        if (ret) {
            perror("Failed to add framebuffer.");
            return -1;
        }
    }

    drm_resources->bo_index = 0;
    ret = drmModeSetCrtc(drm_resources->fd, drm_resources->crtc_id,
            drm_resources->fb_id[drm_resources->bo_index], 0, 0,
            &drm_resources->connector_id, 1, &drm_resources->mode);
    if (ret)
        perror("Failed to set mode.");

    return 0;
}

int drm_destroy()
{
    int i;

    for (i=0;i<2;i++) {
        munmap(drm_resources->mem[i], drm_resources->bo_create[i].size);
        drmModeRmFB(drm_resources->fd, drm_resources->fb_id[i]);
    }

    if (drm_resources->fd >= 0) {
        drmClose(drm_resources->fd);
        drm_resources->fd = -1;
    }

    if (drm_resources) {
        free(drm_resources);
        drm_resources = NULL;
    }

    return 0;
}

int event_init(int event_index)
{
    int ret;
    int i;
    int fd;
    char event_dev_path[64];

    event_resources = malloc(sizeof(event_resources_t));
    if (event_resources == NULL)
        return -1;

    sprintf(event_dev_path, "%s%d", PATH_TOUCH_DEV, event_index);
    fd = open(event_dev_path, O_RDONLY);
    event_resources->fd = fd;
    event_resources->dev = event_index;

    if(fd == -1)
        return -1;

    return 0;
}

int event_destroy()
{
    if (event_resources->fd >= 0) {
        close(event_resources->fd);
        event_resources->fd = -1;
    }

    if (event_resources) {
        free(event_resources);
        event_resources = NULL;
    }
    return 0;
}

void *update_display(void *ptr)
{
    int i;
    int ret;
    int fb_cleared = 0;
    int draw_buff_index = 0;
    int pos_x;
    int pos_y;

    for (;;) {
        pthread_cond_wait(&cond1, &lock);
        if (stop_exit)
            break;

        draw_buff_index = (drm_resources->bo_index)^1;

        for (i=0;i<MAX_TOUCH;i++) {
            if (!fb_cleared) {
                memset(
                    drm_resources->fb_mem[draw_buff_index],
                    0,
                    drm_resources->pitch[draw_buff_index] \
                    * drm_resources->height);
                fb_cleared = 1;
            }

            pos_x = slot[i].pos_x;
            pos_y = slot[i].pos_y;

            if (pos_y>=0&&pos_y<drm_resources->width &&
                    pos_x>=0&&pos_x<drm_resources->height) {
                if (slot[i].tracking_id != -1) {
                    draw_shape(
                            drm_resources->fb_mem[draw_buff_index],
                            drm_resources->width,
                            drm_resources->height,
                            drm_resources->pitch[draw_buff_index],
                            &(circle[i]),
                            pos_y, pos_x);
                            /* 720 - pos_y, pos_x); */
                }
            }
        }

        fb_cleared = 0;

        ret = drmModeSetCrtc(drm_resources->fd, drm_resources->crtc_id,
                drm_resources->fb_id[draw_buff_index], 0, 0,
                &drm_resources->connector_id, 1, &drm_resources->mode);
        if (ret) {
            perror("Failed to set mode.");
            return NULL;
        }

        drm_resources->bo_index = (drm_resources->bo_index)^1;
        usleep(50 * 1000);
    }
    return NULL;
}

int touch_response()
{
    int ret;
    int i;
    int current_slot = 0;
    struct input_event ev[64];

    for (i=0;i<MAX_TOUCH;i++) {
        slot[i].btn_touch = 0;
        slot[i].tracking_id = -1;
        slot[i].pos_x = -1;
        slot[i].pos_y = -1;
    }

    /* Touch event driven loop */
    for (;;) {
        if (stop_exit == 1)
            break;

        ret = read(event_resources->fd, ev, sizeof(struct input_event) * 64);
        if (ret < (int) sizeof(struct input_event)) {
            printf("Reading error.\n");
            return -1;
        }

        for (i = 0;i < ret / sizeof(struct input_event); i++) {
            if (ev[i].type == EV_SYN)
                pthread_cond_signal(&cond1);
            else {
                if (ev[i].type == EV_ABS) {
                    if (ev[i].code == ABS_MT_SLOT) {
                        if (ev[i].value < MAX_TOUCH)
                            current_slot = ev[i].value;
                        else
                            continue;
                    }
                    else if (ev[i].code == ABS_MT_TRACKING_ID) {
                        pthread_mutex_lock(&lock);
                        slot[current_slot].tracking_id = ev[i].value;
                        if (slot[current_slot].tracking_id == -1) {
                            slot[current_slot].pos_x = -1;
                            slot[current_slot].pos_y = -1;
                            slot[current_slot].btn_touch = 0;
                            pthread_cond_signal(&cond1);
                        }
                        pthread_mutex_unlock(&lock);
                    }
                    else if (ev[i].code == ABS_MT_POSITION_X)
                        slot[current_slot].pos_y = ev[i].value;
                     /* slot[current_slot].pos_x = ev[i].value; */
                    else if (ev[i].code == ABS_MT_POSITION_Y)
                        slot[current_slot].pos_x = ev[i].value;
                     /* slot[current_slot].pos_y = ev[i].value; */
                }
                else if (ev[i].type == EV_KEY) {
                    if (ev[i].code == BTN_TOUCH) {
                        slot[current_slot].btn_touch = ev[i].value;
                        if (ev[i].value == 0) {
                            slot[current_slot].pos_x = -1;
                            slot[current_slot].pos_y = -1;
                            slot[current_slot].tracking_id = -1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

int show_inputs()
{
    int i;
    int fd;
    int ret;
    int version;
    char event_dev_path[64];
    char name[256] = "Unknown";

    printf("Input devices:\n");
    printf("Index\tName\t\tDriver\n");
    for (i=0;i<MAX_INPUT_DEV;i++) {
        sprintf(event_dev_path, "%s%d", PATH_TOUCH_DEV, i);
        fd = open(event_dev_path, O_RDONLY);
        if (fd == -1)
            continue;

        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (ret) {
            close(fd);
            continue;
        }
        
        ret = ioctl(fd, EVIOCGVERSION, &version);
        if (ret) {
            close(fd);
            continue;
        }
        printf("%d\t", i);
        printf("%s\t", name);
        printf("%d.%d.%d\n", version >> 16, (version >> 8) & 0xff,
                version & 0xff);

        close(fd);
    }
    printf("\n");

    return 0;
}

int show_drm_info()
{
    int i;
    int j;
    int fd;
    int ret;
    int index = 0;
    int connected;
    drmModeResPtr resources;
    drmModeConnectorPtr connector;
    drmModeModeInfo mode;
    uint32_t width;
    uint32_t height;
    uint32_t crtcs;

    for (i=0;i<sizeof(drm_module_list);i++) {
        fd = drmOpen(drm_module_list[i], NULL);
        if (fd >= 0)
            break;
    }

    if (fd < 0) {
        perror("Failed to open drm device");
        return -1;
    }

    printf("DRM details:\n");
    printf("%s module in use\n", drm_module_list[i]);

    resources = drmModeGetResources(fd);

    printf("Index\tConnectors\tModes\t\tPossible CRTCs\n");
    for (i=0;i<resources->count_connectors;i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED) {
            mode = connector->modes[0];
            printf("%d\t%u\t", index, resources->connectors[i]);
            printf("\t%dx%d\t\t", mode.hdisplay, mode.vdisplay);
            crtcs = drmModeConnectorGetPossibleCrtcs(fd, connector);
            for (j=0;j<resources->count_crtcs; j++) {
                if (crtcs & (1 << j)) {
                    drm_index[index].connector_id = resources->connectors[i];
                    drm_index[index].crtc_id = resources->crtcs[j];
                    printf("%d ", resources->crtcs[j]);
                }
            }
            index++;
            printf("\n");
        }
    }
    printf("\n");

    drmClose(fd);
    return 0;
}

void show_usage()
{
    printf("-s Show all the DRM and Event options\n");
    printf("-e [Event index] Use the Event device index\n");
    printf("-d [DRM index] Use the DRM device index\n");
}

int main(int argc, char *argv[])
{
    int opt;
    int event_index;
    int drm_index;
    int show = 0;
    int drm_ok = 0;
    int event_ok = 0;
    struct sigaction sigact;

    /* Signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = catch;
    sigaction(SIGINT, &sigact, NULL);

    while ((opt = getopt(argc, argv, "se:d:")) != -1) {
        switch (opt) {
            case 's':
                show =1;
            break;
            case 'e':
                event_index = atoi(optarg);
                event_ok = 1;
            break;
            case 'd':
                drm_index = atoi(optarg);
                drm_ok = 1;
            break;
            default: /* '?' */
                show_usage();
                exit(EXIT_FAILURE);
        }
    }

    if (show ==1 && (event_ok == 1 || drm_ok == 1)) {
        perror("-s option cannot be used with other options\n");
        return -1;
    }

    if (show == 1) {
        show_inputs();
        show_drm_info();
        return 0;
    }

    if (drm_ok == 0 || event_ok == 0) {
        perror("both -e and -d options have to be provided together\n");
        return -1;
    }

    pthread_t update_display_id;
    pthread_create(&update_display_id, NULL, update_display, NULL);

    /* Display Init */
    show_drm_info();
    if (drm_init(drm_index)) {
        printf("DRM init failed.\n");
        return -1;
    }

    /* Touch Init */
    if (event_init(event_index)) {
        printf("Event init failed.\n");
        return -1;
    }

    /* Draw circles in a buffer */
    if (create_circle(DIA)) {
        printf("Failed to create circles.\n");
        return -1;
    }

    /* Main loop */
    touch_response();

    destroy_circle();
    event_destroy();
    drm_destroy();
    pthread_join(update_display_id, NULL);
    return 0;
}
