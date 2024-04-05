import Config

config :nextcast, Nextcast.HTTPS,
  transport_opts: [
    ip: {:local, System.get_env("HTTPS_SOCKET", "nextcast.sock")},
    port: 0,
    certfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}.pem"]),
    keyfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}-key.pem"]),
    alpn_preferred_protocols: ["h2", "http/1.1"],
    next_protocols_advertised: ["h2", "http/1.1"]
  ]

config :nextcast, Nextcast.DB,
  database: System.get_env("DB_NAME", "nextcast"),
  name: Nextcast.DB,
  parameters: [application_name: "nextcast"],
  socket_dir: System.get_env("DB_SOCKDIR")

config :nextcast, Nextcast.Listener,
  address: %{
    host: System.get_env("LISTENER_HOST"),
    port: System.get_env("LISTENER_PORT", "443") |> Integer.parse |> elem(0),
  },
  history_url: System.get_env("LISTENER_HISTORY_URL")

config :nextcast, Nextcast.RTP,
  psk: System.get_env("RTP_PSK")

config :nextcast, Nextcast.ExtendedMetadata,
  spotify_token: System.get_env("EXTENDEDMETADATA_SPOTIFY_KEY"),
  lastfm_key: System.get_env("EXTENDEDMETADATA_LASTFM_KEY")

config :nextcast, Nextcast.Auth,
  secret_key_base: System.get_env("AUTH_SECRET_KEY_BASE")

config :logger, :console,
  level: :info,
  metadata: [:module, :function, :line]
