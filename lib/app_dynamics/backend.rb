module AppDynamics
  # Categorizes an Exit Call Backend and adds identifiying information.
  module Backend
    class AnyMatcher
      def ===(other)
        !other.nil?
      end
    end
    Any = AnyMatcher.new

    class Matcher

      def initialize(matchers)
        @matchers = {}
        matchers.each do |key, value|
          @matchers[key] = value.is_a?(Hash) ? Matcher.new(value) : value
        end
      end

      def ===(matchers)
        return false if matchers.nil?
        @matchers.all?{|k,v| v === matchers[k] }
      end

    end

    class Base

      def self.matches(matchers)
        AppDynamics::Backend.register(self, Matcher.new(matchers))
      end

      attr_reader :category, :title, :description, :meta

      def initialize(cat, title, desc, meta)
        @category    = cat
        @title       = title
        @description = desc
        @meta        = meta
      end

      # One of "HTTP", "DB", "CACHE", "RABBITMQ", "WEBSERVICE", "JMS"
      def backend_type
        ''
      end

      def backend_name
        self.class.name.split('::').last
      end

      def identifying_properties
        { }
      end

      # Make it easier for C API to consume
      def identifying_properties_array
        identifying_properties.to_a.flatten.map(&:to_s)
      end

    end

    def self.register(klass, matcher)
      (@registry ||= {})[matcher] = klass
    end

    def self.registry
      @registry
    end

    def self.find(cat, title, desc, meta)
      hash = { category: cat, title: title, description: desc, meta: meta }
      _, klass = @registry.find{|k,v| k === hash }
      klass
    end

    def self.build(*args)
      if klass = find(*args)
        klass.new(*args)
      end
    end

  end
end

%w(elasticsearch http mongo redis sql_database).each do |backend|
  require "app_dynamics/backend/#{backend}"
end
