import Config

config :nextcast, Nextcast.TCPServer, [
  transport: (if config_env() == :prod, do: :ranch_tcp, else: :ranch_ssl),

  transport_opts: (if config_env() in [:prod], do: [
    ip: {:local, System.get_env("TCP_UNIX_SOCKET", "nextcast.sock")},
    port: 0
  ], else: [
    port: System.get_env("TCP_PORT", "8888") |> Integer.parse |> elem(0),
    certfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}.pem"]),
    keyfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}-key.pem"])
  ]),

  protocol: (if config_env() == :prod, do: :cowboy_clear, else: :cowboy_tls),
]

config :nextcast, Nextcast.RTPServer, [
  psk: System.get_env("RTP_PSK"),
  udp_opts: (if config_env() == :prod, do: [
    ip: {:local, System.get_env("RTP_UNIX_SOCKET", "nextcast-rtp.sock")},
    port: 0,
  ], else: [
    port: System.get_env("RTP_PORT", "8889") |> Integer.parse |> elem(0)
  ])
]

config :nextcast, Nextcast.DB, [
  database: System.get_env("DB_NAME", "nextcast"),
  name: Nextcast.DB,
  parameters: [application_name: "nextcast"],
  socket_dir: System.get_env("DB_SOCKDIR")
]

config :nextcast, Nextcast.StreamSupervisor, [
  %{
    key: :system_stream_1,
    listener: [
      address: %{
        host: System.get_env("LISTENER_HOST"),
        port: System.get_env("LISTENER_PORT", "443") |> Integer.parse |> elem(0),
      },
      history_url: System.get_env("LISTENER_HISTORY_URL"),
    ],
  }
]

config :nextcast, Nextcast.ExtendedMetadata, [
  spotify_token: System.get_env("EXTENDEDMETADATA_SPOTIFY_KEY"),
  lastfm_key: System.get_env("EXTENDEDMETADATA_LASTFM_KEY"),
]

config :nextcast, Nextcast.Auth, [
  secret_key_base: System.get_env("AUTH_SECRET_KEY_BASE"),
]

config :logger, :console,
  level: :info,
  metadata: [:module, :function, :line]