#include "lab_png.h"

int main(int argc, char **argv)
{

    if (argc != 6)
    {
        fprintf(stderr, "Usage: %s <B> <P> <C> <X> <N>\n", argv[0]);
        return -1;
    }

    int buf_size = atoi(argv[1]);
    int prod_cnt = atoi(argv[2]);
    int cons_cnt = atoi(argv[3]);
    int wait_time = atoi(argv[4]);
    int img_number = atoi(argv[5]);

    char url[256];

    // initialize Semaphores in shared memory
    sem_t *mutex_p;
    sem_t *mutex_c;
    sem_t *empty;
    sem_t *filled;
    sem_t *buffer_lock;

    void *inf;
    void *def;
    int *img_attr;
    int *counter_p;
    int *counter_c;
    struct recv_stack *stack;

    // initialize Shared memory region variables (Sem)
    int mutex_p_id = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | 0600);
    int mutex_c_id = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | 0600);
    int empty_id = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | 0600);
    int filled_id = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | 0600);
    int buffer_lock_id = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | 0600);

    // initialize Shared memory region variables (Not Sem)
    int img_attr_id = shmget(IPC_PRIVATE, sizeof(int)*2, IPC_CREAT | 0600);
    int inf_id = shmget(IPC_PRIVATE, (9606 * 50), IPC_CREAT | 0600);
    int def_id = shmget(IPC_PRIVATE, (10000 * 50), IPC_CREAT | 0600);
    int cntr_p_id = shmget(IPC_PRIVATE, 32, IPC_CREAT | 0600);           
    int cntr_c_id = shmget(IPC_PRIVATE, 32, IPC_CREAT | 0600);         
    int stack_id = shmget(IPC_PRIVATE, sizeof_shm_stack(buf_size), IPC_CREAT | 0600);

    if (mutex_p_id == -1 || mutex_c_id == -1 || empty_id == -1 || filled_id == -1 || buffer_lock_id == -1 ||
            inf_id == -1 || def_id == -1 || cntr_p_id == -1 || stack_id == -1) {
        perror("shmget failed");
        abort();
    }

    
    mutex_p = shmat(mutex_p_id, NULL, 0);
    mutex_c = shmat(mutex_c_id, NULL, 0);
    empty = shmat(empty_id, NULL, 0);
    filled = shmat(filled_id, NULL, 0);
    buffer_lock = shmat(buffer_lock_id, NULL, 0);

    inf = shmat(inf_id, NULL, 0);
    def = shmat(def_id, NULL, 0);
    counter_p = shmat(cntr_p_id, NULL, 0);
    counter_c = shmat(cntr_c_id, NULL, 0);
    stack = shmat(stack_id, NULL, 0);
    img_attr = (int *)shmat(img_attr_id, 0, 0);

    if (inf == (void *) -1 || def == (void *) -1 || counter_p == (void *) -1 || counter_c == (void *) -1 || stack == (void *) -1) {
        perror("memory shmat failed");
        exit(-1);
    }
    
    if ( mutex_p == (void *) -1 || mutex_c == (void *) -1 || empty == (void *) -1 
            || filled == (void *)-1 || buffer_lock == (void *) -1 ) {
        perror("semaphore shmat failed");
        abort();
    }

    sem_init(mutex_p, 1, 1);
    sem_init(mutex_c, 1, 1);
    sem_init(empty, 1, buf_size); //user input buf_size
    sem_init(filled, 1, 0);
    sem_init(buffer_lock, 1, 1);

    init_shm_stack(stack, buf_size);

    // GET INIT TIME
    double times[2];
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
    {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec / 1000000.;

    pid_t prod_pids[prod_cnt];
    pid_t cons_pids[cons_cnt];

    for (int i = 0; i < prod_cnt; ++i)
    {

        pid_t pid= fork();

        if (pid > 0)
        { /* parent proc */
            prod_pids[i] = pid;
        }
        else if (pid == 0)
        { /* child proc */
            int local_counter = 0;

            while(1){
            
                sem_wait(mutex_p);

                    if (*counter_p == 50) {
                        sem_post(mutex_p);
                        break; //all 50 images received

                    }

                    local_counter = *counter_p;
                    *counter_p += 1;
                
                sem_post(mutex_p);

                RECV_BUF p_recv_buf;

                int recv_init_status = recv_buf_init(&p_recv_buf, BUF_SIZE, def + (local_counter * 10000));
                
                if (recv_init_status == 1)
                {
                    fprintf(stderr, "recv_buf pointer passed in = NULL\n");
                    return recv_init_status;
                }
                else if (recv_init_status == 2)
                {
                    fprintf(stderr, "unable to malloc stack for recv_buf\n");
                    return recv_init_status;
                }

                sprintf(url, IMG_URL, (local_counter % 3) + 1, img_number, local_counter);

                CURL *curl_handle;
                curl_global_init(CURL_GLOBAL_DEFAULT);

                // // /* init a curl session */
                curl_handle = curl_easy_init();

                if (curl_handle == NULL)
                {
                    fprintf(stderr, "curl_easy_init: returned NULL\n");
                    return 1;
                }

                // /* specify URL to get */
                curl_easy_setopt(curl_handle, CURLOPT_URL, url);

                // /* register write call back function to process received data */
                curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);

                // /* user defined data structure passed to the call back function */
                curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&p_recv_buf);

                // /* register header call back function to process received header data */
                curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);

                // /* user defined data structure passed to the call back function */
                curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&p_recv_buf);

                // /* some servers requires a user-agent field */
                curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

                // /* get it! */
                CURLcode res = curl_easy_perform(curl_handle);

                if (res != CURLE_OK) {
                    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                }

                sem_wait(empty); // wait to see if the stack is empty
                sem_wait(buffer_lock); // lock the stack
                
                    int ret = push(stack, p_recv_buf);
                    if ( ret != 0 ) {
                        perror("can not push to stack");
                        exit(ret);
                    }
                                           
                sem_post(buffer_lock); // unlock the stack
                sem_post(filled); // add to the consumer counter_p

                /* cleaning up */
                curl_easy_cleanup(curl_handle);
                curl_global_cleanup();
            }

            exit(0);
        }
        else
        {
            perror("fork");
            abort();
        }
    }

    for (int i = 0; i < cons_cnt; ++i)
    {

        pid_t pid = fork();

        if (pid > 0)
        { /* parent proc */
            cons_pids[i] = pid;
        }
        else if (pid == 0)
        { /* child proc */
            while (1){

                //break if full
                sem_wait(mutex_c);
                
                    if((*counter_c) == 50){
                        sem_post(mutex_c);
                        break;
                    }
                    (*counter_c)++;
                    
                sem_post(mutex_c);
                
                sem_wait(filled); // wait to see if the stack is empty
                sem_wait(buffer_lock); // lock the stack

                    RECV_BUF temp;

                    int ret = pop(stack, &temp);
                    if ( ret != 0 ) {
                        perror("can not pop from stack");
                        exit(ret);
                    }
                    
                    char header_info[4];
                    memcpy(&header_info, temp.buf,4);

                sem_post(buffer_lock); // unlock the stack
                sem_post(empty);

                usleep(wait_time*1000);

                int temp_width;
                    if (temp.seq == 0){ 
                        memcpy(&temp_width, temp.buf+8+4+4,4);
                        img_attr[0] = ntohl(temp_width);
                    }    

                int temp_height;
                memcpy(&temp_height, temp.buf+8+4+4+4,4);
                img_attr[1] += ntohl(temp_height);
                
                U64 inf_length = 0;

                int status = mem_inf(inf+((temp.seq)*(9606)), &inf_length, (U8 *)(temp.buf+41), temp.size-41-16);
                if (status != 0)
                    fprintf(stderr, "mem_inf failed. ret = %d.\n", status);

            }
            exit(0);
        }
        else
        {
            perror("fork");
            abort();
        }
    }

    for (int i = 0; i < prod_cnt; ++i)
        wait(prod_pids[i]);
    for (int i = 0; i < cons_cnt; ++i)
        wait(cons_pids[i]);

    // Concatenate png
    catpng(img_attr[0], img_attr[1], inf);

    // GET END TIME
    if (gettimeofday(&tv, NULL) != 0)
    {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec / 1000000.;

    fflush(stdout);
    printf("paster2 execution time: %.6lf seconds\n", times[1] - times[0]);

    //destroy semaphoore
    sem_destroy(mutex_p);
    sem_destroy(mutex_c);
    sem_destroy(filled);
    sem_destroy(empty);
    sem_destroy(buffer_lock);

    //detach memory for semaphores
    shmdt(&mutex_p);
    shmdt(&mutex_c);
    shmdt(&empty);
    shmdt(&filled);
    shmdt(&buffer_lock);
    
    //detach memory for shared memory regions
    shmdt(inf);
    shmdt(def);
    shmdt(counter_p);
    shmdt(counter_c);
    shmdt(stack);
    shmdt(img_attr);

    //remove shared semaphore regions
    shmctl(mutex_p_id, IPC_RMID, NULL);
    shmctl(mutex_c_id, IPC_RMID, NULL);
    shmctl(empty_id, IPC_RMID, NULL);
    shmctl(filled_id, IPC_RMID, NULL);
    shmctl(buffer_lock_id, IPC_RMID, NULL);

    //remove shared memory regions
    shmctl(inf_id, IPC_RMID, NULL);
    shmctl(def_id, IPC_RMID, NULL);
    shmctl(cntr_p_id, IPC_RMID, NULL);
    shmctl(cntr_c_id, IPC_RMID, NULL);
    shmctl(stack_id, IPC_RMID, NULL);
    shmctl(img_attr_id, IPC_RMID, NULL);

    return 0;
}

int catpng(int width, int height, void *inf){

    //GET DEFLATED DATA

    U64 compressed_length;
    U8 *final_def = malloc(10000*50);

    int status = mem_def(final_def, &compressed_length, inf, 9606*50, Z_DEFAULT_COMPRESSION);
    if (status != 0)
        fprintf(stderr, "mem_def failed. ret = %d.\n", status);

    FILE *fp = fopen("all.png", "wb+");

    //WRITE PNG HEADER

    U8 header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; ++i) fputc(header[i], fp);

    //WRITE IHDR DATA

    U8 IHDR_len_type[] = {0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52};
    for (int i = 0; i < 8; ++i) fputc(IHDR_len_type[i], fp);
        
    int width_f = htonl(width);
    int height_f = htonl(height);
    fwrite((U32 *)&width_f , 4, 1, fp);
    fwrite((U32 *)&height_f, 4, 1, fp);
    char IHDR_data[5] = {0x08,0x06,0x00,0x00,0x00};
    for (int i = 0; i < 5; ++i) fputc(IHDR_data[i], fp);
    
    fseek(fp, PNG_SIG_SIZE + CHUNK_LEN_SIZE, SEEK_SET);
    int buf_size = DATA_IHDR_SIZE + CHUNK_TYPE_SIZE;
    unsigned char ihdr_type_data[buf_size];
    fread(&ihdr_type_data, buf_size, 1, fp);
    U32 crc_val = ntohl(crc(ihdr_type_data, buf_size));
    fwrite(&crc_val, CHUNK_CRC_SIZE, 1, fp);

    //WRITE IDATA DATA
    
    int length_f = htonl(compressed_length);
    fwrite((U32 *)&length_f, 4, 1, fp);

    U8 IDAT_type[] = {0x49,0x44,0x41,0x54};
    for (int i = 0; i < 4; ++i) fputc(IDAT_type[i], fp);
    
    fwrite(final_def, (int)compressed_length, 1, fp);
    
    fseek(fp, 8 + 8 + 13 + 4 + 4, SEEK_SET);
    buf_size = (int)compressed_length + CHUNK_TYPE_SIZE;
    unsigned char idat_type_data[buf_size];
    fread(&idat_type_data, buf_size, 1, fp);
    crc_val = ntohl(crc(idat_type_data, buf_size));
    fwrite(&crc_val, CHUNK_CRC_SIZE, 1, fp);

    //WRITE IEND DATA
    U8 IEND_len_type[] = {0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44};
    for (int i = 0; i < 8; ++i) fputc(IEND_len_type[i], fp);
    
    fseek(fp, -4, SEEK_CUR);
    unsigned char iend_type_data[CHUNK_TYPE_SIZE];
    fread(&iend_type_data, CHUNK_TYPE_SIZE, 1, fp);
    crc_val = ntohl(crc(iend_type_data, CHUNK_TYPE_SIZE));
    fwrite(&crc_val, CHUNK_CRC_SIZE, 1, fp);

    fclose(fp);

    free(final_def);
    
    return 0;
}