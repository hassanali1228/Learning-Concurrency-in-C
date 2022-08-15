#include "lab_png.h"

// global

int image_counter = 0;
int waiting_thread = 1;

int hash_counter = 0;
int m = 50;
int t = 1;

// ENTRY **hash_pointers;
char *duplicated_values[1000];
char dp[1000][256];

// define mutexes
pthread_mutex_t hashmap_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t png_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_v = PTHREAD_COND_INITIALIZER;

void *curling(void *argument)
{

    thread_arguments *args = argument;

    while (1)
    {

        char url[256];

        pthread_mutex_lock(&stack_mutex);

        while (is_empty(args->frontier))
        {

            __sync_fetch_and_add(&waiting_thread, 1);
            pthread_cond_wait(&cond_v, &stack_mutex);
            if ((is_empty(args->frontier) && (__sync_fetch_and_add(&waiting_thread, 0) >= t)) || (m) <= __sync_fetch_and_add(&image_counter, 0)) // nothing to pop & every thing is waiting or counter is met
            {
                // printf("cond %d\n", __sync_fetch_and_add(&waiting_thread, 0));
                // printf("exited\n");
                //  pthread_cond_broadcast(&cond_v);
                pthread_mutex_unlock(&stack_mutex);
                return 0;
            }
            __sync_fetch_and_sub(&waiting_thread, 1);
        }

        pop(args->frontier, url);

        pthread_mutex_unlock(&stack_mutex);

        CURL *curl_handle;
        CURLcode res;
        RECV_BUF recv_buf;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle = easy_handle_init(&recv_buf, url);

        if (curl_handle == NULL)
        {
            fprintf(stderr, "Curl initialization failed. Exiting...\n");
            curl_global_cleanup();
            abort();
        }
        /* get it! */
        res = curl_easy_perform(curl_handle);

        if (res != CURLE_OK)
        {
            // fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            cleanup(curl_handle, &recv_buf);
            continue;
        }
        else
        {
            // printf("%lu bytes received in memory %p, seq=%d.\n",
            //        recv_buf.size, recv_buf.buf, recv_buf.seq);
        }

        /* process the download data */
        process_data(curl_handle, &recv_buf, args->frontier, args->png);
        pthread_mutex_lock(&stack_mutex);
        if ((is_empty(args->frontier) && (__sync_fetch_and_add(&waiting_thread, 0) >= t)) || (m) <= __sync_fetch_and_add(&image_counter, 0)) // nothing to pop & every thing is waiting or counter is met
        {
            pthread_cond_broadcast(&cond_v);
            cleanup(curl_handle, &recv_buf);
            pthread_mutex_unlock(&stack_mutex);
            break;
        }
        pthread_mutex_unlock(&stack_mutex);
        cleanup(curl_handle, &recv_buf);
    }

    return 0;
}

int main(int argc, char **argv)
{
    xmlInitParser();
    char url[256];
    int c;

    char *v = NULL;
    char *str = "option requires an argument";
    ENTRY e;

    // creates hashmap to store 1000 things
    hcreate(1000);
    // struct hsearch_data *htab = calloc(1, sizeof(struct hsearch_data));
    // hcreate_r(1000, htab);
    while ((c = getopt(argc, argv, "t:m:v:")) != -1)
    {
        switch (c)
        {
        case 't':
            t = strtoul(optarg, NULL, 10);
            // printf("option -t specifies a value of %d.\n", t);
            if (t <= 0)
            {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }

            break;

        case 'm':
            m = strtoul(optarg, NULL, 10);
            // printf("option -m specifies a value of %d.\n", m);
            if (m <= 0)
            {
                fprintf(stderr, "%s: %s > 0 -- 'm'\n", argv[0], str);
                return -1;
            }
            break;
        case 'v':
            v = optarg;
            // printf("option -v specifies a value of %s.\n", v);
            break;
        default:
            return -1;
        }
    }

    if(optind >= argc){
        fprintf(stderr, "Expected url argument after options\n");
        return -1;
    }
    strcpy(url, argv[optind]);

    // printf("url is %s , m = %d , t= %d, v=%s\n", url, m, t, v);

    pthread_t *p_tids = malloc(sizeof(pthread_t) * t);

    ISTACK *stack_thread_png = create_stack(1000);

    ISTACK *stack_thread_frontier = create_stack(1000);

    thread_arguments *thread_struct = malloc(500);
    thread_struct->frontier = stack_thread_frontier;
    thread_struct->png = stack_thread_png;

    // the size of the stack is subject to change, got to clarify, as well as above

    init_shm_stack(stack_thread_frontier, 1000);
    init_shm_stack(stack_thread_png, 1000);

    // GET INIT TIME
    double times[2];
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
    {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec / 1000000.;

    // add seed url to the hashmap
    e.key = malloc(256);

    strcpy(e.key, url);

    e.data = NULL;
    if (hsearch(e, ENTER) == NULL)
    {
        fprintf(stderr, "entry failed\n");
        exit(1);
    }
    duplicated_values[hash_counter] = e.key;
    strcpy(dp[hash_counter], url);
    hash_counter++;

    // Then push onto the frontier
    if (push(stack_thread_frontier, url))
    {
        printf("Error pushing value to stack\n");
        return 0;
    } // double checked and made sure, the stack can only accept m items (used for loop)

    for (int i = 0; i < t; ++i)
    {
        pthread_create(p_tids + i, NULL, curling, (void *)thread_struct);
    }

    // do one signal

    for (int i = 0; i < t; ++i)
    {

        pthread_join(p_tids[i], NULL);
    }

    // write to a png_urls here

    FILE *fp = NULL;
    fp = fopen("png_urls.txt", "wb+");
    for (int x = 0; x < m; x++)
    {
        char url[256];
        if (pop(stack_thread_png, url))
        {
            break;
        }
        if (url != NULL) fprintf(fp, "%s\n", url);
    }
    fclose(fp);
    // GET END TIME
    if (gettimeofday(&tv, NULL) != 0)
    {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec / 1000000.;

    fflush(stdout);
    printf("findpng2 execution time: %.6lf seconds\n", times[1] - times[0]);

    if (v != NULL)
    {
        fp = fopen(v, "wb+");
        for (int x = 0; x < hash_counter; x++)
        {
            // e.key = (char *)duplicated_values[x];
            // e.data = (void *)1;
            // ep = hsearch(e, FIND);
            fprintf(fp, "%s", dp[x]);
            free(duplicated_values[x]);
        }
        fclose(fp);
    }
    else
    {
        for (int x = 0; x < hash_counter; x++)
        {
            free(duplicated_values[x]);
        }
    }

    hdestroy();

    free(thread_struct);
    free(p_tids);

    destroy_stack(stack_thread_png);
    destroy_stack(stack_thread_frontier);

    pthread_mutex_destroy(&stack_mutex);
    pthread_mutex_destroy(&hashmap_mutex);
    pthread_mutex_destroy(&png_mutex);
    pthread_cond_destroy(&cond_v);

    xmlCleanupParser();

    return 0;
}
