require 'app_dynamics/background_metrics'
require 'securerandom'

module AppDynamics
  class Instrumenter < Skylight::Core::Instrumenter
    def self.new(config)
      config.validate!

      inst = if AppDynamics.native?
        native_new
      else
        allocate
      end

      inst.send(:initialize, SecureRandom.uuid, config)
      inst
    end

    def self.trace_class
      AppDynamics::Trace
    end

    attr_reader :pid

    def initialize(*)
      super
      @pid = Process.pid
      @fully_started = false
    end

    def start!
      return nil unless super
      finalize_start unless config.get(:lazy_start)
      self
    end

    def finalize_start
      return true if @fully_started

      # FIXME: This is to account for a mocked case during testing and should be improved.
      if self.class == AppDynamics::Instrumenter
        native_start_sdk
      end

      begin
        @background_metrics = BackgroundMetrics.new(self)
      rescue Exception => e
        log_error "failed to start runtime metric tracking; msg=%s", e.message
        t { e.backtrace.join("\n") }
      end

      @fully_started = true
    end

    def fully_started?
      @fully_started
    end

    def before_fork
      raise "Can't fork after fully started!" if fully_started?
      config.before_fork
    end

    def after_fork
      config.after_fork
      log_debug "Generated new index: #{config.node_index} after forking"
    end

    def shutdown
      @background_metrics.stop if @background_metrics
      config.before_shutdown
      super
    end

    def check_install!
      unless AppDynamics.native?
        config.alert_logger.error \
          "[AppDynamics] [#{AppDynamics::VERSION}] The AppDynamics C/C++ SDK was " \
          "not found on your system. No data will be reported! " \
          "To install the SDK, please follow the instructions at: " \
          "https://docs.appdynamics.com/pages/viewpage.action?pageId=42583435."

          # For non-standard locations:
          #   bundle config build.appdynamics --with-appdynamics-dir={INSTALL_PATH}"
          #   bundle exec gem pristine appdynamics
      end
    end

    # Check whether the current trace is snapshotting. See {Trace#is_snapshotting} for more details.
    def is_snapshotting
      current_trace && current_trace.is_snapshotting
    end

    # Add details to the current trace's snapshot. See {Trace#add_snapshot_details} for more details.
    def add_snapshot_details(details=nil, &block)
      return unless current_trace
      current_trace.add_snapshot_details(details, &block)
    end

    # Set the URL for the current trace's snapshot. See {Trace#set_snapshot_url} for more details.
    def set_snapshot_url(url)
      return unless current_trace
      current_trace.set_snapshot_url(url)
    end

    def process_sql(sql)
      AppDynamics.lex_sql(sql) if is_snapshotting
    rescue => e
      if config[:log_sql_parse_errors]
        config.logger.error "Failed to extract binds from SQL query. " \
                            "It's likely that this query uses more advanced syntax than we currently support. " \
                            "sql=#{sql.inspect}"
      end
      nil
    end
  end
end
