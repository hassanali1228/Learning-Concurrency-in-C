#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>

#include "lab_png.h"

int main(int argc, char *argv[]) 
{

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory name>\n", argv[0]);
        exit(1);
    }

    char *directory = argv[1];

    if (directory[strlen(directory)-1] == '/') directory[strlen(directory)-1] = '\0';

    if ( !(search_dir(directory)) ) printf("findpng: No PNG file found\n");

    return 0;
}

int search_dir(char *directory){

    DIR *p_dir;
    struct dirent *p_dirent;

    char *str_path;
    int str_type;
    char full_path[PATH_MAX];

    int png_count = 0;

    if ((p_dir = opendir(directory)) == NULL) {
        char str[64];

        sprintf(str, "opendir(%s)", directory);
        perror(str);
        exit(2);
    }

    while ((p_dirent = readdir(p_dir)) != NULL) {
        
        str_path = p_dirent->d_name;
        str_type = p_dirent->d_type;   //4=DT_DIR, 8=DT_REG, 10=DT_LNK

            if (str_type == DT_DIR && ((strcmp(str_path, ".") != 0) && (strcmp(str_path, "..") != 0))){

                sprintf(full_path, "%s/%s", directory, str_path);

                png_count += search_dir(full_path);

            } else if (str_type == DT_REG){

                int len = strlen(str_path);
                
                if (str_path[len-1] == 'g' &&
                    str_path[len-2] == 'n' &&
                    str_path[len-3] == 'p' &&
                    str_path[len-4] == '.' ) {

                        sprintf(full_path, "%s/%s", directory, str_path);

                        FILE *fp = fopen(full_path, "rb");

                        U8 signature[PNG_SIG_SIZE];

                        if (fp == NULL){
                            printf("%s can not be opened", full_path);
                            exit(2);
                        }
                        
                        fscanf(fp, "%s", signature);

                        if ((is_png(signature, sizeof(signature))))
                        {
                            png_count++;
                            printf("%s\n", full_path);
                        }

                    }
            
            }

    }

    if ( closedir(p_dir) != 0 ) {
        perror("closedir");
        exit(3);
    }

    return png_count;
    
}

int is_png(U8 *buf, size_t n)
{
    return (buf[1] == 0x50 && buf[2] == 0x4e && buf[3] == 0x47);
}