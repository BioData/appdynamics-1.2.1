# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.1] - 2020-06-08
### Fixes
- Resolved a memory leak

## [1.2.0] - 2020-03-17
### Added
- Additional log configuration options

### Changed
- Updated AppDynamics C SDK to 4.5.3.0

### Fixed
- Resolved frozen string error when using with Sidekiq.

## [1.1.2] - 2019-12-18
### Fixed
- Correct logging location for node index pool
- Avoid unnecessary 'invalid type' warnings

## [1.1.1] - 2019-10-30
### Fixed
- Segfault due to a poorly timed GC

## [1.1.0] - 2019-06-04
### Added
- nodeindex path can be configured with `nodeindex_path` in the config file or `APPD_NODEINDEX_PATH` in the environment.
- A proxy can be configured with `controller.http_proxy_host`, `...port`, `...username`, `...password`, `...password_file` or in the environment with `APPD_CONTROLLER_HTTP_PROXY_*`.
- SQL parsing errors can be silenced by setting `log_sql_parse_errors: false` in config or `APPD_LOG_SQL_PARSE_ERRORS=false` in the environment.

### Changed
- Redis exit calls are now reported as cache instead of database.

### Fixed
- Changing the controller port now works correctly.
- Sidekiq instrumentation now works correctly.
- A GC related crash has been resolved.

## [1.0.0] - 2019-04-30
- First Release
