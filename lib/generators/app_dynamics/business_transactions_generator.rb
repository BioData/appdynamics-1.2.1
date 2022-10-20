require "generators/app_dynamics"
require "action_dispatch/routing/inspector"
require "app_dynamics/business_transactions"

module AppDynamics
  module Generators
    class BusinessTransactionsGenerator < Base
      desc "EXPERIMENTAL: Generate BusinessTransactions from current Rails Routes"

      def create_business_transactions
        routes = Rails.application.routes.routes.reject(&:internal)
        bts = routes.map do |route|
          path = route.path.spec.to_s
          path.gsub!('.', '\.')
          path = if route.parts.length > 0
            route.parts.each do |part|
              path.sub!(":#{part}", ".+")
            end
            path.gsub!(')', ')?')
            "%r{#{path}}"
          else
            "'#{path}'"
          end

          bt = ""
          if route.name
            bt << "bt '#{route.name}' => "
          else
            bt << "# bt ??? => "
          end
          bt << path

          verbs = route.verb.split("|")
          unless verbs.empty?
            bt << ", method: "
            if verbs.length == 1
              bt << ":#{::AppDynamics::BusinessTransactions::METHODS.key(verbs.first)}"
            else
              bt << "[" << verbs.map{|v| ":#{::AppDynamics::BusinessTransactions::METHODS.key(v)}" }.join(", ") << "]"
            end
          end
          bt
        end

        output = ["AppDynamics::BusinessTransactions.define do"]
        output.concat bts.map{|bt| "  #{bt}" }
        output << ["end"]

        initializer "app_dynamics.rb", output.join("\n")
      end

      class FileFormatter

        def initialize
          @buffer = []
        end

        def result
          @buffer
        end

        def section_title(title)
          @buffer << "# #{title}"
        end

        def section(routes)
          @buffer << draw_section(routes)
        end

        def header(routes)
          # @buffer << draw_header(routes)
        end

        def no_routes(*)
          @buffer << "# No routes"
        end

        private

          def draw_section(routes)
            header_lengths = ["Prefix", "Verb", "URI Pattern"].map(&:length)
            name_width, verb_width, path_width = widths(routes).zip(header_lengths).map(&:max)

            routes.map do |r|
              unnamed = r[:name].empty?
              bt = ""
              bt << "# " if unnamed
              bt << "bt #{(unnamed ? "?" : "'" + r[:name] + "'").ljust(name_width+2)} => "
              bt << "#{r[:path]}"

              # "#{r[:name].rjust(name_width)} #{r[:verb].ljust(verb_width)} #{r[:path].ljust(path_width)} #{r[:reqs]}"
              bt
            end
          end

          def draw_header(routes)
            name_width, verb_width, path_width = widths(routes)

            "#{"Prefix".rjust(name_width)} #{"Verb".ljust(verb_width)} #{"URI Pattern".ljust(path_width)} Controller#Action"
          end

          def widths(routes)
            [routes.map { |r| r[:name].length }.max || 0,
             routes.map { |r| r[:verb].length }.max || 0,
             routes.map { |r| r[:path].length }.max || 0]
          end

      end

    end
  end
end
