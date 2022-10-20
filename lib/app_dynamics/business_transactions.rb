module AppDynamics
  # The BusinessTransactions module specifies a list of named Business Transactions
  # and which requests should map to them. All non-mapped requests are ignored.
  #
  #   AppDynamics::BusinessTransactions.define do
  #     bt "index" => "/" # exact match (any method)
  #     bt "help_page" => %r{^/help_topics/\d+/?$} # regex match (any method)
  #     bt "user_update" => %r{^/users/\d+/?$}, method: :put # regex match with method
  #     bt "multi" => ["/multi", %r{/multi/id/.+}], method: [:post, :put]
  #     bt "multi_block" do
  #       get "/foo", %r{^/foo/[a-z]+} # multiple strings or regexes
  #       post "/bar"
  #     end
  #   end
  #
  # A Business Transaction is specified as a name mapping to a path matcher which can
  # be a String (for an exact match) or a Regex or an array of either type.
  #
  #   bt "users" => "/users"
  #   bt "admin" => %r{^/admin/?}
  #   bt "companies" => ["/company_list", "/companies", %r{/company/\d+}]
  #
  # You can also specify an HTTP method or list of methods to limit to only requests
  # of that type. Available options are `:get`, `:post`, `:put`, `:patch`, and `:delete`.
  #
  #   bt "new_user" => "/users", method: :post
  #   bt "update_user" => "/users", method: [:put, :patch]
  #
  # Finally, more complex definitions can be made using the block API. Inside of the block
  # a list of path matchers is specified for a specific HTTP method.
  #
  #   bt "api" do
  #     get "/users"
  #     post "/users", %r{.+\.json$}
  #   end
  module BusinessTransactions

    def self.reset!
      @global_set = TransactionSet.new
    end

    def self.global_set
      @global_set || reset!
    end

    def self.define(&block)
      global_set.define(&block)
    end

    class Transaction
      def name_for(env)
        nil
      end

      def matches?(env)
        false
      end
    end

    class NamedTransaction < Transaction
      attr_reader :name, :matchers

      def initialize(name, matchers)
        @name = name
        @matchers = matchers
      end

      def name_for(env)
        name
      end

      def matches?(env)
        @matchers.any?{|m| m.matches?(env) }
      end

      def add_matcher(matcher)
        @matchers << matcher
      end

      # For testing
      def ==(other)
        name == other.name && matchers == other.matchers
      end
    end

    class AutoTransaction < Transaction

      def initialize(full: false, segments: nil, dynamic: nil)
        if full && (segments || dynamic)
          raise ArgumentError, "can't specify other options with full"
        end
        @full = full

        unless full
          segments ||= { first: 2 }
          symbolize_keys!(segments)
          if !segments[:first].is_a?(Integer) && !segments[:last].is_a?(Integer)
            raise ArgumentError, "segments must include :first or :last"
          end
          @segment_opts = segments

          dynamic ||= {}
          unless dynamic.empty?
            if dynamic.is_a?(Symbol)
              h = {}
              h[dynamic] = true
              dynamic = h
            end

            symbolize_keys!(dynamic)

            if dynamic.keys.length > 1
              raise ArgumentError, "can only specific one dynamic option"
            end

            unless %i(param header method host origin).include?(dynamic.keys.first)
              raise ArgumentError, "invalid dynamic type: #{dynamic.keys.first}"
            end
          end
          @dynamic_opts = dynamic
        end
      end

      def name_for(env)
        return env['PATH_INFO'] if @full

        segments = env['PATH_INFO'].split('/')
        segments.shift() if segments[0] && segments[0].empty?

        if (specific = @dynamic_opts[:segments])
          return segments.values_at(*specific.map{|i| i - 1 }).join
        end

        segments = if (first = @segment_opts[:first])
          segments[0...first]
        elsif (last = @segment_opts[:last])
          last = segments.length if last > segments.length
          segments[-last..-1]
        end

        name = '/' + segments.join('/')

        extra_val = if (param = @dynamic_opts[:param])
          req = Rack::Request.new(env)
          req.params[param.to_s]
        elsif (header = @dynamic_opts[:header])
          header = header.tr('-', '_').upcase
          env[header] || env["HTTP_#{header}"]
        elsif (method = @dynamic_opts[:method])
          env['REQUEST_METHOD']
        elsif (host = @dynamic_opts[:host])
          env['HTTP_HOST'] || env['SERVER_NAME']
        elsif (origin = @dynamic_opts[:origin])
          env['HTTP_ORIGIN']
        end

        unless extra_val.blank?
          name << '.' << extra_val
        end

        name
      end

      def matches?(*)
        true
      end

      # For testing
      def ==(other)
        @full == other.instance_variable_get(:@full) &&
          @segment_opts == other.instance_variable_get(:@segment_opts) &&
          @dynamic_opts == other.instance_variable_get(:@dynamic_opts)
      end

      private

      def symbolize_keys!(hash)
        hash.keys.each do |key|
          hash[key.to_sym] = hash.delete(key)
        end
      end
    end

    class Matcher
      def matches?(env)
        false
      end
    end

    class PathAndMethodMatcher < Matcher
      attr_reader :paths, :methods

      def initialize(paths, methods)
        @paths = paths
        @methods = methods
      end

      def matches?(env)
        path, method = env['PATH_INFO'], env['REQUEST_METHOD']
        @paths.any?{|p| p === path } && (@methods.nil? || @methods.any?{|m| m == method })
      end

      # For testing
      def ==(other)
        paths == other.paths && methods == other.methods
      end
    end


    METHODS = {
      get:    'GET'.freeze,
      post:   'POST'.freeze,
      put:    'PUT'.freeze,
      patch:  'PATCH'.freeze,
      delete: 'DELETE'.freeze
    }

    class TransactionSet

      def initialize
        @auto_transaction = AutoTransaction.new
        @named_transactions = {}
      end

      def define(&block)
        definition = Definition.new(self)
        definition.instance_exec(&block)
      end

      def all_transactions
        transactions = @named_transactions.values
        transactions << @auto_transaction if @auto_transaction
        transactions
      end

      def match(env)
        bt = all_transactions.find{|t| t.matches?(env) }
        bt ? bt.name_for(env) : nil
      end

      def update_auto(enabled=true, **options)
        if enabled
          @auto_transaction = AutoTransaction.new(**options)
        else
          @auto_transaction = nil
        end
      end

      def add_named_matcher(name, paths, methods=[])
        # FIXME: Check name type
        # FIXME: Check paths type
        # FIXME: Check methods type

        unless @named_transactions.has_key?(name)
          # FIXME: Check if we've exceeded limit
          @named_transactions[name] = NamedTransaction.new(name, [])
        end

        transaction = @named_transactions[name]

        paths = [paths] unless paths.is_a?(Array)

        unless methods.nil?
          methods = [methods] unless methods.is_a?(Array)
          methods.map!{|m| METHODS.fetch(m) }
        end

        # FIXME: Check data type
        transaction.add_matcher(PathAndMethodMatcher.new(paths, methods))
      end

    end

    class Definition

      def initialize(set)
        @set = set
      end

      def bt(name_or_hash, &block)
        if name_or_hash.is_a?(Hash)
          # FIXME: Add description
          raise ArgumentError if name_or_hash.empty?

          name = name_or_hash.keys.first
          paths = name_or_hash[name]
          method = name_or_hash[:method]

          @set.add_named_matcher(name, paths, method)
        else
          # FIXME: Add description
          raise ArgumentError unless block_given?
          name = name_or_hash

          scope = Scope.new(@set, name)

          scope.instance_exec(&block)
        end
      end

      def auto(*args, **keywords)
        @set.update_auto(*args, **keywords)
      end

    end

    class Scope

      def initialize(set, name)
        @set = set
        @name = name
      end

      def get(*paths)
        @set.add_named_matcher(@name, paths, :get)
      end

      def post(*paths)
        @set.add_named_matcher(@name, paths, :post)
      end

      def put(*paths)
        @set.add_named_matcher(@name, paths, :put)
      end

      def patch(*paths)
        @set.add_named_matcher(@name, paths, :patch)
      end

      def delete(*paths)
        @set.add_named_matcher(@name, paths, :delete)
      end

    end

  end
end
