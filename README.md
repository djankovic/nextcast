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
                        IN
             ┌───────+───────+───────┐
             |  MP3  |  AAC  | H.264 |
     ┌───────+───────+───────+───────+
     |  MP3  |   ✔   |   ✘   |   ✘   |
     +───────+───────+───────+───────+
OUT  |  AAC  |   ✘   |   ✔   |   ✘   |
     +───────+───────+───────+───────+
     | H.264 |   ✘   |   ✘   |   ✔   |
     └───────+───────+───────+───────┘
```
