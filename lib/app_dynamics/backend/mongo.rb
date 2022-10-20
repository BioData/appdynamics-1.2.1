module AppDynamics
  module Backend
    class Mongo < Base

      matches category: /^db\.mongo\./,
              meta: { database: Any }

      def backend_type
        "DB"
      end

      def backend_name
        "Mongo #{meta[:database]}"
      end

      def identifying_properties
        { "VENDOR"   => "Mongo",
          "DATABASE" => meta[:database] }
      end

    end
  end
end