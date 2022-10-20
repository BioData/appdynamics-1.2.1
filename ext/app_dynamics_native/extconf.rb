require 'mkmf'
require 'pathname'

# For escaping the GVL
unless have_func('rb_thread_call_without_gvl', 'ruby/thread.h')
  have_func('rb_thread_blocking_region') or abort "Ruby is unexpectedly missing rb_thread_blocking_region. This should not happen."
end

platform_dir = Pathname.new("../#{Gem::Platform.local}").expand_path(__dir__)

idir, ldir = dir_config("appdynamics", platform_dir.join("sdk/include").to_s, platform_dir.join("sdk/lib").to_s)

if File.exist?(idir) && File.exist?(ldir)
  find_library('appdynamics', nil, ldir)
  find_library('sqllexer', nil, platform_dir.to_s)
  find_library('pthread', nil)
  find_library('dl', nil)
  have_func("lex_sql_unknown")
else
  warn "AppDynamics SDK is not installed. Agent will not report data."
end

have_header('appdynamics.h')

create_header
create_makefile 'app_dynamics_native'
