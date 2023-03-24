# Nextcast

## Requirements

- [PostgreSQL](https://www.postgresql.org)
- [Erlang/OTP 24](https://www.erlang.org)

## Architecture diagrams

### C4-L1<sup>[1](https://c4model.com)</sup>
```
┌─────────────┐
│   Source    │     ┌──────────┐     ┌───────────┐
│ (eg. RIST,  │ ←─→ │ Nextcast │ ←─→ │  client   │
│ Shoutcast™) │     └──────────┘     |   (HLS)   │
└─────────────┘             ↑        └───────────┘
                            |        ┌───────────┐
                            └──────→ │  client   │
                                     │ (Icecast) │
                                     └───────────┘
```


## Supported source protocols (ingestion)

1. Icecast / Shoutcast™
    - Tested with Shoutcast™ v1.9.8 and v2.5.1
2. RIST (VSF TR-06-2:2021, Main profile)
    - Stream encryption (AES-128-CTR / AES-256-CTR) is **required**, eg. as `secret=$PSK` and `aes-type=128|256` parameters in the RIST URL. Unencrypted packets will be discarded.
    - Tested with [OBS](https://obsproject.com) 29.0, [FFmpeg](https://ffmpeg.org) 5.1, and [libRIST](https://code.videolan.org/rist/librist) 0.2.7
      - [OBS](https://obsproject.com) >=28.0, [FFmpeg](https://ffmpeg.org) >=4.4, and [libRIST](https://code.videolan.org/rist/librist) >=0.2.0 are also expected to work

## Supported sink protocols (distribution)

1. Icecast/Shoutcast
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
