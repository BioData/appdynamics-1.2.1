require "app_dynamics/version"
require "skylight/core"
require 'app_dynamics/instrumenter'
require 'app_dynamics/trace'
require 'app_dynamics/middleware'
require 'app_dynamics/sidekiq'

[Kernel, Kernel.singleton_class, Process.singleton_class].each do |target|
  next unless target.respond_to?(:fork)

  target.module_eval do
    alias fork_without_appd fork
    def fork
      if (instrumenter = AppDynamics.instrumenter)
        if instrumenter.fully_started?
          AppDynamics.log_error <<~MESSAGE

            Can't fork after agent has fully started. See documentation for more details.

          MESSAGE
          return false
        else
          AppDynamics.log_debug("Detected forking. Can safely continue since SDK has not yet initialized.")
        end
      end

      AppDynamics.before_fork

      if block_given?
        fork_without_appd do
          AppDynamics.after_fork
          yield
        end
      else
        ret = fork_without_appd
        if ret.nil?
          # We're in the child
          AppDynamics.after_fork
        end
        ret
      end
    end
  end
end

module AppDynamics
  include Skylight::Core::Instrumentable

  IGNORE = "__appd_ignore__".freeze

  def self.config_class
    Config
  end

  def self.instrumenter_class
    Instrumenter
  end

  def self.before_fork
    instrumenter&.before_fork
  end

  def self.after_fork
    instrumenter&.after_fork
  end

  # Returns the name of the HTTP correlation header
  # @return [String] header name
  def self.correlation_header
    @correlation_header ||= native_correlation_header
  end

  # Returns the list of business transactions defined in the active config.
  # @return [BusinessTransactions::TransactionSet, nil]
  def self.business_transactions
    return unless instrumenter
    instrumenter.config.get(:business_transactions)
  end

  # Check whether the current trace is snapshotting. See {Trace#is_snapshotting} for more details.
  def self.is_snapshotting
    instrumenter && instrumenter.is_snapshotting
  end

  # Add details to the current trace's snapshot. See {Trace#add_snapshot_details} for more details.
  def self.add_snapshot_details(details=nil, &block)
    return unless instrumenter
    instrumenter.add_snapshot_details(details, &block)
  end

  # Set the URL for the current trace's snapshot. See {Trace#set_snapshot_url} for more details.
  def self.set_snapshot_url(url)
    return unless instrumenter
    instrumenter.set_snapshot_url(url)
  end

end

# Require after loading everything that depends on native
require 'app_dynamics_native'
require 'app_dynamics/business_transactions'
require 'app_dynamics/backend'
require 'app_dynamics/config'
require 'app_dynamics/error_handler'
require 'app_dynamics/node_index_pool'

if defined?(Rails::Railtie)
  require 'app_dynamics/railtie'
end

require 'net/http'
AppDynamics.probe(:net_http)

# Load Redis probe if Redis is available
begin
  require 'redis'
  AppDynamics.probe(:redis)
rescue LoadError
end


