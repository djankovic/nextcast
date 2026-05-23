import Config

config :elixir, time_zone_database: Zoneinfo.TimeZoneDatabase

# Don't download Membrane's precompiled libsrtp
config :bundlex, :disable_precompiled_os_deps, apps: [:ex_libsrtp]
