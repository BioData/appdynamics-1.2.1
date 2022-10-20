require 'thread'
require 'fileutils'
require 'lockfile'

module AppDynamics
  class NodeIndexPool
    include Skylight::Core::Util::Logging

    attr_reader :path, :config

    MAX_INDEX = 256
    KEEPALIVE = 60 # seconds

    def initialize(path, config:)
      @path = path
      @config = config
    end

    def index
      @index ||= fetch_index
    end

    def exit
      debug "Exiting index pool; index=#{@index}"
      @thread&.kill
      if @index
        FileUtils.rm_f(path_for_nodeindex(index))
        @index = nil
      end
    end

    private

    def next_index(max)
      (1..max).find do |i|
        file = path_for_nodeindex(i)
        !File.exist?(file) || File.mtime(file) < Time.now - (KEEPALIVE * 2)
      end
    end

    def touch_index(index)
      path = path_for_nodeindex(index)
      debug "Touching #{path}"
      FileUtils.touch(path)
    end

    def keepalive(index)
      @thread = Thread.new do
        loop do
          touch_index(index)
          sleep KEEPALIVE
        end
      end
    end

    # NOTE: This may be susceptible to race conditions
    def fetch_index
      index = nil
      FileUtils.mkdir_p(path)
      Lockfile.new(File.expand_path("lock", path)) do
        if (index = next_index(MAX_INDEX))
          touch_index(index)
        else
          raise "Unable to get node index"
        end
      end
      keepalive(index)
      index
    end

    def path_for_nodeindex(index)
      File.expand_path("#{index}.nodeindex", path)
    end
  end
end
