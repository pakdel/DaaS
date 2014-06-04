#define SELECT_EXPR "json_agg(daas_result)"
#define ERROR_EXPR "[{\"status\": \"Error\"}]"

#define MAX_PARAMETER_COUNT 9
#define ACCESSTOKEN_COOKIE "Authorization"
// AUTHORIZATION_QUERY is the key of the query to be executed for authorization
// AUTHORIZATION_QUERY gets ACCESSTOKEN_COOKIE and the requested command (query key / prepared statement name) and returns username
#define AUTHORIZATION_QUERY "~"
#if defined(ACCESSTOKEN_COOKIE) && defined(AUTHORIZATION_QUERY)
#define REQUIRE_AUTH
#endif

#define EXIT_BAD_ARG 1
#define EXIT_FAILED_DAEMONIZE 1
#define EXIT_THREAD_CREATION_FAILED 3
#define EXIT_MHD_start_daemon_FAILED 4
#define EXIT_DB_CONNECTION_FAILED 5
#define EXIT_PQ_PREPARE_FAILED 6

#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <postgresql/libpq-fe.h>


struct query_list_el {
#ifdef REQUIRE_AUTH
    char * accesstoken;
#endif
    char * query;
    char * result;
    struct query_list_el * next;
};
typedef struct query_list_el query_item;
struct {
    pthread_cond_t  query;
    pthread_cond_t  result;
    pthread_mutex_t lock;
    query_item * head;
} query_list;


int exiting;
void sig_handler(int signum) {
    exiting = 1;
    pthread_cond_broadcast(&(query_list.query));
}


/**********************************************************************
 *
 *      void* query_thread
 *
 **********************************************************************/
void* query_thread(void *connectionString) {
    PGconn *conn = NULL;
    conn = PQconnectdb(connectionString);
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Connection to database failed!\n");
        PQfinish(conn);
        exit(EXIT_DB_CONNECTION_FAILED);
    }
    /*
     * Creating Prepared Statements
     */
    PGresult *res = NULL;
    res = PQexec(conn, "SELECT key, 'WITH daas_results AS ( ' || query || "
                             " CASE WHEN left(upper(ltrim(query)), 6) = 'SELECT'"
                             "   THEN ' ' "
                             "   ELSE ' RETURNING * ' "
                             " END"
                             " || ') SELECT " SELECT_EXPR
                       " FROM daas_results AS daas_result' FROM daas"
                       ";");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("Could not get list of queries\n\t%s\n", PQresultErrorMessage(res) );
        PQclear(res);
        PQfinish(conn);
        exit(EXIT_PQ_PREPARE_FAILED);
    }
    int queryNo;
    PGresult *prepareRes = NULL;
    for(queryNo=0; queryNo<PQntuples(res); queryNo++) {
        prepareRes = PQprepare(conn,  PQgetvalue(res, queryNo, 0), PQgetvalue(res, queryNo, 1), 1, NULL); // const Oid *paramTypes);
        if (PQresultStatus(prepareRes) != PGRES_COMMAND_OK) {
            PQfinish(conn);
            exit(EXIT_PQ_PREPARE_FAILED);
        }
    }
    PQclear(prepareRes);
    PQclear(res);

    /*
     * Processing queries
     */
    const char *paramValues[MAX_PARAMETER_COUNT];
#ifdef REQUIRE_AUTH
    const char *authParamValues[2];
#endif
    
    while(!exiting) {
        pthread_mutex_lock(&(query_list.lock));
        while (!query_list.head) {
          if(exiting) break;
          pthread_cond_wait(&(query_list.query), &(query_list.lock));
        }
        if(exiting) break;
        // Get the query in the head, advance the list and release the lock
        // FIXME If somethng goes wrong, are we going to loose this query?
        query_item * head = query_list.head;
        query_list.head = query_list.head->next;
        pthread_mutex_unlock(&(query_list.lock));
      
        // Do the SQL query
        // head->result might me pointing to strdup of the result, so malloc may or may not be needed
        // head->result = malloc(SQL_RESULT_BUF_SIZE+1);
        // Assuming query type (preparestatement name) is one character
        // /a/Parameter0
        // 0123
        // Moved the following after finding parameters
        // head->query[2]=0;
        paramValues[0] = &(head->query[3]);

/***************************************
 *      REQUIRE_AUTH
 ***************************************/
#ifdef REQUIRE_AUTH
        const char *authParamValues[2];
        authParamValues[0] = head->accesstoken;
        authParamValues[1] = &(head->query[1]);
        res = PQexecPrepared(conn,  // PGconn *conn,
                       AUTHORIZATION_QUERY,    // const char *stmtName,
                       2, // int nParams,
                       authParamValues,  // const char * const *paramValues, 
                       NULL,  // const int *paramLengths,
                       NULL,  // const int *paramFormats, /* all parameters are presumed to be text strings. */
                       0);    // int resultFormat);

        int authorized = 0;
        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res)==1) {
            authorized = strlen(PQgetvalue(res, 0, 0));
        }
        PQclear(res);
        if (!authorized) {
            head->result = strdup("[{\"status\": \"Not authorized\"}]");
            // Result is ready (actually, failure is ready)
            // Detach the head query_item: Already done
            // query_list.head = query_list.head->next;
            pthread_cond_broadcast(&(query_list.result));
            // pthread_mutex_unlock(&(query_list.lock));
            continue;
        }
#endif

/***************************************
 *      Actual Queries
 ***************************************/
        int nParams = 0;
        char *lastPtr = &(head->query[2]); //head->query+3;

        while(nParams < MAX_PARAMETER_COUNT && (lastPtr = strchr( lastPtr, '/' )) != NULL ) {
            *lastPtr = 0; // NULL terminate previous part
            paramValues[nParams++] = ++lastPtr;
        }
        head->query[2]=0;
        
        res = PQexecPrepared(conn,  // PGconn *conn,
                           &(head->query[1]),    // const char *stmtName,
                           nParams, // int nParams,
                           paramValues,  // const char * const *paramValues, 
                           NULL,  // const int *paramLengths,
                           NULL,  // const int *paramFormats, /* all parameters are presumed to be text strings. */
                           0);    // int resultFormat);

        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            head->result = strdup(ERROR_EXPR);
        } else if(PQntuples(res)<1) {
            head->result = strdup("{}");
        } else {
            head->result = strdup(PQgetvalue(res, 0, 0));
        }
        PQclear(res);

        // Result is ready
        // Detach the head query_item: Already done
        // query_list.head = query_list.head->next;
        pthread_cond_broadcast(&(query_list.result));
        // pthread_mutex_unlock(&(query_list.lock));
    }
    
    PQfinish(conn);
    pthread_exit(NULL);
}


/**********************************************************************
 *
 *      int con_handler
 *
 **********************************************************************/
static int con_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size,
            void ** ptr) {
    static int dummy;
  
    struct MHD_Response * response;
    int ret;

    // OPTIMIZATION FANATIC!
    // if (0 != strcmp(method, "GET")) return MHD_NO; /* unexpected method */
    if (*method != 'G' || *(method+1) != 'E' || *(method+2) != 'T' || *(method+3) != 0) return MHD_NO; /* unexpected method */
    if (&dummy != *ptr) {
        /* The first time only the headers are valid,
         * do not respond in the first round... */
        /* http://www.gnu.org/software/libmicrohttpd/manual/libmicrohttpd.html#microhttpd_002dpost */
        *ptr = &dummy;
        return MHD_YES;
    }
    if (0 != *upload_data_size) return MHD_NO; /* upload data in a GET!? */
  
#ifdef REQUIRE_AUTH
    const char * accesstoken = MHD_lookup_connection_value(connection, MHD_COOKIE_KIND, ACCESSTOKEN_COOKIE);
    if (!accesstoken) {
        return MHD_NO;
    }
#endif

    pthread_mutex_lock(&(query_list.lock));
    query_item * new_query = malloc(sizeof(query_item));

#ifdef REQUIRE_AUTH
    new_query->accesstoken = strdup(accesstoken);
#endif
    new_query->query = strdup(url);
    new_query->result = NULL;
    new_query->next = NULL;
    if(query_list.head) {
        query_item * last_query = query_list.head;
        while (last_query->next) {
            last_query = last_query->next;
        }
        last_query->next = new_query;
    } else {
        query_list.head = new_query;
    }

    pthread_cond_signal(&(query_list.query));

    // Waiting for the result
    while (!new_query->result) {
        pthread_cond_wait(&(query_list.result), &(query_list.lock));
    }
    pthread_mutex_unlock(&(query_list.lock)); // We do not need the lock anymore
  
    *ptr = NULL; /* clear context pointer */
    response = MHD_create_response_from_buffer(strlen(new_query->result),
                                               new_query->result,
                                               MHD_RESPMEM_MUST_FREE);
    free(new_query->accesstoken);
    free(new_query->query);
    /* free(new_query->result); // Passed it as the response creation function with MHD_RESPMEM_MUST_FREE Attr */
    free(new_query);
    
    /* Disable caching */
    MHD_add_response_header(response, "cache-control", "private, max-age=0, no-cache");
    
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

  return ret;
}


/**********************************************************************
 *
 *      int main
 *
 **********************************************************************/
int main(int argc, char ** argv) {
    struct MHD_Daemon * d;
    if (argc != 3) {
      printf("Usage:\n\t%s PORT CONNECTION_STRING\n", argv[0]);
      return EXIT_BAD_ARG;
    }
    char connectionString[512];
    snprintf(connectionString, 511, "%s", argv[2]);
    if (strlen(connectionString)>510) {
      printf("Connection String too long:\n\t%s\n", argv[2]);
      return EXIT_BAD_ARG;
    }
  
    exiting = 0;
    signal(SIGINT, sig_handler);
    if (close(0)) {
        return EXIT_FAILED_DAEMONIZE;
    }

    /* unbuffered STD OUT */
    setvbuf(stdout, NULL, _IONBF, 0);
    /* line buffered
     * setlinebuf(stdout)
     */

    pthread_mutex_init(&(query_list.lock), NULL);
    query_list.head = NULL;
    pthread_t tid;
    int err = pthread_create(&tid, NULL, &query_thread, connectionString);
    if (err != 0) {
        printf("\ncan't create thread: [%s]", strerror(err));
        // Maybe we can use a cleanup function
        pthread_mutex_destroy(&(query_list.lock));
        return EXIT_THREAD_CREATION_FAILED;
    }
  
    d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
                         atoi(argv[1]),
                         NULL,
                         NULL,
                         &con_handler,
                         NULL,
                         MHD_OPTION_END);
    if (d == NULL) return EXIT_MHD_start_daemon_FAILED;
  
    // Inform Query execution thread
    // exiting = 1;
    /* Wait for thread(s)
     */
    pthread_join(tid, NULL);
    
    pthread_cond_broadcast(&(query_list.query));
    pthread_mutex_destroy(&(query_list.lock));
    
    
    MHD_stop_daemon(d);
    
    return 0;
}
