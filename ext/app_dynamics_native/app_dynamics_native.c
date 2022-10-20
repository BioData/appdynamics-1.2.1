#include <ruby.h>
#include <app_dynamics_native.h>

VALUE rb_mAppDynamics;

// Stub out the values
#ifndef HAVE_APPDYNAMICS_H
  static VALUE check_native(VALUE klass) { return Qfalse; }
  static VALUE get_correlation_header_name(VALUE klass) { return Qnil; }
  static VALUE lex_sql(VALUE klass) { return Qnil; }
  static VALUE instrumenter_new(VALUE klass, VALUE rb_config) { return Qnil; }
  static VALUE instrumenter_start(VALUE self) { return Qnil; }
  static VALUE instrumenter_start_sdk(VALUE self) { return Qnil; }
  static VALUE instrumenter_stop(VALUE self) { return Qnil; }
  static VALUE instrumenter_submit_trace(VALUE self, VALUE rb_trace) { return Qnil; }
  static VALUE instrumenter_track_desc(VALUE self, VALUE rb_endpoint, VALUE rb_desc) { return Qnil; }
  static VALUE trace_new(VALUE klass, VALUE start, VALUE uuid, VALUE endpoint, VALUE meta) { return Qnil; }
  static VALUE trace_get_started_at(VALUE self) { return Qnil; }
  static VALUE trace_get_endpoint(VALUE self) { return Qnil; }
  static VALUE trace_set_endpoint(VALUE self, VALUE endpoint) { return Qnil; }
  static VALUE trace_set_exception(VALUE self, VALUE exception) { return Qnil; }
  static VALUE trace_get_uuid(VALUE self) { return Qnil; }
  static VALUE trace_start_span(VALUE self, VALUE time, VALUE category) { return Qnil; }
  static VALUE trace_stop_span(VALUE self, VALUE span_id, VALUE time) { return Qnil; }
  static VALUE trace_span_get_category(VALUE self, VALUE span_id) { return Qnil; }
  static VALUE trace_span_set_title(VALUE self, VALUE span_id, VALUE title) { return Qnil; }
  static VALUE trace_span_get_title(VALUE self, VALUE span_id) { return Qnil; }
  static VALUE trace_span_set_description(VALUE self, VALUE span_id, VALUE desc) { return Qnil; }
  static VALUE trace_span_set_meta(VALUE self, VALUE span_id, VALUE meta) { return Qnil; }
  static VALUE trace_span_started(VALUE self, VALUE span_id) { return Qnil; }
  static VALUE trace_span_set_exception(VALUE self, VALUE span_id, VALUE exception, VALUE exception_details) { return Qnil; }
  static VALUE trace_span_get_correlation_header(VALUE self, VALUE span_id) { return Qnil; }
  static VALUE trace_is_snapshotting(VALUE self) { return Qnil; }
  static VALUE trace_add_snapshot_details(VALUE self, VALUE key, VALUE value) { return Qnil; }
  static VALUE trace_add_snapshot_url(VALUE self, VALUE url) { return Qnil; }
  static VALUE metrics_define(VALUE self, VALUE name) { return Qnil; }
  static VALUE metrics_report(VALUE self, VALUE name, VALUE value) { return Qnil; }
#endif

#ifdef HAVE_APPDYNAMICS_H
  #include <time.h>
  #include <stdio.h>
  #include <inttypes.h>

  #define TO_S(VAL) \
    RSTRING_PTR(rb_funcall(VAL, rb_intern("to_s"), 0))

  #define CHECK_TYPE(VAL, T)                        \
    do {                                            \
      if (TYPE(VAL) != T) {                         \
        rb_raise(rb_eArgError, "expected " #VAL " to be " #T " but was '%s' (%s [%i])", \
                  TO_S(VAL), rb_obj_classname(VAL), TYPE(VAL)); \
        return Qnil;                                \
      }                                             \
    } while(0)

  #define CHECK_NUMERIC(VAL)                        \
    do {                                            \
      if (TYPE(VAL) != T_BIGNUM &&                  \
          TYPE(VAL) != T_FIXNUM) {                  \
        rb_raise(rb_eArgError, "expected " #VAL " to be numeric but was '%s' (%s [%i])", \
                  TO_S(VAL), rb_obj_classname(VAL), TYPE(VAL)); \
        return Qnil;                                \
      }                                             \
    } while(0)                                      \


  #define My_Struct(name, Type, msg)                \
    Get_Struct(name, self, Type, msg);              \

  #define Transfer_My_Struct(name, Type, msg)       \
    My_Struct(name, Type, msg);                     \
    DATA_PTR(self) = NULL;                          \

  #define Transfer_Struct(name, obj, Type, msg)     \
    Get_Struct(name, obj, Type, msg);               \
    DATA_PTR(obj) = NULL;                           \

  #define Get_Struct(name, obj, Type, msg)          \
    Data_Get_Struct(obj, Type, name);               \
    if (name == NULL) {                             \
      rb_raise(rb_eRuntimeError, "%s", msg);        \
    }

  /**
   * Ruby GVL helpers
   */

  // FIXME: This conditional doesn't logically cover every case
  #if defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL) && \
      defined(HAVE_RUBY_THREAD_H)

  // Ruby 2.0+
  #include <ruby/thread.h>
  typedef void* (*blocking_fn_t)(void*);
  #define WITHOUT_GVL(fn, a) \
    rb_thread_call_without_gvl((blocking_fn_t)(fn), (a), 0, 0)

  // Ruby 1.9
  #elif defined(HAVE_RB_THREAD_BLOCKING_REGION)

  typedef VALUE (*blocking_fn_t)(void*);
  #define WITHOUT_GVL(fn, a) \
    rb_thread_blocking_region((blocking_fn_t)(fn), (a), 0, 0)


  #endif


  /**
   * Ruby types defined here
   */

  static const char* existing_instrumenter_msg =
  "Instrumenter is already running";

  static const char* no_instrumenter_msg =
  "Instrumenter not currently running";

  static const char* no_trace_msg =
  "Trace is not available";

  /*
   * Logging
   */

  #define LOG(target, method, fmt, ...)            \
    do {                                           \
      char* msg;                                   \
      int assigned = asprintf(&msg, fmt, ##__VA_ARGS__); \
      if (assigned > -1) { \
        rb_funcall(target, rb_intern(method), 1, rb_str_new_cstr(msg)); \
      } else {                                     \
        rb_raise(rb_eRuntimeError, "Unable to log from native"); \
      }                                            \
      free(msg);                                   \
    } while (0)

  #define TRACE(fmt, ...) \
    LOG(self, "log_trace", fmt, ##__VA_ARGS__);

  #define DEBUG(fmt, ...) \
    LOG(self, "log_debug", fmt, ##__VA_ARGS__);

  #define INFO(fmt, ...) \
    LOG(self, "log_info", fmt, ##__VA_ARGS__);

  #define WARN(fmt, ...) \
    LOG(self, "log_warn", fmt, ##__VA_ARGS__);

  #define ERROR(fmt, ...) \
    LOG(self, "log_error", fmt, ##__VA_ARGS__);


  // FIXME:
  //   - Add more type checking
  //   - Split up methods


  /*
  * Returns true if the native SDK is present.
  */
  static VALUE
  check_native(VALUE klass) {
    UNUSED(klass);
    return Qtrue;
  }

  /*
  * AppDynamics correlation header name
  * @return [String]
  */
  static VALUE
  get_correlation_header_name(VALUE klass) {
    UNUSED(klass);
    return rb_str_new_cstr(APPD_CORRELATION_HEADER_NAME);
  }

  /*
  * Lex SQL queries
  * @return [String, String]
  */
  static VALUE
  lex_sql(VALUE klass, VALUE sql) {
    #ifdef HAVE_LEX_SQL_UNKNOWN
      sql_lex_result_t result;

      UNUSED(klass);
      CHECK_TYPE(sql, T_STRING);

      result = lex_sql_unknown(StringValueCStr(sql));
      if (result.status == 1) {
        VALUE title = rb_str_new2(result.title);
        VALUE statement = rb_str_new2(result.statement);
        free_lex_result(result);
        return rb_ary_new3(2, title, statement);
      } else {
        VALUE exception = rb_exc_new_cstr(rb_eRuntimeError, result.error);
        free_lex_result(result);
        rb_exc_raise(exception);
        return Qnil;
      }
    #endif
    #ifndef HAVE_LEX_SQL_UNKNOWN
      return Qnil;
    #endif
  }

  /*
  *
  * class AppDynamics::Instrumenter
  *
  */

  static VALUE
  instrumenter_new(VALUE klass) {
    sky_instrumenter_t* instrumenter = malloc(sizeof(sky_instrumenter_t));

    instrumenter->config = NULL;
    instrumenter->started = false;
    instrumenter->num_backends = 0;

    return Data_Wrap_Struct(klass, NULL, free, instrumenter);
  }

  static void*
  instrumenter_start_nogvl(sky_instrumenter_t* instrumenter) {
    int initRC;

    initRC = appd_sdk_init(instrumenter->config);
    if (initRC) {
      return (void*) false;
    }

    sky_activate_memprof();

    instrumenter->started = true;

    return (void*) true;
  }

  static VALUE
  instrumenter_start_sdk(VALUE self) {
    sky_instrumenter_t* instrumenter;
    struct appd_config *config;

    VALUE rb_config;
    VALUE rb_config_hash;

    VALUE app_name;
    VALUE tier_name;
    VALUE node_name;
    VALUE controller_host;
    VALUE controller_port;
    VALUE controller_account;
    VALUE controller_access_key;
    VALUE controller_use_ssl;
    VALUE controller_cert_path;
    VALUE controller_http_proxy_host;
    VALUE controller_http_proxy_port;
    VALUE controller_http_proxy_username;
    VALUE controller_http_proxy_password;
    VALUE controller_http_proxy_password_file;
    VALUE controller_log_dir;
    VALUE controller_log_level;
    VALUE controller_log_max_num_files;
    VALUE controller_log_max_file_size;
    VALUE init_timeout_ms;
    VALUE logger;

    My_Struct(instrumenter, sky_instrumenter_t, no_instrumenter_msg);

    if (instrumenter->started) {
      rb_raise(rb_eRuntimeError, "%s", existing_instrumenter_msg);
    }

    // FIXME: Does the extra step of converting to a hash make sense here?
    rb_config = rb_iv_get(self, "@config");
    rb_config_hash = rb_funcall(rb_config, rb_intern("to_native_hash"), 0);

    logger = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("logger")));

    config = appd_config_init();

    // FIXME: This could definitely be more elegant, maybe make new class to wrap appd_config
    // FIXME: Add Windows proxy settings
    app_name = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("app_name")));
    CHECK_TYPE(app_name, T_STRING);
    LOG(logger, "info", "App Name: %s", StringValueCStr(app_name));

    tier_name = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("tier_name")));
    CHECK_TYPE(tier_name, T_STRING);
    LOG(logger, "info", "Tier Name: %s", StringValueCStr(tier_name));

    node_name = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("node_name")));
    CHECK_TYPE(node_name, T_STRING);
    LOG(logger, "info", "Node Name: %s", StringValueCStr(node_name));

    controller_host = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.host")));
    CHECK_TYPE(controller_host, T_STRING);

    controller_port = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.port")));
    CHECK_TYPE(controller_port, T_FIXNUM);

    controller_account = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.account")));
    CHECK_TYPE(controller_account, T_STRING);

    controller_access_key = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.access_key")));
    CHECK_TYPE(controller_access_key, T_STRING);

    controller_use_ssl = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.use_ssl")));

    controller_cert_path = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.cert_path")));

    controller_http_proxy_host = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.http_proxy_host")));
    controller_http_proxy_port = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.http_proxy_port")));
    controller_http_proxy_username = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.http_proxy_username")));
    controller_http_proxy_password = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.http_proxy_password")));
    controller_http_proxy_password_file = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.http_proxy_password_file")));

    controller_log_dir = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.log_dir")));
    controller_log_level = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.log_level")));
    controller_log_max_num_files = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.log_max_num_files")));
    controller_log_max_file_size = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("controller.log_max_file_size")));

    init_timeout_ms = rb_hash_aref(rb_config_hash, ID2SYM(rb_intern("init_timeout_ms")));
    CHECK_TYPE(init_timeout_ms, T_FIXNUM);

    appd_config_set_app_name(config, StringValueCStr(app_name));
    appd_config_set_tier_name(config, StringValueCStr(tier_name));
    appd_config_set_node_name(config, StringValueCStr(node_name));
    appd_config_set_controller_host(config, StringValueCStr(controller_host));
    appd_config_set_controller_port(config, FIX2UINT(controller_port));
    appd_config_set_controller_account(config, StringValueCStr(controller_account));
    appd_config_set_controller_access_key(config, StringValueCStr(controller_access_key));
    appd_config_set_controller_use_ssl(config, RTEST(controller_use_ssl) ? 1 : 0);
    appd_config_set_logging_log_dir(config, StringValueCStr(controller_log_dir));

    if (controller_log_level != Qnil) {
      appd_config_set_logging_min_level(config, FIX2UINT(controller_log_level));
    }

    if (controller_log_max_num_files != Qnil) {
      appd_config_set_logging_max_num_files(config, FIX2UINT(controller_log_max_num_files));
    }

    if (controller_log_max_file_size != Qnil) {
      appd_config_set_logging_max_file_size_bytes(config, FIX2UINT(controller_log_max_file_size));
    }

    appd_config_set_init_timeout_ms(config, FIX2UINT(init_timeout_ms));

    if (controller_cert_path != Qnil) {
      if (TYPE(controller_cert_path) == T_STRING) {
        LOG(logger, "info", "Setting cert path: %s", StringValueCStr(controller_cert_path));
        appd_config_set_controller_certificate_file(config, StringValueCStr(controller_cert_path));
      } else {
        LOG(logger, "warn", "Ignoring cert path, invalid type");
      }
    }

    if (controller_http_proxy_host != Qnil) {
      if (TYPE(controller_http_proxy_host) == T_STRING) {
        LOG(logger, "info", "Setting HTTP proxy host: %s", StringValueCStr(controller_http_proxy_host));
        appd_config_set_controller_http_proxy_host(config, StringValueCStr(controller_http_proxy_host));
      } else {
        LOG(logger, "warn", "Ignoring HTTP proxy host, invalid type");
      }
    }

    if (controller_http_proxy_port != Qnil) {
      if (TYPE(controller_http_proxy_port) == T_FIXNUM) {
        LOG(logger, "info", "Setting HTTP proxy port: %i", FIX2UINT(controller_http_proxy_port));
        appd_config_set_controller_http_proxy_port(config, FIX2UINT(controller_http_proxy_port));
      } else {
        LOG(logger, "warn", "Ignoring HTTP proxy port, invalid type");
      }
    }

    if (controller_http_proxy_username != Qnil) {
      if (TYPE(controller_http_proxy_username) == T_STRING) {
        LOG(logger, "info", "Setting HTTP proxy username: %s", StringValueCStr(controller_http_proxy_username));
        appd_config_set_controller_http_proxy_username(config, StringValueCStr(controller_http_proxy_username));
      } else {
        LOG(logger, "warn", "Ignoring HTTP proxy username, invalid type");
      }
    }

    if (controller_http_proxy_password != Qnil) {
      if (TYPE(controller_http_proxy_password) == T_STRING) {
        LOG(logger, "info", "Setting HTTP proxy password");
        appd_config_set_controller_http_proxy_password(config, StringValueCStr(controller_http_proxy_password));
      } else {
        LOG(logger, "warn", "Ignoring HTTP proxy password, invalid type");
      }
    }

    if (controller_http_proxy_password_file != Qnil) {
      if (TYPE(controller_http_proxy_password_file) == T_STRING) {
        LOG(logger, "info", "Setting HTTP proxy password file: %s", StringValueCStr(controller_http_proxy_password_file));
        appd_config_set_controller_http_proxy_password_file(config, StringValueCStr(controller_http_proxy_password_file));
      } else {
        LOG(logger, "warn", "Ignoring HTTP proxy password file, invalid type");
      }
    }

    instrumenter->config = config;

    if (WITHOUT_GVL(instrumenter_start_nogvl, instrumenter)) {
      INFO("Initialized AppDynamics SDK");
      instrumenter->started = true;
      return Qtrue;
    } else {
      ERROR("Unable to initialize AppDynamics SDK");
      return Qfalse;
    }
  }

  // TODO: Revist this since it doesn't do much
  static VALUE
  instrumenter_start(VALUE self) {
    sky_instrumenter_t* instrumenter;

    My_Struct(instrumenter, sky_instrumenter_t, no_instrumenter_msg);

    if (instrumenter->started) {
      rb_raise(rb_eRuntimeError, "%s", existing_instrumenter_msg);
    }

    return Qtrue;
  }

  static VALUE
  instrumenter_stop(VALUE self) {
    sky_instrumenter_t* instrumenter;

    My_Struct(instrumenter, sky_instrumenter_t, no_instrumenter_msg);

    if (instrumenter->started) {
      INFO("Terminating AppDynamics SDK");

      sky_deactivate_memprof();
      TRACE("appd_sdk_term before");
      appd_sdk_term();

      instrumenter->started = false;

      DEBUG("Terminated AppDynamics SDK");
    }

    return Qnil;
  }

  int sort_span_duration_desc(const void *a, const void *b) {
    sky_span_t *span_a = *(sky_span_t **)a;
    sky_span_t *span_b = *(sky_span_t **)b;
    long long a_dur = span_a->stop - span_a->start;
    long long b_dur = span_b->stop - span_b->start;
    return (int)(b_dur - a_dur);
  }

  static VALUE
  instrumenter_submit_trace(VALUE self, VALUE rb_trace) {
    sky_trace_t* trace;
    uint i = 0;

    Transfer_Struct(trace, rb_trace, sky_trace_t, no_trace_msg);


    if (sky_have_memprof()) {
      trace->allocations = sky_consume_allocations();
    }

    TRACE("start: %llu", trace->start);
    TRACE("endpoint: %s", trace->endpoint);
    if (sky_have_memprof()) {
      TRACE("allocations: %" PRIu64, trace->allocations);
    }
    TRACE("num_spans: %d", trace->num_spans);
    TRACE("spans:");

    // TODO: Only iterate here if tracing
    for (i=0; i < trace->num_spans; i++) {
      sky_span_t *span = trace->spans[i];
      TRACE("  %u:", i);
      TRACE("    is_exitcall: %i", span->is_exitcall);
      TRACE("    start: %llu", span->start);
      TRACE("    stop: %llu", span->stop);
      TRACE("    category: %s", span->category);
      TRACE("    title: %s", span->title ? span->title : "-");
      TRACE("    description: %s", span->description ? span->description : "-");
    }

    if (appd_bt_is_snapshotting(trace->handle)) {
      sky_span_t *gc_span = NULL;
      sky_span_t **sql_spans = malloc(trace->num_spans * sizeof(sky_span_t*));
      uint num_sql_spans = 0;

      TRACE("Logging additional snapshot details");

      // Set starting URL
      if (trace->request_url != NULL) {
        LOG(rb_mAppDynamics, "log_debug", "Setting URL: %s", trace->request_url);
        appd_bt_set_url(trace->handle, trace->request_url);
      }

      if (sky_have_memprof()) {
        char buffer[20]; // 64-bit number is 19 chars max
        sprintf(buffer, "%" PRIu64, trace->allocations);
        appd_bt_add_user_data(trace->handle, "Objects Allocated", buffer);
      }

      // Find GC and SQL spans
      for (i=0; i < trace->num_spans; i++) {
        sky_span_t *span = trace->spans[i];
        if (strcmp(span->category, "db.sql.query") == 0) {
          sql_spans[num_sql_spans++] = span;
        } else if (strcmp(span->category, "noise.gc") == 0) {
          gc_span = span;
        }
      }

      if (gc_span != NULL) {
        // Log GC details in snapshot
        char value[25]; // More than enough for duration
        float duration;
        duration = (float)(gc_span->stop - gc_span->start) / 10;
        sprintf(value, "%.2f ms", duration);
        TRACE("GC Time: %s", value);
        appd_bt_add_user_data(trace->handle, "GC Time", value);
      }

      // Sort SQL spans in reverse duration order
      qsort(sql_spans, num_sql_spans, sizeof(sky_span_t*), sort_span_duration_desc);

      // Log details about each SQL statement in snapshot
      for (i=0; i < num_sql_spans && i < 10; i++) {
        char key[25]; // Allows for 18 string chars and up to 7 numbers, more than enough
        char *value = NULL;
        float duration;
        int aspfRC;
        sky_span_t *span = sql_spans[i];

        sprintf(key, "Top SQL statement %i", i+1);

        duration = (float)(span->stop - span->start) / 10;
        aspfRC = asprintf(&value, "%.2f ms: %s", duration, span->description);
        if (aspfRC > -1) {
          TRACE("%s: %s", key, value);
          appd_bt_add_user_data(trace->handle, key, value);
        } else {
          ERROR("asprintf failed; unable to log slow SQL statement");
        }
        free(value);
      }

      free(sql_spans);
    } else {
      TRACE("Not snapshotting, won't log extra details");
    }

    TRACE("Ending BT; endpoint=%s; handle=%p", trace->endpoint, trace->handle);
    appd_bt_end(trace->handle);

    return Qnil;
  }

  static VALUE
  instrumenter_track_desc(VALUE self, VALUE rb_endpoint, VALUE rb_desc) {
    UNUSED(self);
    UNUSED(rb_endpoint);
    UNUSED(rb_desc);

    return Qtrue;
  }

  /*
  *
  * class AppDynamics::Trace
  *
  */

  void trace_mark(sky_trace_t *trace) {
    uint i;
    if (trace->spans != NULL) {
      for (i = 0; i < trace->num_spans; i++) {
        sky_span_t *span = trace->spans[i];
        // This is a Ruby object, so mark it to show that we care about it
        rb_gc_mark(span->meta);
      }
    }
  }

  void trace_free(sky_trace_t *trace) {
    uint i;

    free(trace->endpoint);
    free(trace->request_url);

    for (i = 0; i < trace->num_spans; i++) {
      sky_span_t *span = trace->spans[i];
      free(span->category);
      free(span->title);
      free(span->description);
      // No need to free meta since it is a Ruby object, but we do need to mark it
      free(span);
    }

    free(trace->spans);

    free(trace);
  }

  static VALUE
  trace_new(VALUE klass, VALUE start, VALUE uuid, VALUE endpoint, VALUE meta) {
    sky_trace_t *trace = malloc(sizeof(sky_trace_t));
    char *uuid_str = NULL;
    char *endpoint_str = NULL;
    char *correlation_header = NULL;
    char *request_url = NULL;
    char *tmp = NULL;
    bool ignored;

    UNUSED(klass);

    CHECK_NUMERIC(start);
    CHECK_TYPE(endpoint, T_STRING);

    if (meta != Qnil) {
      VALUE rb_correlation_header;
      VALUE rb_request_url;

      CHECK_TYPE(meta, T_HASH);

      // Could/should the uuid be used for the correlation header?
      rb_correlation_header = rb_hash_aref(meta, ID2SYM(rb_intern("correlation_header")));
      if (TYPE(rb_correlation_header) == T_STRING) {
        correlation_header = StringValueCStr(rb_correlation_header);
      }

      rb_request_url = rb_hash_aref(meta, ID2SYM(rb_intern("request_url")));
      if (TYPE(rb_request_url) == T_STRING) {
        tmp = StringValueCStr(rb_request_url);
        request_url = malloc(sizeof(char) * strlen(tmp) + 1);
        strcpy(request_url, tmp);
      }
    }

    uuid_str = StringValueCStr(uuid);
    endpoint_str = StringValueCStr(endpoint);
    ignored = (strcmp(endpoint_str, IGNORE) == 0);

    LOG(rb_mAppDynamics, "log_debug", "trace=%s; BT Begin: %s, %s, ignored=%d", uuid_str, endpoint_str, correlation_header, ignored);

    if (ignored) {
      trace->handle = NULL;
      trace->ignored = true;
    } else {
      trace->handle = appd_bt_begin(endpoint_str, correlation_header);
      trace->ignored = false;
      LOG(rb_mAppDynamics, "log_trace", "trace=%s; BT handle=%p", uuid_str, trace->handle);
      if (trace->handle == NULL) {
        LOG(rb_mAppDynamics, "log_warn", "trace=%s; Unable to obtain a BT handle, request will not be tracked.", uuid_str);
      }
    }

    trace->start = NUM2ULL(start);

    tmp = StringValueCStr(endpoint);
    trace->endpoint = malloc(sizeof(char) * strlen(tmp) + 1);
    strcpy(trace->endpoint, tmp);

    trace->request_url = request_url;

    // Reset allocations counter
    if (sky_have_memprof()) {
      sky_consume_allocations();
    }

    trace->num_spans = 0;
    trace->spans = NULL;

    return Data_Wrap_Struct(klass, trace_mark, trace_free, trace);
  }

  static VALUE
  trace_get_started_at(VALUE self) {
    sky_trace_t* trace;

    My_Struct(trace, sky_trace_t, no_trace_msg);

    return ULL2NUM(trace->start);
  }

  static VALUE
  trace_get_endpoint(VALUE self) {
    sky_trace_t* trace;

    My_Struct(trace, sky_trace_t, no_trace_msg);

    return rb_str_new_cstr(trace->endpoint);
  }

  // AppDynamics doesn't allow us to set after the fact
  static VALUE
  trace_set_endpoint(VALUE self, VALUE endpoint) {
    return Qnil;
  }

  static VALUE
  trace_set_exception(VALUE self, VALUE exception) {
    sky_trace_t* trace;
    VALUE exception_str;
    char* error_name;
    bool mark_bt_as_error;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    exception_str = rb_funcall(exception, rb_intern("to_s"), 0);
    error_name = StringValueCStr(exception_str);
    mark_bt_as_error = true;

    DEBUG("Adding error: %s", error_name);
    appd_bt_add_error(trace->handle, APPD_LEVEL_ERROR, error_name, mark_bt_as_error);

    return Qnil;
  }

  static VALUE
  trace_get_uuid(VALUE self) {
    return Qnil;
  }

  static VALUE
  trace_start_span(VALUE self, VALUE time, VALUE category) {
    sky_trace_t* trace;
    sky_span_t* span = malloc(sizeof(sky_span_t));

    uint span_id;
    char* tmp;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_NUMERIC(time);
    CHECK_TYPE(category, T_STRING);

    span->is_exitcall = false;
    span->start = NUM2ULL(time);

    tmp = StringValueCStr(category);
    span->category = malloc(sizeof(char) * strlen(tmp) + 1);
    strcpy(span->category, tmp);

    span->title = NULL;
    span->description = NULL;
    span->meta = Qnil;

    span_id = trace->num_spans;
    if (span_id % SPAN_BATCH == 0) {
      if (span_id >= MAX_SPANS) {
        rb_raise(rb_eRuntimeError, "Exceeded maximum number of spans (%d)", MAX_SPANS);
      }
      TRACE("Growing spans size to %i", span_id+SPAN_BATCH);
      trace->spans = realloc(trace->spans, (span_id+SPAN_BATCH) * sizeof(sky_span_t*));
    }
    trace->spans[span_id] = span;
    trace->num_spans++;

    return UINT2NUM(span_id);
  }

  static VALUE
  trace_stop_span(VALUE self, VALUE span_id, VALUE time) {
    sky_trace_t* trace;
    sky_span_t* span;

    UNUSED(self);

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_TYPE(span_id, T_FIXNUM);
    CHECK_NUMERIC(time);

    // In theory we should check that the spans are closed in order
    span = trace->spans[FIX2UINT(span_id)];
    span->stop = NUM2ULL(time);

    if (span->is_exitcall) {
      char *title;
      int rc;

      title = span->title ? span->title : span->category;
      DEBUG("Setting exitcall title: %s", title);

      rc = appd_exitcall_set_details(span->handle, title);
      if (rc) {
        ERROR("Failed to set exitcall details; %s; err=%i", title, rc);
      }

      appd_exitcall_end(span->handle);
    }

    return Qnil;
  }

  static VALUE
  trace_span_get_category(VALUE self, VALUE span_id) {
    sky_trace_t* trace;
    sky_span_t* span;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_TYPE(span_id, T_FIXNUM);

    span = trace->spans[FIX2UINT(span_id)];
    return rb_str_new_cstr(span->category);
  }

  static VALUE
  trace_span_set_title(VALUE self, VALUE span_id, VALUE title) {
    sky_trace_t* trace;
    sky_span_t* span;
    char *tmp;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_TYPE(span_id, T_FIXNUM);
    CHECK_TYPE(title, T_STRING);

    span = trace->spans[FIX2UINT(span_id)];
    tmp = StringValueCStr(title);
    span->title = malloc(sizeof(char) * strlen(tmp) + 1);
    strcpy(span->title, tmp);

    return Qnil;
  }

  static VALUE
  trace_span_get_title(VALUE self, VALUE span_id) {
    sky_trace_t* trace;
    sky_span_t* span;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_TYPE(span_id, T_FIXNUM);

    span = trace->spans[FIX2UINT(span_id)];
    return span->title ? rb_str_new_cstr(span->title) : Qnil;
  }

  static VALUE
  trace_span_set_description(VALUE self, VALUE span_id, VALUE desc) {
    sky_trace_t* trace;
    sky_span_t* span;
    char *tmp;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_TYPE(span_id, T_FIXNUM);
    CHECK_TYPE(desc, T_STRING);

    span = trace->spans[FIX2UINT(span_id)];
    tmp = StringValueCStr(desc);
    span->description = malloc(sizeof(char) * strlen(tmp) + 1);
    strcpy(span->description, tmp);

    return Qnil;
  }

  static VALUE
  trace_span_set_meta(VALUE self, VALUE span_id, VALUE meta) {
    sky_trace_t* trace;
    sky_span_t* span;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_TYPE(span_id, T_FIXNUM);
    CHECK_TYPE(meta, T_HASH);

    span = trace->spans[FIX2UINT(span_id)];

    span->meta = meta;

    return Qnil;
  }

  static VALUE
  trace_span_started(VALUE self, VALUE span_id) {
    sky_trace_t* trace;
    sky_span_t* span;
    VALUE rb_mAppDynamics;
    VALUE rb_mBackend;
    VALUE rb_iBackendSub;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    CHECK_TYPE(span_id, T_FIXNUM);

    span = trace->spans[FIX2UINT(span_id)];

    rb_mAppDynamics = rb_define_module("AppDynamics");
    rb_mBackend = rb_define_module_under(rb_mAppDynamics, "Backend");
    rb_iBackendSub = rb_funcall(rb_mBackend, rb_intern("build"), 4,
                                              rb_str_new_cstr(span->category),
                                              span->title ? rb_str_new_cstr(span->title) : Qnil,
                                              span->description ? rb_str_new_cstr(span->description) : Qnil,
                                              span->meta);

    if (rb_iBackendSub != Qnil) {
      VALUE rb_oInstrumenter;
      sky_instrumenter_t* instrumenter;
      VALUE rb_backend_name;
      char* backend_name;
      bool has_backend;
      uint i;
      int rc;

      DEBUG("Attempting to treat as an exit call");

      // TODO: We can streamline this by maintining references in the C-code
      rb_oInstrumenter = rb_iv_get(self, "@instrumenter");
      Get_Struct(instrumenter, rb_oInstrumenter, sky_instrumenter_t, "missing native instrumenter");

      // FIXME: Maybe this should include the type too
      rb_backend_name = rb_funcall(rb_iBackendSub, rb_intern("backend_name"), 0);
      backend_name = StringValueCStr(rb_backend_name);

      has_backend = false;
      for (i = 0; i < instrumenter->num_backends; i++) {
        if (strcmp(instrumenter->backends[i], backend_name) == 0) {
          has_backend = true;
          break;
        }
      }

      if (has_backend) {
        TRACE("Backend already added: %s", backend_name);
      } else {
        int backend_idx;
        VALUE backend_type;
        VALUE identifying_properties;
        uint ipc;

        DEBUG("Adding backend: %s", backend_name);

        backend_type = rb_funcall(rb_iBackendSub, rb_intern("backend_type"), 0);

        appd_backend_declare(StringValueCStr(backend_type), backend_name);

        identifying_properties = rb_funcall(rb_iBackendSub, rb_intern("identifying_properties_array"), 0);

        ipc = (int) RARRAY_LEN(identifying_properties);
        for (i = 0; i < ipc; i += 2) {
          VALUE rb_key = rb_ary_entry(identifying_properties, i);
          VALUE rb_val = rb_ary_entry(identifying_properties, i+1);

          char *key = StringValueCStr(rb_key);
          char *val = StringValueCStr(rb_val);

          DEBUG("Identifying: %s - %s\n", key, val);
          rc = appd_backend_set_identifying_property(backend_name, key, val);
          if (rc) {
            ERROR("Unable to set identifying property for backend: %s; key=%s; val=%s; err=%d.", backend_name, key, val, rc);
          }
        }

        backend_idx = instrumenter->num_backends++;
        strcpy(instrumenter->backends[backend_idx], backend_name);
      }

      rc = appd_backend_add(backend_name);
      if (rc) {
        ERROR("Unable to add backend: %s; err=%d.", backend_name, rc);
      }

      TRACE("Starting exit call; endpoint=%s; backend=%s", trace->endpoint, backend_name);
      span->is_exitcall = true;
      span->handle = appd_exitcall_begin(trace->handle, backend_name);
      TRACE("exit call handle=%p", span->handle);
    }

    return Qnil;
  }

  static VALUE
  trace_span_set_exception(VALUE self, VALUE span_id, VALUE exception, VALUE exception_details) {
    sky_trace_t* trace;
    sky_span_t* span;
    VALUE exception_str;
    char* error_name;
    bool mark_bt_as_error;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    span = trace->spans[FIX2UINT(span_id)];

    if (span->is_exitcall) {
      if (exception != Qnil) {
        exception_str = rb_funcall(exception, rb_intern("to_s"), 0);
      } else {
        exception_str = rb_funcall(exception_details, rb_intern("join"), rb_str_new_cstr(", "));
      }

      error_name = StringValueCStr(exception_str);

      // We'll report the bt error separately
      mark_bt_as_error = false;

      DEBUG("Adding span error: %s\n", error_name);
      appd_exitcall_add_error(span->handle, APPD_LEVEL_ERROR, error_name, mark_bt_as_error);
    }

    return Qnil;
  }

  static VALUE
  trace_span_get_correlation_header(VALUE self, VALUE span_id) {
    sky_trace_t* trace;
    sky_span_t* span;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    span = trace->spans[FIX2UINT(span_id)];

    if (span->is_exitcall) {
      return rb_str_new_cstr(appd_exitcall_get_correlation_header(span->handle));
    } else {
      return Qnil;
    }
  }

  static VALUE
  trace_is_snapshotting(VALUE self) {
    sky_trace_t* trace;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qfalse; }

    if (appd_bt_is_snapshotting(trace->handle)) {
        return Qtrue;
    } else {
        return Qtrue;
    }
  }

  static VALUE
  trace_add_snapshot_details(VALUE self, VALUE key, VALUE value) {
    sky_trace_t* trace;
    char* c_key;
    char* c_value;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    c_key = StringValueCStr(key);
    c_value = StringValueCStr(value);

    appd_bt_add_user_data(trace->handle, c_key, c_value);

    return Qnil;
  }

  static VALUE
  trace_add_snapshot_url(VALUE self, VALUE url) {
    sky_trace_t* trace;
    char* c_url;

    My_Struct(trace, sky_trace_t, no_trace_msg);
    if (trace->ignored) { return Qnil; }

    c_url = StringValueCStr(url);

    appd_bt_set_url(trace->handle, c_url);

    return Qnil;
  }

  // FIXME: Remove duplication in next two methods
  static VALUE
  metrics_define(VALUE self, VALUE name) {
    char* metric_name;
    int rc = asprintf(&metric_name, "Custom Metrics|Ruby|%s", StringValueCStr(name));
    if (rc > -1) {
      TRACE("Registering metric: %s", metric_name)
      appd_custom_metric_add(NULL, metric_name, APPD_TIMEROLLUP_TYPE_AVERAGE,
                                                APPD_CLUSTERROLLUP_TYPE_INDIVIDUAL,
                                                APPD_HOLEHANDLING_TYPE_RATE_COUNTER);
    } else {
      ERROR("Unable to allocate metric_name");
    }
    free(metric_name);
    return Qnil;
  }

  static VALUE
  metrics_report(VALUE self, VALUE name, VALUE value) {
    char* metric_name;
    int rc = asprintf(&metric_name, "Custom Metrics|Ruby|%s", StringValueCStr(name));
    if (rc > -1) {
      unsigned long c_value = NUM2ULONG(value);
      TRACE("Reporting metric: %s: %lu", metric_name, c_value);
      appd_custom_metric_report(NULL, metric_name, c_value);
    } else {
      ERROR("Unable to allocate metric_name");
    }
    free(metric_name);
    return Qnil;
  }
#endif

void Init_app_dynamics_native() {
  VALUE rb_cTrace;
  VALUE rb_cInstrumenter;
  VALUE rb_cBackgroundMetrics;

  rb_mAppDynamics = rb_define_module("AppDynamics");
  // AppDynamics custom methods
  rb_define_singleton_method(rb_mAppDynamics, "native?", check_native, 0);
  rb_define_singleton_method(rb_mAppDynamics, "native_correlation_header", get_correlation_header_name, 0);
  rb_define_singleton_method(rb_mAppDynamics, "lex_sql", lex_sql, 1);

  rb_cTrace = rb_const_get(rb_mAppDynamics, rb_intern("Trace"));
  // Skylight required methods
  rb_define_singleton_method(rb_cTrace, "native_new", trace_new, 4);
  rb_define_method(rb_cTrace, "native_get_started_at", trace_get_started_at, 0);
  rb_define_method(rb_cTrace, "native_get_endpoint", trace_get_endpoint, 0);
  rb_define_method(rb_cTrace, "native_set_endpoint", trace_set_endpoint, 1);
  rb_define_method(rb_cTrace, "native_set_exception", trace_set_exception, 1);
  rb_define_method(rb_cTrace, "native_get_uuid", trace_get_uuid, 0);
  rb_define_method(rb_cTrace, "native_start_span", trace_start_span, 2);
  rb_define_method(rb_cTrace, "native_stop_span", trace_stop_span, 2);
  rb_define_method(rb_cTrace, "native_span_get_category", trace_span_get_category, 1);
  rb_define_method(rb_cTrace, "native_span_set_title", trace_span_set_title, 2);
  rb_define_method(rb_cTrace, "native_span_get_title", trace_span_get_title, 1);
  rb_define_method(rb_cTrace, "native_span_set_description", trace_span_set_description, 2);
  rb_define_method(rb_cTrace, "native_span_set_meta", trace_span_set_meta, 2);
  rb_define_method(rb_cTrace, "native_span_started", trace_span_started, 1);
  rb_define_method(rb_cTrace, "native_span_set_exception", trace_span_set_exception, 3);
  rb_define_method(rb_cTrace, "native_span_get_correlation_header", trace_span_get_correlation_header, 1);

  // AppDynamics custom methods
  rb_define_method(rb_cTrace, "native_is_snapshotting", trace_is_snapshotting, 0);
  rb_define_method(rb_cTrace, "native_add_snapshot_details", trace_add_snapshot_details, 2);
  rb_define_method(rb_cTrace, "native_set_snapshot_url", trace_add_snapshot_url, 1);

  rb_cInstrumenter = rb_const_get(rb_mAppDynamics, rb_intern("Instrumenter"));
  // Skylight required methods
  rb_define_singleton_method(rb_cInstrumenter, "native_new", instrumenter_new, 0);
  rb_define_method(rb_cInstrumenter, "native_start", instrumenter_start, 0);
  rb_define_method(rb_cInstrumenter, "native_start_sdk", instrumenter_start_sdk, 0);
  rb_define_method(rb_cInstrumenter, "native_stop", instrumenter_stop, 0);
  rb_define_method(rb_cInstrumenter, "native_submit_trace", instrumenter_submit_trace, 1);
  rb_define_method(rb_cInstrumenter, "native_track_desc", instrumenter_track_desc, 2);

  // AppDynamics custom methods
  rb_cBackgroundMetrics = rb_const_get(rb_mAppDynamics, rb_intern("BackgroundMetrics"));
  rb_define_method(rb_cBackgroundMetrics, "native_define_metric", metrics_define, 1);
  rb_define_method(rb_cBackgroundMetrics, "native_report_metric", metrics_report, 2);
}