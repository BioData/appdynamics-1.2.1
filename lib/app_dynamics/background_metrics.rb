require 'barnes/resource_usage'
require 'barnes/periodic'

module AppDynamics
  class BackgroundMetrics
    include Skylight::Core::Util::Logging

    attr_reader :instrumenter
    attr_accessor :sample_rate

    # Metrics are tracked as an average per-process on each node
    # COUNTERS are per-minute, GAUGES are current values

    COUNTERS = {
      :"Time.pct.cpu" => "% CPU",
      :"GC.count" => "GC|Total/min",
      :"GC.major_count" => "GC|Major/min",
      :"GC.minor_gc_count" => "GC|Minor/min",
      :"GC.time" => "GC|ms/min",
      :"VM.method_cache_invalidations" => "Cache|Method Cache Invalidations/min",
      :"VM.constant_cache_invalidations" => "Cache|Constant Cache Invalidations/min",
    }

    GAUGES = {
      :"Objects.TOTAL" => "GC|Total Objects Allocated",
      :"GC.heap_live_slots" => "GC|Heap Live Slots",
      :"GC.heap_free_slots" => "GC|Heap Free Slots",
      :"Proc.RSS" => "Memory (RSS, MB)",
      :"Threads.alive" => "Threads|Alive",
      :"Threads.running" => "Threads|Running",
      :"Threads.sleeping" => "Threads|Sleeping",
    }

    class GCTime

      def initialize(config, sample_rate)
        @gc = config.gc
        @sample_rate = sample_rate
      end

      def start!(state)
        # In milliseconds
        state[:gc_time] = @gc.total_time / 1_000
      end

      def instrument!(state, counters, gauges)
        last = state[:gc_time]
        cur = state[:gc_time] = @gc.total_time / 1_000
        val = cur - last

        counters[:'GC.time'] = val * (1/@sample_rate)
      end

    end

    class Memory

      def self.status_file
        @status_file ||= "/proc/#{Process.pid}/status"
      end

      def self.available?
        File.exists?(status_file)
      end

      def instrument!(state, counters, gauges)
        gauges[:'Proc.RSS'] = File.open(self.class.status_file, "r") do |file|
          if file.read_nonblock(4096) =~ /RSS:\s*(\d+) kB/i
            $1.to_f / 1024
          end
        end
      end

    end

    class RubyVM

      CACHES = {
        global_method_state: :'VM.method_cache_invalidations',
        global_constant_state: :'VM.constant_cache_invalidations'
      }

      def initialize(sample_rate)
        @sample_rate = sample_rate
      end

      def start!(state)
        state[:ruby_vm] = ::RubyVM.stat
      end

      def instrument!(state, counters, gauges)
        last = state[:ruby_vm]
        cur = state[:ruby_vm] = ::RubyVM.stat

        CACHES.each do |key, metric|
          val = cur[key] - last[key]
          counters[metric] = val * (1/@sample_rate)
        end
      end

    end

    class Threads

      def instrument!(state, counters, gauges)
        alive = 0
        running = 0
        sleeping = 0

        Thread.list.each do |t|
          if t.alive?
            alive += 1
            if t.stop?
              sleeping += 1
            else
              running += 1
            end
          end
        end

        gauges[:'Threads.alive'] = alive
        gauges[:'Threads.running'] = running
        gauges[:'Threads.sleeping'] = sleeping
      end

    end

    class Panel < Barnes::Panel
      def initialize(config, sample_rate)
        super()

        instrument GCTime.new(config, sample_rate)

        if Memory.available?
          instrument Memory.new
        end

        instrument RubyVM.new(sample_rate)

        instrument Threads.new
      end
    end

    def initialize(instrumenter)
      @instrumenter = instrumenter

      # The AppDynamics Controller will sum all metrics reported on a server in a
      # 1-minute period. Since we can't distinguish individual Ruby processes, we want to
      # report the 1-minute values once a minute and display the total across all processes.

      # Barnes reports two types of values:
      #   * Counters: tracked over the given interval (e.g. number of GCs, CPU time)
      #   * Gauges: value at the specific point in time (e.g. number of objects currently allocated)

      # How often, in seconds, to instrument and report
      interval = 60
      # The minimal aggregation period in use, in seconds.
      # This value appears to be hardcoded in Barnes::Periodic, but that's ok because it's the
      # same as what the AppDynamics collector uses
      aggregation_period = 60

      # To further increase accuracy, we could decrease the interval and then average the data
      # locally before sending to AppDynamics.

      sample_rate = interval.to_f / aggregation_period.to_f

      panels = [Barnes::ResourceUsage.new(sample_rate), Panel.new(config, sample_rate)]
      @periodic = Barnes::Periodic.new(reporter: self, sample_rate: sample_rate, panels: panels)

      (COUNTERS.values + GAUGES.values).each do |name|
        native_define_metric(name)
      end
    end

    def config
      instrumenter.config
    end

    def stop
      @periodic.stop
    end

    def report(env)
      COUNTERS.each do |metric, name|
        # These are sampled, so we need to convert the value accordingly
        value = env[Barnes::COUNTERS][metric] / sample_rate
        native_report_metric(name, value)
      end

      GAUGES.each do |metric, name|
        value = env[Barnes::GAUGES][metric]
        native_report_metric(name, value)
      end
    end

  end
end
