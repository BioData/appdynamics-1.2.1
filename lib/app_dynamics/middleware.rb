module AppDynamics
  # Primary Middleware for tracking requests in AppDynamics
  class Middleware < Skylight::Core::Middleware

    def instrumentable
      AppDynamics
    end

    def endpoint_name(env)
      transactions = AppDynamics.business_transactions
      bt = transactions ? transactions.match(env) : nil
      bt || IGNORE
    end

    def endpoint_meta(env)
      meta = {}

      if header = AppDynamics.correlation_header
        meta[:correlation_header] = env["HTTP_#{header.upcase}"]
      end

      # Maybe only do if snapshotting? Though not that expensive
      req = Rack::Request.new(env)
      meta[:request_url] = req.url

      meta
    end

  end
end
