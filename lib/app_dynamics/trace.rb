module AppDynamics
  class Trace < Skylight::Core::Trace

    def self.new(instrumenter, *rest)
      instrumenter.finalize_start
      super(instrumenter, *rest)
    end

    # Check whether a snapshot is currently in progress. This can be useful to avoid running
    # heavy snapshot related code when not snapshotting.
    #
    # @return [true, false] whether a snapshot is in progress
    def is_snapshotting
      native_is_snapshotting
    end

    # Add details to the current snapshot. Either takes a hash or a block that returns
    # a hash. The block will only be executed if currently snapshotting to avoid
    # running heavy code unnecessarily.
    #
    # @param details [Hash] details to track for the current snapshot
    # @yieldreturn [Hash] details to track for the current snapshot
    #
    # @example With Hash
    #   add_snapshot_details(user_id: 123, user_name: "Peter")
    # @example With Block
    #   add_snapshot_details do
    #     user.load_extra_details # expensive call
    #     { user_id: user.id, user_name: user.name, user_extra_details: user.extra_details }
    #   end
    def add_snapshot_details(details=nil, &block)
      if (!details && !block_given?) || (details && block_given?)
        raise ArgumentError, "provide either details or block but not both"
      end

      if !details.nil? && !details.is_a?(Hash)
        raise ArgumentError, "details must be a Hash"
      end

      return unless is_snapshotting

      if block_given?
        return add_snapshot_details(yield);
      end

      details.each do |k,v|
        native_add_snapshot_details(k.to_s, v.to_s)
      end
    end

    # Set the URL for a snapshot
    #
    # @param url [String] url for the snapshot
    def set_snapshot_url(url)
      native_set_snapshot_url(url)
    end

  end
end
