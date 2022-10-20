module AppDynamics
  class ConfigError < Skylight::Core::ConfigError; end

  # TODO: Make Skylight::Core::Config raise different error classes depending on inheritor, e.g. AppDynamics::ConfigError
  class Config < Skylight::Core::Config
    def self.log_name; "AppDynamics" end
    def self.support_email; "help@appdynamics.com" end
    def self.env_matcher; /^(?:APPD)_(.+)$/ end
    def self.env_prefix; "APPD_" end

    def self.env_to_key
      @env_to_key ||= super.merge(
        'APP_NAME' => :app_name,
        'TIER_NAME' => :tier_name,
        'NODE_NAME' => :node_name,
        'NODEINDEX_PATH' => :nodeindex_path,
        'LOG_MAX_NUM_FILES' => :log_max_num_files,
        'LOG_MAX_FILE_SIZE' => :log_max_file_size,
        'CONTROLLER_HOST' => :'controller.host',
        'CONTROLLER_ACCOUNT' => :'controller.account',
        'CONTROLLER_ACCESS_KEY' => :'controller.access_key',
        'CONTROLLER_CERT_PATH' => :'controller.cert_path',
        'CONTROLLER_PORT' => :'controller.port',
        'CONTROLLER_USE_SSL' => :'controller.use_ssl',
        'CONTROLLER_HTTP_PROXY_HOST' => :'controller.http_proxy_host',
        'CONTROLLER_HTTP_PROXY_PORT' => :'controller.http_proxy_port',
        'CONTROLLER_HTTP_PROXY_USERNAME' => :'controller.http_proxy_username',
        'CONTROLLER_HTTP_PROXY_PASSWORD' => :'controller.http_proxy_password',
        'CONTROLLER_HTTP_PROXY_PASSWORD_FILE' => :'controller.http_proxy_password_file',
        'CONTROLLER_LOG_DIR' => :'controller.log_dir',
        'CONTROLLER_LOG_LEVEL' => :'controller.log_level',
        'CONTROLLER_LOG_MAX_NUM_FILES' => :'controller.log_max_num_files',
        'CONTROLLER_LOG_MAX_FILE_SIZE' => :'controller.log_max_file_size',
        'LAZY_START' => :lazy_start
      )
    end

    def self.required_keys
      @required_keys ||= super.merge(
        app_name: "app name",
        tier_name: "tier name",
        node_name: "node name",
        :'controller.host' => "controller host",
        :'controller.account' => "controller account",
        :'controller.access_key' => "controller access_key"
      )
    end

    def self.default_values
      @default_values ||= super.merge(
        :'controller.port' => 8080,
        :'controller.use_ssl' => false,
        :'controller.cert_path' => File.expand_path("data/cacert.pem", __dir__),
        init_timeout_ms: 0,
        lazy_start: true,
        business_transactions: BusinessTransactions.global_set,
        nodeindex_path: 'tmp/appdynamics/nodeindex',
        # Same defaults as the SDK
        log_max_num_files: 10,
        log_max_file_size: 5 * 1024 * 1024
      )
    end

    def nodeindex_path
      @nodeindex_path ||= File.expand_path(get(:nodeindex_path), root)
    end

    def node_index
      @node_index ||= get_node_index
    end

    def node_name
      @node_name ||= begin
        name = get(:node_name)
        node_index == 1 ? name : "#{name}-#{node_index}"
      end
    end

    def before_fork
      reset_node_index
    end

    def after_fork
      node_index # Force caching
    end

    def before_shutdown
      reset_node_index
    end

    def reset_node_index
      @node_index_pool&.exit
      @node_index = nil
      @node_name = nil
    end

    def controller_log_dir
      @controller_log_dir ||= get_controller_log_dir
    end

    def controller_log_level
      @controller_log_level ||= get_controller_log_level
    end

    def validate!
      super
      check_file_permissions(nodeindex_path, 'nodeindex_path')
      check_controller_log_dir_permissions
      true
    end

    def to_native_hash
      keys = %i(app_name tier_name node_name controller.host controller.port
          controller.account controller.access_key controller.use_ssl controller.cert_path
          controller.http_proxy_host controller.http_proxy_port controller.http_proxy_username
          controller.http_proxy_password controller.http_proxy_password_file
          controller.log_max_file_size controller.log_max_num_files
          init_timeout_ms)

      hash = Hash[*keys.map{|k| [k, send_or_get(k)]}.flatten]

      hash[:"controller.log_dir"] = controller_log_dir
      hash[:"controller.log_level"] = controller_log_level
      hash[:logger] = logger

      hash
    end

    private

      def create_logger(out)
        l = begin
          if out.is_a?(String)
            out = File.expand_path(out, root)
            # May be redundant since we also do this in the permissions check
            FileUtils.mkdir_p(File.dirname(out))
          end

          # NOTE: The max num files and max file size don't apply to non-file targets
          Logger.new(out, get(:log_max_num_files), get(:log_max_file_size))
        rescue
          Logger.new(STDOUT)
        end
        l.progname = self.class.log_name
        l
      end

      def get_node_index
        @node_index_pool ||= begin
          NodeIndexPool.new(nodeindex_path, config: self)
        end
        @node_index_pool.index
      end

      def get_controller_log_dir
        if (log_dir = get(:'controller.log_dir'))
          File.expand_path(log_dir, root)
        elsif (log_file = get(:log_file)) && log_file != '-'
          File.expand_path("./appdynamics", File.dirname(File.expand_path(log_file, root)))
        else
          # This is the default for the C SDK. We're duplicating that setting so we can do a permissions check.
          "/tmp/appd"
        end
      end

      # enum appd_config_log_level
      # {
      #   APPD_LOG_LEVEL_TRACE,
      #   APPD_LOG_LEVEL_DEBUG,
      #   APPD_LOG_LEVEL_INFO,
      #   APPD_LOG_LEVEL_WARN,
      #   APPD_LOG_LEVEL_ERROR,
      #   APPD_LOG_LEVEL_FATAL
      # };
      def get_controller_log_level
        if trace?
          0
        elsif (log_level = get(:'controller.log_level') || get(:log_level))
          # NOTE: `trace` wouldn't be an option for `log_level`, but would be
          #   for `controller.log_level`
          case log_level
          when /^trace$/i then 0
          when /^debug$/i then 1
          when /^info$/i  then 2
          when /^warn$/i  then 3
          when /^error$/i then 4
          when /^fatal$/i then 5
          end
        end
      end

      def dir_writable?(dir)
        if dir.exist?
          dir.writable?
        else
          return dir_writable?(dir.parent) unless dir.root?

          # Root dir doesn't exist, that's strange...
          false
        end
      end

      def check_controller_log_dir_permissions
        unless dir_writable?(Pathname.new(controller_log_dir))
          raise ConfigError, "Directory `#{controller_log_dir}` is not writable. Please set controller.log_dir in your config to a writable path"
        end
      end

  end
end
