require 'skylight/core/railtie'

module AppDynamics
  # Railtie to ease integration with Rails applications.
  class Railtie < Rails::Railtie
    include Skylight::Core::Railtie

    def self.root_key; :appdynamics end
    def self.config_class; AppDynamics::Config end
    def self.middleware_class; AppDynamics::Middleware end
    def self.gem_name; 'AppDynamics' end
    def self.namespace; AppDynamics end
    def self.log_file_name; 'appdynamics' end
    def self.version; AppDynamics::VERSION end

    config.appdynamics = ActiveSupport::OrderedOptions.new

    # The environments in which skylight should be enabled
    config.appdynamics.environments = ['production']

    # The path to the configuration file
    config.appdynamics.config_path = "config/appdynamics.yml"

    # The probes to load
    #   net_http, action_controller, action_dispatch, action_view, and middleware are on by default
    #   See https://www.skylight.io/support/getting-more-from-skylight#available-instrumentation-options
    #   for a full list.
    config.appdynamics.probes = ['net_http', 'action_controller', 'action_dispatch', 'action_view', 'middleware']

    # The position in the middleware stack to place Skylight
    # Default is first, but can be `{ after: Middleware::Name }` or `{ before: Middleware::Name }`
    config.appdynamics.middleware_position = 0

    initializer 'appdynamics.configure_instrumentation' do |app|
      run_initializer(app)
    end

    initializer "appdynamics.add_error_handling" do |app|
      if AppDynamics.started?
        begin
          # FIXME: This assumes that Skylight::Core::Middleware will be before DebugExceptions, which should be true,
          #   but isn't guaranteed to be.
          app.config.middleware.insert_after ActionDispatch::DebugExceptions, AppDynamics::ErrorHandler
        rescue
          app.config.middleware.insert_after Skylight::Core::Middleware, AppDynamics::ErrorHandler
        end
      end
    end

    private

      def load_skylight_config(app)
        config = super

        # Hackily detect and name Sidekiq, there may be a more graceful way to handle this
        if defined?(::Sidekiq) && ::Sidekiq.server?
          config[:tier_name] += " - Sidekiq"
        end

        config
      end

      def set_middleware_position(app, config)
        super

        if defined?(::Sidekiq)
          AppDynamics::Sidekiq.add_middleware(config)
        end
      end

      def log_prefix
        "[AppDynamics] [#{AppDynamics::VERSION}]"
      end

  end
end
