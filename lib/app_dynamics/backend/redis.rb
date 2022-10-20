

module AppDynamics
  module Backend
    class Redis < Base

      matches category: /^db\.redis\./

      def backend_type
        "CACHE"
      end

      def backend_name
        "Redis"
      end

      def identifying_properties
        { "VENDOR"   => "Redis" }
      end

    end
  end
end