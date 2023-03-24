# Nextcast

A live streaming suite using state of the art protocols to enable low latency and secure audio/video distribution.

## Features

- **Icecast compatibility** (source and/or sink) makes it easy to set up Nextcast alongside existing setups commonly found in online radio broadcasting -- providing an easy path to distribute existing streams over more reliable and secure protocols such as HLS while ensuring continued support for legacy clients.
- **Encrypted audio/video ingestion with RIST**, an open-source, open-specification, and RTP-compatible protocol designed for reliable transmission over lossy networks with low latency and high quality.
- **Built-in web player** that automatically connects to the most reliable and secure supported transport. With Javascript enabled (optional), it includes 100+ visualizations for audio-only streams and displays playback history with cover art from Apple Music®, Spotify®, or Last.fm®.
- **Currently active and unique listener statistics** for the past month, collected for all distribution protocols.

## Runtime dependencies

- [LibreSSL](https://www.libressl.org) or [OpenSSL](https://www.openssl.org) >=3.0
- [PostgreSQL](https://www.postgresql.org) >=13.0 with   [TimescaleDB](https://timescale.com/install/latest/self-hosted/) >=2.10
  - Required only if the `Nextcast.Track` module is explicitly enabled in `config/runtime.exs`
  - TimescaleDB is a PostgreSQL extension that may need to be manually enabled in PostgreSQL's configuration files depending on the installation method. If `select * from pg_available_extensions;` doesn't list `timescaledb`, refer to [TimescaleDB's installation instructions](https://docs.timescale.com/install/latest/)

## Build requirements

- [Erlang/OTP](https://www.erlang.org) >=24.0
- [Elixir](https://elixir-lang.org) version listed in `mix.exs`

## Architecture diagrams

### C4-L1<sup>[1](https://c4model.com)</sup>
```
┌────────────┐
│   Source   │     ┌──────────┐     ┌───────────┐
│ (eg. RIST, │ ←─→ │ Nextcast │ ←─→ │  client   │
│  Icecast)  │     └──────────┘     |   (HLS)   │
└────────────┘             ↑        └───────────┘
                           |        ┌───────────┐
                           └──────→ │  client   │
                                    │ (Icecast) │
                                    └───────────┘
```


## Supported source protocols (ingestion)

1. Icecast
    - Tested with Shoutcast® v1.9.8 and v2.5.1
2. RIST (VSF TR-06-2:2021, Main profile)
    - Stream encryption (AES-128-CTR / AES-256-CTR) is **required**, eg. as `secret=$PSK` and `aes-type=128|256` parameters in the RIST URL. Unencrypted packets will be discarded.
    - Tested with [OBS](https://obsproject.com) 29.0, [FFmpeg](https://ffmpeg.org) 5.1, and [libRIST](https://code.videolan.org/rist/librist) 0.2.7
      - [OBS](https://obsproject.com) >=28.0, [FFmpeg](https://ffmpeg.org) >=4.4, and [libRIST](https://code.videolan.org/rist/librist) >=0.2.0 are also expected to work

## Supported sink protocols (distribution)

1. Icecast
2. HLS

## Codec support / transcoding matrix

```
                ┌───────────────+───────────────+───────────────┐
          +─────+ MP3 (ADTS)    | AAC-LC (ADTS) | H264 (High 4) |
  ┌────┐  | OUT | mp4a.40.34    | mp4a.40.2     | avc1.64001f   |
  | IN |  +─────+ HLS, Icecast  | HLS, Icecast  | HLS           |
┌─+────+────────+───────────────+───────────────+───────────────+
| MP3           |               |               |               |
| mp4a.40.34    |       ✔       |       ✘       |       ✘       |
| ADTS, Icecast |               |               |               |
+───────────────+───────────────+───────────────+───────────────+
| AAC-LC        |               |               |               |
| mp4a.40.2     |       ✘       |       ✔       |       ✘       |
| ADTS, Icecast |               |               |               |
+───────────────+───────────────+───────────────+───────────────+
| H264 (High 4) |               |               |               |
| avc1.64001f   |       ✘       |       ✘       |       ✔       |
| RIST          |               |               |               |
└───────────────+───────────────+───────────────+───────────────┘
```
