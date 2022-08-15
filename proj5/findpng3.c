#include "lab_png.h"

// global

int image_counter = 0;

int hash_counter = 0;
int curl_counter = 0;
int m = 50;
int t = 1;

// ENTRY **hash_pointers;
char *duplicated_values[1000];
RECV_BUF *duplicated_curl_malloc[1000];

char dp[1000][400];

void curling(void *argument)
{

    thread_arguments *args = argument;

    CURLM *cm = NULL;
    CURLMsg *msg = NULL;
    CURLcode return_code = 0;
    int still_running = 0, msgs_left = 0;
    int http_status_code;
    curl_global_init(CURL_GLOBAL_ALL);
    cm = curl_multi_init();
    while (1)
    {
        for (int x = 0; x < t; x++)
        {
            // init(cm, x);
            char url[400];
            if (pop(args->frontier, url) == 0)
            {

                RECV_BUF *new_data = malloc(sizeof(RECV_BUF));

                // printf(" the pointer is %p and %p\n", new_data, duplicated_curl_malloc[curl_counter]);

                CURL *eh = easy_handle_init(new_data, url); // PROBLEM

                // printf("url is %s and %d\n", url, x);
                curl_easy_setopt(eh, CURLOPT_PRIVATE, new_data);

                curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
                curl_easy_setopt(eh, CURLOPT_URL, url);
                curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);

                curl_multi_add_handle(cm, eh);
                duplicated_curl_malloc[curl_counter] = new_data;
                curl_counter++;
            }
        }

        curl_multi_perform(cm, &still_running); // PROBLEM

        do
        {

            int numfds = 0;
            int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
            if (res != CURLM_OK)
            {
                fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
                continue;
            }
            /*
             if(!numfds) {
                fprintf(stderr, "error: curl_multi_wait() numfds=%d\n", numfds);
                return EXIT_FAILURE;
             }
            */
            curl_multi_perform(cm, &still_running); // PROBLEM

        } while (still_running);

        /* get it! */
        // printf("why\n");

        /* process the download data */
        // process_data(curl_handle, &recv_buf, args->frontier, args->png);

        while ((msg = curl_multi_info_read(cm, &msgs_left)))
        {

            if (msg->msg == CURLMSG_DONE)
            {
                CURL *eh = msg->easy_handle;

                return_code = msg->data.result;
                if (return_code != CURLE_OK)
                {
                    // fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                    curl_multi_remove_handle(cm, eh);
                    curl_easy_cleanup(eh);
                    continue;
                }

                // Get HTTP status code
                http_status_code = 0;

                RECV_BUF *recv_buf;

                curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
                curl_easy_getinfo(eh, CURLINFO_PRIVATE, &recv_buf);

                process_data(eh, recv_buf, args->frontier, args->png); // PROBLEM

                curl_multi_remove_handle(cm, eh);
                curl_easy_cleanup(eh);
            }
            else
            {
                fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
            }
        }

        if (image_counter >= m || is_empty(args->frontier))
        {
            break;
        }
    }
    curl_multi_cleanup(cm);
    // curl_global_cleanup();
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

    if (optind >= argc)
    {
        fprintf(stderr, "Expected url argument after options\n");
        return -1;
    }
    strcpy(url, argv[optind]);

    // printf("url is %s , m = %d , t= %d, v=%s\n", url, m, t, v);

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
    times[0] = (tv.tv_sec) + tv.tv_usec / 1000000;

    // INIT ALL CONNECTIONS

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

    curling(thread_struct);

    FILE *fp = NULL;
    fp = fopen("png_urls.txt", "wb+");
    for (int x = 0; x < m; x++)
    {
        char url[256];
        if (pop(stack_thread_png, url))
        {
            break;
        }
        if (url != NULL)
            fprintf(fp, "%s\n", url);
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
    printf("findpng3 execution time: %.6lf seconds\n", times[1] - times[0]);

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

    for (int x = 0; x < curl_counter; x++)
    {
        // printf(" the pointer is %p\n", duplicated_curl_malloc[x]);
        recv_buf_cleanup(duplicated_curl_malloc[x]);

        free(duplicated_curl_malloc[x]);
    }

    hdestroy();
    curl_global_cleanup();
    free(thread_struct);

    destroy_stack(stack_thread_png);
    destroy_stack(stack_thread_frontier);

    xmlCleanupParser();

    return 0;
}
