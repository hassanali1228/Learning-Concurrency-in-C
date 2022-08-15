/**
 * @brief  micros and structures for a simple PNG file
 *
 * Copyright 2018-2020 Yiqing Huang
 *
 * This software may be freely redistributed under the terms of MIT License
 */
#pragma once

/******************************************************************************
 * INCLUDE HEADER FILES
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <time.h>
#include <sys/time.h>
#include <semaphore.h>
#include <sys/shm.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/queue.h>

#include "zutil.h" /* for mem_def() and mem_inf() */
#include "crc.h"
#include "shm_stack.h"

/******************************************************************************
 * DEFINED MACROS
 *****************************************************************************/

#define PNG_SIG_SIZE 8    /* number of bytes of png image signature data */
#define CHUNK_LEN_SIZE 4  /* chunk length field size in bytes */
#define CHUNK_TYPE_SIZE 4 /* chunk type field size in bytes */
#define CHUNK_CRC_SIZE 4  /* chunk CRC field size in bytes */
#define DATA_IHDR_SIZE 13 /* IHDR chunk data field size */

#define IMG_URL "http://ece252-%d.uwaterloo.ca:2530/image?img=%d&part=%d"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576 /* 1024*1024 = 1M */
#define BUF_INC 524288   /* 1024*512  = 0.5M */

/******************************************************************************
 * STRUCTURES and TYPEDEFS
 *****************************************************************************/
typedef unsigned int U32;

typedef struct {
    long mtype;
    RECV_BUF recv_buf;
} msg_t;

typedef struct chunk
{
    U32 length; /* length of data in the chunk, host byte order */
    U8 type[4]; /* chunk type */
    U8 *p_data; /* pointer to location where the actual data are */
    U32 crc;    /* CRC field  */
} * chunk_p;

/* note that there are 13 Bytes valid data, compiler will padd 3 bytes to make
   the structure 16 Bytes due to alignment. So do not use the size of this
   structure as the actual data size, use 13 Bytes (i.e DATA_IHDR_SIZE macro).
 */
typedef struct data_IHDR
{                   // IHDR chunk data
    U32 width;      /* width in pixels, big endian   */
    U32 height;     /* height in pixels, big endian  */
    U8 bit_depth;   /* num of bits per sample or per palette index.
                       valid values are: 1, 2, 4, 8, 16 */
    U8 color_type;  /* =0: Grayscale; =2: Truecolor; =3 Indexed-color
                       =4: Greyscale with alpha; =6: Truecolor with alpha */
    U8 compression; /* only method 0 is defined for now */
    U8 filter;      /* only method 0 is defined for now */
    U8 interlace;   /* =0: no interlace; =1: Adam7 interlace */
} * data_IHDR_p;

/* A simple PNG file format, three chunks only*/
typedef struct simple_PNG
{
    U8 *p_IHDR;
    U8 *p_IDAT; /* only handles one IDAT chunk */
    U8 *p_IEND;
} * simple_PNG_p;

/******************************************************************************
 * FUNCTION PROTOTYPES
 *****************************************************************************/
int is_png(U8 *buf, size_t n);
int get_png_height(struct data_IHDR *buf);
int get_png_width(struct data_IHDR *buf);
int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence);

void *curl_operation(void *url);

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size, void *sh_addr);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
int catpng(int width, int height, void *inf);

#define max(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

/**
 * @brief  cURL header call back function to extract image sequence number from
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
        strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0)
    {

        /* extract img sequence number */
        p->seq = atoi(p_recv + strlen(ECE252_HEADER));
    }
    return realsize;
}

/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv,
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size, void *sh_addr)
{
    ptr->buf = sh_addr;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1; /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL)
    {
        return 1;
    }

    free(ptr->buf);
    ptr->buf = NULL;
    ptr->size = 0;
    ptr->max_size = 0;
    ptr->seq = 0;
    return 0;
}

/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL)
    {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL)
    {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL)
    {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len)
    {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3;
    }
    return fclose(fp);
}