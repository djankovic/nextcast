import Config

config :nextcast, Nextcast.TCPServer, [
  transport: (if config_env() == :prod, do: :ranch_tcp, else: :ranch_ssl),

  transport_opts: (if config_env() in [:prod], do: [
    ip: {:local, System.get_env("TCP_UNIX_SOCKET", "nextcast.sock")},
    port: 0
  ], else: [
    port: System.get_env("TCP_PORT", "8888") |> Integer.parse |> elem(0),
    certfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}.pem"]),
    keyfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}-key.pem"]),
    alpn_preferred_protocols: ["h2", "http/1.1"],
  ]),

  protocol: (if config_env() == :prod, do: :cowboy_clear, else: :cowboy_tls),
]

config :nextcast, Nextcast.DB, [
  database: System.get_env("DB_NAME", "nextcast"),
  name: Nextcast.DB,
  parameters: [application_name: "nextcast"],
  socket_dir: System.get_env("DB_SOCKDIR"),
  pool_size: System.get_env("DB_POOLSIZE", "25") |> Integer.parse |> elem(0),
]

config :nextcast, Nextcast.StreamSupervisor, [
  streams: (if config_env() == :prod, do: [
    system_stream_1: [
      listener: [
        source: {:icy, "tcp://#{System.get_env("LISTENER_HOST")}:#{System.get_env("LISTENER_PORT", "443")}"},
        history: {:icy_json, System.get_env("LISTENER_HISTORY_URL")},
      ],
      broadcasts: [
        hls: [
          target_duration: 4,
        ],
        icy: [],
      ],
    ],
    system_stream_2: [
      listener: [
        source: {:rtp, "unix://#{System.get_env("RTP_UNIX_SOCKET", "nextcast-rtp.sock")}?psk=#{System.get_env("RTP_PSK", "")}"}
      ],
      broadcasts: [
        hls: [
          target_duration: 4,
        ],
      ],
    ],
    # system_stream_3: [
    #   listener: [
    #     source: {:mpegts, "tls://localhost:8443"},
    #     cacertfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}-ca.pem"]),
    #     certs_keys: [%{
    #       certfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}.pem"]),
    #       keyfile: Application.app_dir(:nextcast, ["priv", "#{Atom.to_string(config_env())}-key.pem"]),
    #     }],
    #   ],
    #   broadcasts: [
    #     hls: [
    #       target_duration: 4,
    #     ],
    #   ],
    # ],
  ], else: [])
]

config :nextcast, Nextcast.ExtendedMetadata, [
  spotify_client_id: System.get_env("EXTENDEDMETADATA_SPOTIFY_CLIENT_ID"),
  spotify_client_secret: System.get_env("EXTENDEDMETADATA_SPOTIFY_CLIENT_SECRET"),
  lastfm_key: System.get_env("EXTENDEDMETADATA_LASTFM_KEY"),
]

config :nextcast, Nextcast.Auth, [
  secret_key_base: System.get_env("AUTH_SECRET_KEY_BASE"),
]

config :logger, :console,
  level: :info,
  metadata: [:module, :function, :line]
