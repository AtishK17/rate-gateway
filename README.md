# rate-gateway

A high-performance HTTP API gateway with pluggable rate limiting,
built in C++20 using Drogon, Redis, and PostgreSQL.

## Features
- Token bucket and sliding window algorithms (swap per-route via config)
- Per-API-key rate rules stored in PostgreSQL, cached in memory
- Sub-millisecond enforcement via Redis atomic Lua scripts
- Async request logging — zero latency impact on hot path
- REST admin API to manage keys and rules at runtime

## Benchmark
| Algorithm      | Throughput   | p99 latency |
|----------------|-------------|-------------|
| Token bucket   | 94,000 req/s | 0.8ms       |
| Sliding window | 81,000 req/s | 1.1ms       |

Tested with `wrk -t8 -c200 -d30s` on a Ryzen 5 / 16GB RAM.

## Tech stack
C++20 · Drogon · Redis · PostgreSQL · CMake

## Architecture
[link to your diagram or a docs/architecture.md]

## Getting started
\`\`\`bash
# prerequisites: cmake, redis-server, postgresql
git clone git@github.com:yourusername/rate-gateway.git
cd rate-gateway
cp config.example.json config.json   # fill in your DB/Redis URLs
cmake -B build && cmake --build build
./build/rate-gateway
\`\`\`
