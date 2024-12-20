# Nextcast

A live streaming suite using state of the art protocols to enable low latency and secure audio/video distribution.

## Features

- **Icecast compatibility** (source and/or sink) makes it easy to set up Nextcast alongside existing setups commonly found in online radio broadcasting -- providing an easy path to distribute existing streams over more reliable and secure protocols such as HLS while ensuring continued support for legacy clients.
- **Encrypted audio/video ingestion with RIST**, an open-source, open-specification, and RTP-compatible protocol designed for reliable transmission over lossy networks with low latency and high quality.
- **Built-in web player** that automatically connects to the most reliable and secure supported transport. With Javascript enabled (optional), it includes 100+ visualizations for audio-only streams and displays playback history with cover art from Apple Music®, Spotify®, or Last.fm®.
- **Currently active and unique listener statistics** for the past month, collected for all distribution protocols.

## Runtime dependencies

- [PostgreSQL](https://www.postgresql.org) >=16.0

## Build requirements

- [Erlang/OTP](https://www.erlang.org) and [Elixir](https://elixir-lang.org) as specified by `.tool-versions`
- [Node.js](https://nodejs.org) and [pnpm](https://pnpm.io) as specified by `package.json`

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
### Pulling from other sources
1. ADTS/MP3 from an Icecast or Shoutcast® server
    - Tested with Shoutcast® v1.9.8 and v2.5.1

### Pushing to Nextcast
1. RIST (VSF TR-06-2:2021, Main profile)
    - Stream encryption (AES-128-CTR / AES-256-CTR) is **required**, eg. as `secret=$PSK` and `aes-type=128|256` parameters in the RIST URL. Unencrypted packets will be discarded.
    - Tested with [OBS](https://obsproject.com) 29.0, [FFmpeg](https://ffmpeg.org) 5.1, and [libRIST](https://code.videolan.org/rist/librist) 0.2.7
      - [OBS](https://obsproject.com) >=28.0, [FFmpeg](https://ffmpeg.org) >=4.4, and [libRIST](https://code.videolan.org/rist/librist) >=0.2.0 are also expected to work
2. MPEG-TS over TLS over TCP
    - Tested with [FFmpeg](https://ffmpeg.org) 7.0, eg. `ffmpeg -i ... -f mpegts -cert_file $CERTFILE -key_file $KEYFILE tls://$IP:PORT`
3. ADTS/MP3 over HTTP
    - Tested with a [fork](https://github.com/djankovic/nextcast/tree/main/packages/butt) of Daniel Nöthen's [butt](https://danielnoethen.de/butt/), adding support for Transfer-Encoding: chunked
    - Requires HTTP/1.1
4. Opus via WebRTC (with WHIP)
    - Experimental

## Supported sink protocols (distribution)

1. Icecast
2. HLS

## Codec support / transcoding matrix

```
                ┌───────────────+───────────────+───────────────+───────────────┐
          +─────+ MP3           | AAC-LC        | H264 (High 4) | Opus          |
  ┌────┐  | OUT | MPEG-2 Audio  | ADTS          | MPEG-TS       | N/A           |
  | IN |  +─────+ HLS, Icecast  | HLS, Icecast  | HLS           | Icecast       |
┌─+────+────────+───────────────+───────────────+───────────────+───────────────+
| MP3           |               |               |               |               |
| MPEG-2 Audio  |       ✔       |       ✘       |       ✘       |       ✘       |
| Icecast       |               |               |               |               |
+───────────────+───────────────+───────────────+───────────────+───────────────+
| AAC-LC        |               |               |               |               |
| ADTS          |       ✘       |       ✔       |       ✘       |       ✘       |
| Icecast       |               |               |               |               |
+───────────────+───────────────+───────────────+───────────────+───────────────+
| H264 (High 4) |               |               |               |               |
| MPEG-TS       |       ✘       |       ✘       |       ✔       |       ✘       |
| RIST          |               |               |               |               |
+───────────────+───────────────+───────────────+───────────────+───────────────+
| Opus          |               |               |               |               |
| N/A           |       ✘       |       ✘       |       ✘       |       ✔       |
| WebRTC-WHIP   |               |               |               |               |
└───────────────+───────────────+───────────────+───────────────+───────────────┘
```
