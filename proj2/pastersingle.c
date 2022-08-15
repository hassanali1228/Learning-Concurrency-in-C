/*
 * The pastersingle.c code is
 * Hassan Ali Amjad, <h4amjad@uwaterloo.ca> & Harsh Gandhi, <h6gandhi@uwaterloo.ca>.
 */

#include "lab_png.h"

int main(int argc, char **argv)
{
    CURLcode res;
    char url[256];
    RECV_BUF recv_buf;
    int image_strip_count = 50;

    RECV_BUF horizontal_strips[image_strip_count];     //array of RECV_BUF structs
    memset(&horizontal_strips, -1, sizeof(horizontal_strips));

    char *image_buffer = malloc(BUF_SIZE*50);
    
    size_t offset = 0;
    int counter = 0;
    CURL *curl_handle;

    int init_status = recv_buf_init(&recv_buf, BUF_SIZE);
    if (init_status) return init_status;

    if (argc == 1)
    {
        strcpy(url, IMG_URL);
        sprintf(url, IMG_URL, 1, 1);
    }
    else
    {
        strcpy(url, argv[1]);
        sprintf(url, IMG_URL, 1, 1);
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL)
    {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return 1;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    while (1)
    {
        /* get it! */
        memset(&recv_buf, 0, sizeof(RECV_BUF));
        res = curl_easy_perform(curl_handle);

        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        
        if ( (recv_buf).seq < image_strip_count && horizontal_strips[(recv_buf).seq].seq == -1 )
        {
            memcpy(image_buffer + offset, (recv_buf).buf, (recv_buf).size);

            (recv_buf).buf = image_buffer + offset;

            offset += (recv_buf).max_size;

            horizontal_strips[(recv_buf).seq] = (recv_buf);
            counter++;
        }

        if (counter == image_strip_count) break;
    }

    catpng(image_strip_count, horizontal_strips);

    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    memset(&recv_buf, 0, sizeof(RECV_BUF));
    recv_buf_cleanup(&recv_buf);
    free(image_buffer);

    return 0;
}

int catpng(int image_strip_count, RECV_BUF images_buffer[])
{

    data_IHDR_p IHDR = malloc(DATA_IHDR_SIZE);
    U8* all_SIG_IHDR = malloc(PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE+DATA_IHDR_SIZE+CHUNK_CRC_SIZE);
    U8 *all_IEND = malloc(CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE+CHUNK_CRC_SIZE);

    char *data_ptr;
    U32 total_height = 0;
    U32 total_IDAT_length = 0;

    U64 uncompressed_length = 0;
    U32 current_IDAT_length = 0;
    U64 compressed_length = 0;

    for (int i = 0; i < image_strip_count; ++i)     //get lengths for compressed & uncompressed buffers
    {
        data_ptr = images_buffer[i].buf;

        data_ptr += (PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE);
        memcpy(IHDR, data_ptr, DATA_IHDR_SIZE);

        uncompressed_length += (ntohl(IHDR->height) * (4 * ntohl(IHDR->width) + 1));

        data_ptr += (DATA_IHDR_SIZE+CHUNK_CRC_SIZE);

        memcpy(&current_IDAT_length, data_ptr, 4);

        compressed_length += ntohl(current_IDAT_length);
    }

    compressed_length *= image_strip_count;
    U8 *final_buffer_uncompressed = malloc(uncompressed_length);    // for all inflated data
    U8 *final_buffer_compressed = malloc(compressed_length);
    int offset = 0;                                                 //to increment inflated data in one buffer

    U8 IDAT_type[4];

    for (int i = 0; i < image_strip_count; ++i){    //inflate and store data in uncompressed buffer

        data_ptr = images_buffer[i].buf;

        if (i == 0) memcpy(all_SIG_IHDR, data_ptr, PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE+DATA_IHDR_SIZE+CHUNK_CRC_SIZE);    //assign first IHDR to final IHDR

        data_ptr += (PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE);
        memcpy(IHDR, data_ptr, DATA_IHDR_SIZE);

        total_height += ntohl(IHDR->height);

        data_ptr += (DATA_IHDR_SIZE+CHUNK_CRC_SIZE);
        current_IDAT_length = 0;
        memcpy(&current_IDAT_length, data_ptr, 4);
        current_IDAT_length = ntohl(current_IDAT_length);
        total_IDAT_length += current_IDAT_length;

        data_ptr += CHUNK_LEN_SIZE;
        if (i == 0) memcpy(&IDAT_type, data_ptr, 4);

        U8 *compressed_data = malloc(current_IDAT_length);
        data_ptr += CHUNK_TYPE_SIZE;
        memcpy(compressed_data, data_ptr, current_IDAT_length);

        U64 length_uncompressed = ntohl(IHDR->height) * (4 * ntohl(IHDR->width) + 1);
        U8 *uncompressed_data = malloc(length_uncompressed);

        int status = mem_inf(uncompressed_data, &length_uncompressed, compressed_data, current_IDAT_length);
        if (status)
            fprintf(stderr, "mem_inf failed. ret = %d.\n", status);

        memcpy(final_buffer_uncompressed + offset, uncompressed_data, length_uncompressed);

        offset += length_uncompressed;

        data_ptr += (current_IDAT_length+CHUNK_CRC_SIZE);

        if (i == 0){    //read IEND data from first file
            memcpy(all_IEND, data_ptr, 12);
        }
    }

    total_height = htonl(total_height);
    memcpy(all_SIG_IHDR+PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE+4, &total_height, 4);    //update height in IHDR data

    //compute IHDR crc

    U32 buf_size = CHUNK_TYPE_SIZE + DATA_IHDR_SIZE;
    U8 *IHDR_type_data = malloc(buf_size);
    memcpy(IHDR_type_data, all_SIG_IHDR+8+4, buf_size);
    U32 crc_val = ntohl(crc(IHDR_type_data, buf_size));
    memcpy(all_SIG_IHDR+8+8+13, &crc_val, CHUNK_CRC_SIZE);  //store crc val

    int status = mem_def(final_buffer_compressed, &compressed_length, final_buffer_uncompressed, uncompressed_length, Z_DEFAULT_COMPRESSION);
    if (status != 0)
        fprintf(stderr, "mem_def failed. ret = %d.\n", status);

    U32 IDAT_length = (U32)compressed_length;

    U8 *IDAT_chunk = malloc(CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE + IDAT_length + CHUNK_CRC_SIZE);

    int IDAT_length_htonl = htonl(compressed_length);
    memcpy(IDAT_chunk, &IDAT_length_htonl, 4);
    memcpy(IDAT_chunk+CHUNK_LEN_SIZE, &IDAT_type, 4);
    memcpy(IDAT_chunk+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE, final_buffer_compressed, IDAT_length);

    //compute IDAT crc

    buf_size = CHUNK_TYPE_SIZE + IDAT_length;
    U8 *IDAT_type_data = malloc(buf_size);
    memcpy(IDAT_type_data, IDAT_chunk+CHUNK_LEN_SIZE, buf_size);
    crc_val = ntohl(crc(IDAT_type_data, buf_size));
    memcpy(IDAT_chunk+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE+IDAT_length, &crc_val, CHUNK_CRC_SIZE);  //store crc val

    FILE *all_fp = fopen("all.png", "a+");

    fwrite(all_SIG_IHDR, 8+8+13+4, 1, all_fp);
    fwrite(IDAT_chunk, 8+IDAT_length+4, 1, all_fp);
    fwrite(all_IEND, 12, 1, all_fp);

    fclose(all_fp);

    return 0;
}