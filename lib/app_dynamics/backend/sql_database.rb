module AppDynamics
  module Backend
    class SQLDatabase < Base

      matches category: 'db.sql.query',
              meta: { adapter: Any, database: Any }

      def backend_type
        "DB"
      end

      def adapter_name
        meta[:adapter].to_s.capitalize
      end

      def backend_name
        "#{adapter_name} #{meta[:database]}"
      end

      def identifying_properties
        { "VENDOR"   => adapter_name,
          "DATABASE" => meta[:database] }
      end

    end
  end
end