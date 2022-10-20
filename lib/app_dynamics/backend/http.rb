module AppDynamics
  module Backend
    class HTTP < Base

      matches category: /^api\.http\./,
              meta: { host: Any }

      def backend_type
        "HTTP"
      end

      def backend_name
        meta[:host]
      end

      def identifying_properties
        { "HOST" => meta[:host] }
      end

    end
  end
end