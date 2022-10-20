module AppDynamics
  # Rack Middleware for tracking unhandled exceptions
  class ErrorHandler

    def initialize(app)
      @app = app
    end

    def call(env)
      @app.call(env)
    rescue Exception => e
      track_exception(e)
      raise e
    end

    private

      def track_exception(e)
        return unless instance = AppDynamics.instrumenter
        return unless trace = instance.current_trace
        trace.native_set_exception(e)
      end

  end
end
