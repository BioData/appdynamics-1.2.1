module AppDynamics
  module Sidekiq
    def self.add_middleware(config)
      config.debug "Adding Sidekiq Middleware"

      ::Sidekiq.configure_server do |sidekiq_config|
        sidekiq_config.server_middleware do |chain|
          chain.add AppDynamics::Sidekiq::ServerMiddleware, config
        end
      end
    end

    class ServerMiddleware
      include Skylight::Core::Util::Logging

      def initialize(config)
        @config = config
      end

      def call(worker, job, queue)
        t { "Sidekiq middleware beginning trace" }
        job_class = job['wrapped'] || job["class"]
        AppDynamics.trace("#{job_class}#perform", 'app.sidekiq.worker', 'process') { yield }
      end
    end
  end
end
