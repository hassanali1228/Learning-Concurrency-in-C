#include "lab_png.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <png_filename>\n", argv[0]);
        return -1; // FAILURE
    }

    char *filepath = argv[1];

    FILE *fp = fopen(filepath, "rb");

    U8 signature[PNG_SIG_SIZE];

    if (fp == NULL)
    {
        printf("File not opened");
        return -1;
    }

    fread(signature,8,1,fp);

    if (!(is_png(signature, sizeof(signature))))
    {
        printf("%s: Not a PNG file\n", filepath);
        return -1;
    }

    data_IHDR_p IHDR = malloc(DATA_IHDR_SIZE);

    int output = get_png_data_IHDR(IHDR, fp, PNG_SIG_SIZE + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE, SEEK_SET);

    if (output != 1)
    {
        printf("IHDR memory block not read");
        return -1;
    }

    printf("%s: %d x %d\n", filepath, ntohl(IHDR->width), ntohl(IHDR->height));

    // get length
    fseek(fp, 8, SEEK_SET);

    int result = crc_check(fp, "IHDR");

    if (result == -1)
        return -1;

    while (1)
    {

        result = crc_check(fp, "IDAT");

        if (result == -1)
            return -1;
        else if (result == 1)
            break;
    }

    fseek(fp, -12, SEEK_END);

    result = crc_check(fp, "IEND");

    if (result == -1)
        return -1;

    fclose(fp);
    free(IHDR);

    return 0;
}

int crc_check(FILE *fp, char *chunk_type)
{
    // Read and store Length
    unsigned int length;
    fread(&length, CHUNK_LEN_SIZE, 1, fp);

    if (ntohl(length) == 0 && strcmp(chunk_type, "IEND") != 0)
        return 1; // last false IDAT

    int buf_size = ntohl(length) + CHUNK_TYPE_SIZE; // len(data) + len(type)

    unsigned char *type_data = malloc(buf_size);
    fread(type_data, buf_size, 1, fp); // store type + data contents in char[]

    U32 crc_val = ntohl(crc(type_data, buf_size)); // CRC calculation

    U32 listed_crc;
    fread(&listed_crc, 4, 1, fp); // load CRC from chunk

    if (listed_crc != crc_val)
    {
        printf("%s chunk CRC error: computed %x, expected %x\n", chunk_type, ntohl(crc_val), ntohl(listed_crc));
        return -1; // assume exit upon first CRC error
    }

    free(type_data);

    return 0;
}

int is_png(U8 *buf, size_t n)
{
    return (buf[0] == 0x89 && buf[1] == 0x50 && buf[2] == 0x4e && buf[3] == 0x47 &&
                buf[4] == 0x0d && buf[5] == 0x0a && buf[6] == 0x1a && buf[7] == 0x0a);
}

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence)
{
    fseek(fp, offset, whence);
    return fread(out, DATA_IHDR_SIZE, 1, fp);
}