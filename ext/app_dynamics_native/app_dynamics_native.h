#ifndef __APP_DYNAMICS_NATIVE__
#define __APP_DYNAMICS_NATIVE__

#define UNUSED(x) (void)(x)

  #include <extconf.h>

  #ifdef HAVE_APPDYNAMICS_H
    #include <stdint.h>
    #include <stdbool.h>
    #include <appdynamics.h>
    #include <skylight_memprof.h>

    #define MAX_BACKENDS 50
    #define BACKEND_LENGTH 256

    #define MAX_SPANS 10000
    #define SPAN_BATCH 100

    #define IGNORE "__appd_ignore__"

    typedef struct {
      struct appd_config *config;
      bool started;
      uint num_backends;
      char backends[MAX_BACKENDS][BACKEND_LENGTH];
    } sky_instrumenter_t;

    typedef struct {
      bool is_exitcall;
      appd_exitcall_handle handle;
      unsigned long long start; // Decisecond
      unsigned long long stop; // Decisecond
      char* category;
      char* title;
      char* description;
      VALUE meta;
    } sky_span_t;

    typedef struct {
      appd_bt_handle handle;
      bool ignored;
      unsigned long long start; // Decisecond
      char* endpoint;
      char* request_url;
      uint64_t allocations;
      uint num_spans;
      sky_span_t **spans;
    } sky_trace_t;
  #endif

  #ifdef HAVE_LEX_SQL_UNKNOWN
    // libsqllexer has no headers of its own

    typedef struct {
      int status;
      char* title;
      char* statement;
      char* error;
    } sql_lex_result_t;

    sql_lex_result_t lex_sql_unknown(char*);
    void* free_lex_result(sql_lex_result_t);
  #endif
#endif
