#include "lab_png.h"

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("Usage: %s <png1> <png2> <png3> ... \n", argv[0]);
        return 1; // FAILURE
    }

    return catpng(argc, argv);
}

int catpng(int filecount, char *files[]){   
    FILE *fp_current;

    data_IHDR_p IHDR = malloc(DATA_IHDR_SIZE);
    U64 uncompressed_length = 0;
    U64 current_IDAT_length = 0;
    U64 compressed_length = 0;

    for (int i = 1; i < filecount; i++)
    {
        fp_current = fopen(files[i], "rb");

        get_png_data_IHDR(IHDR, fp_current, PNG_SIG_SIZE + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE, SEEK_SET);
        U64 current_mem_inf_length = ntohl(IHDR->height) * (4 * ntohl(IHDR->width) + 1);
        uncompressed_length += current_mem_inf_length;

        fseek(fp_current, 8+4+4+13+4, SEEK_SET);

        fread(&current_IDAT_length, 4, 1, fp_current);

        compressed_length += ntohl(current_IDAT_length);
       
        fclose(fp_current);
    }

    compressed_length = filecount * compressed_length;
    U8 *final_buffer_uncompressed = malloc(uncompressed_length); // OVER ALL BUFFER
    U8 *final_buffer_compressed = malloc(compressed_length);

    int final_height = 0;

    int prev_data_size = 0;

    for (int i = 1; i < filecount; i++)
    {
        fp_current = fopen(files[i], "rb");

        get_png_data_IHDR(IHDR, fp_current, PNG_SIG_SIZE + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE, SEEK_SET);

        final_height += ntohl(IHDR->height);    // height of all.png

        int length_compressed;                 // read length of IDAT chunk
        fseek(fp_current, 33, 0);
        fread(&length_compressed, 4, 1, fp_current);
        length_compressed = ntohl(length_compressed);

        fseek(fp_current, -4, SEEK_CUR);    // reset to start of IDAT

        U8 *compressed_data = malloc(length_compressed);                                // OG deflated data
        U64 length_uncompressed = ntohl(IHDR->height) * (4 * ntohl(IHDR->width) + 1);   // size of inflated data
        U8 *uncompressed_data = malloc(length_uncompressed);                            // inflated data buffer

        chunk_p idatChunk = malloc(length_compressed + 12);            // read entire IDAT Chunk
        fread(idatChunk, (length_compressed + 12), 1, fp_current);

        // reassign values
        fseek(fp_current, 33 + 8, 0);                               // skip to data part of IDAT chunk
        fread(compressed_data, length_compressed, 1, fp_current);   // read deflated data into compressed_data
        fread(&(idatChunk->crc), 4, 1, fp_current);                 // read old crc into new crc

        int status = mem_inf(uncompressed_data, &length_uncompressed, compressed_data, length_compressed);     //inflating data
        if (status != 0) fprintf(stderr, "mem_inf failed. ret = %d.\n", status);

        memcpy(final_buffer_uncompressed + prev_data_size, uncompressed_data, length_uncompressed);     //copy curr inf data to full buffer
        prev_data_size += length_uncompressed;

        // free
        free(compressed_data);
        free(uncompressed_data);
        free(idatChunk);

        fclose(fp_current);
    }

    int status = mem_def(final_buffer_compressed, &compressed_length, final_buffer_uncompressed, uncompressed_length, Z_DEFAULT_COMPRESSION);
    // deflates final_buffer_uncompressed into final_buffer_compressed

    if (status != 0)  fprintf(stderr, "mem_def failed. ret = %d.\n", status);

    FILE *fp = fopen(files[1], "rb");        //template file
    FILE *fp_all = fopen("all.png", "a+");

    U8 signature[PNG_SIG_SIZE];
    fread(&signature, 8, 1, fp);
    fwrite(&signature, 8, 1, fp_all);

    // IHDR
    U32 length = ntohl(13);
    U8 IHDR_type[4];
    fseek(fp, 4, SEEK_CUR);
    fread(&IHDR_type, 4, 1, fp);

    // IHDR data
    get_png_data_IHDR(IHDR, fp, PNG_SIG_SIZE + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE, SEEK_SET);
    IHDR->height = htonl(final_height);

    // IHDR
    fwrite(&length, CHUNK_LEN_SIZE, 1, fp_all);
    fwrite(&IHDR_type, CHUNK_TYPE_SIZE, 1, fp_all);
    fwrite(&(IHDR->width), 4, 1, fp_all);
    fwrite(&(IHDR->height), 4, 1, fp_all);
    fwrite(&(IHDR->bit_depth), 1, 1, fp_all);
    fwrite(&(IHDR->color_type), 1, 1, fp_all);
    fwrite(&(IHDR->compression), 1, 1, fp_all);
    fwrite(&(IHDR->filter), 1, 1, fp_all);
    fwrite(&(IHDR->interlace), 1, 1, fp_all);

    // Compute crc of IHDR
    fseek(fp_all, 8 + 4, SEEK_SET);
    int buf_size = 13 + CHUNK_TYPE_SIZE;
    unsigned char type_data[buf_size];
    fread(&type_data, buf_size, 1, fp_all);
    U32 crc_val = ntohl(crc(type_data, buf_size));
    fwrite(&crc_val, CHUNK_CRC_SIZE, 1, fp_all);

    // IDAT
    fseek(fp, 8 + 4 + 4 + 13 + 4 + 4, SEEK_SET); // go to type of IDAT chunk fp
    fseek(fp_all, 8 + 4 + 4 + 13 + 4, SEEK_SET); // go to length of IDAT chunk fp_all

    U8 IDAT_type[4];
    fread(&IDAT_type, CHUNK_TYPE_SIZE, 1, fp);  //read fp type
    U32 IDAT_length = ntohl(compressed_length); //use actual compressed IDAT size

    fwrite(&IDAT_length, CHUNK_LEN_SIZE, 1, fp_all);
    fwrite(&IDAT_type, CHUNK_TYPE_SIZE, 1, fp_all);
    fwrite(final_buffer_compressed, ntohl(IDAT_length), 1, fp_all);

    // compute crc of IDAT    
    fseek(fp_all, -(4 + ntohl(IDAT_length)), SEEK_CUR);
    buf_size = ntohl(IDAT_length) + CHUNK_TYPE_SIZE;
    U8 *IDAT_type_data = malloc(buf_size);
    fread(IDAT_type_data, buf_size, 1, fp_all);
    crc_val = ntohl(crc(IDAT_type_data, buf_size));
    fwrite(&crc_val, CHUNK_CRC_SIZE, 1, fp_all);


    // IEND
    U32 IEND_length = 0;
    U8 IEND_type[4];
    U32 IEND_crc;

    fseek(fp, -12, SEEK_END);
    fread(&IEND_length, CHUNK_TYPE_SIZE, 1, fp);
    fread(&IEND_type, CHUNK_TYPE_SIZE, 1, fp);
    fread(&IEND_crc, CHUNK_CRC_SIZE, 1, fp);

    fwrite(&IEND_length, CHUNK_LEN_SIZE, 1, fp_all);
    fwrite(&IEND_type, CHUNK_TYPE_SIZE, 1, fp_all);
    fwrite(&IEND_crc, CHUNK_CRC_SIZE, 1, fp_all);

    // free
    free(final_buffer_uncompressed);
    free(final_buffer_compressed);
    free(IDAT_type_data);
    free(IHDR);

    fclose(fp);
    fclose(fp_all);

    return 0;
}

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence)
{
    fseek(fp, offset, whence);

    return fread(out, DATA_IHDR_SIZE, 1, fp);
}
