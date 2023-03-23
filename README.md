# Nextcast

## Requirements

- [PostgreSQL](https://www.postgresql.org)
- [Erlang/OTP 24](https://www.erlang.org)

## Architecture diagrams

### C4-L1<sup>[1](https://c4model.com)</sup>
```
┌────────────┐
│   Source   │     ┌──────────┐     ┌───────────┐
│ (eg. RIST, │ ←─→ │ Nextcast │ ←─→ │  client   │
│ Shoutcast) │     └──────────┘     |   (HLS)   │
└────────────┘              ↑       └───────────┘
                            |       ┌───────────┐
                            └─────→ │  client   │
                                    │ (Icecast) │
                                    └───────────┘
```


## Supported source protocols (ingestion)

1. Icecast/Shoutcast
2. RIST (Main profile)

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
