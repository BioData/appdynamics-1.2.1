

module AppDynamics
  module Backend
    class Elasticsearch < Base

      matches category: 'db.elasticsearch.request'

      def backend_type
        "DB"
      end

      def backend_name
        "Elasticsearch"
      end

      def identifying_properties
        { "VENDOR"   => "Elasticsearch" }
      end

    end
  end
end