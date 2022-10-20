require 'app_dynamics'

module AppDynamics
  module Probes
    module Sinatra
      class Probe
        def install
          class << ::Sinatra::Base

            alias build_without_appd build

            def build(*args, &block)
              use AppDynamics::Middleware

              config = respond_to?(:appdynamics_config) ? appdynamics_config : nil
              AppDynamics.start!(config)

              build_without_appd(*args, &block)
            end
          end
        end
      end
    end

    Skylight::Core::Probes.register(:sinatra, "Sinatra::Base", "sinatra/base", Sinatra::Probe.new)
  end
end
