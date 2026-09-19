# RouteForge: Mini Route Optimization Engine

RouteForge is a C++ service that models a network as a graph and computes optimal paths. It is similar to a tiny version of production route optimization engines.

## Features
- **Core Engine**: Topology graph with nodes and links.
- **Algorithms**: Dijkstra shortest paths with interchangeable weight functions (Cost, Latency, Hop Count).
- **Constrained Routing**: Maximum utilization, minimum available bandwidth, excluded links, and excluded nodes.
- **Resilience**: A failed link automatically re-routes affected stored paths, or marks them `UNROUTABLE`.
- **HTTP API**: Topology load/read, path computation and history, link metric updates, fail, and restore operations.

*Note: Real routing protocols, device integration, authentication, and UI are out of scope.*

## Getting Started

### Prerequisites
- CMake 3.20+
- A current C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+). The old MinGW 6.x distribution is unsupported.
- Ninja (recommended)

### Building
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### Testing
```sh
ctest --test-dir build --output-on-failure
```

### Run locally

```powershell
.\build\routeforge_server.exe
```

The service listens on `http://localhost:8080`. Use `PORT=9090` to choose another port.

### API demo

Load this small topology (physical links are bidirectional):

```powershell
$topology = '{"nodes":[{"name":"A"},{"name":"B"},{"name":"C"}],"links":[{"id":1,"src":"A","dst":"B","bandwidth_mbps":1000,"latency_ms":2,"cost":1},{"id":2,"src":"B","dst":"C","bandwidth_mbps":1000,"latency_ms":2,"cost":1},{"id":3,"src":"A","dst":"C","bandwidth_mbps":1000,"latency_ms":10,"cost":10}]}'
Invoke-RestMethod http://localhost:8080/topology -Method Post -ContentType application/json -Body $topology
Invoke-RestMethod http://localhost:8080/paths/compute -Method Post -ContentType application/json -Body '{"source":"A","target":"C","metric":"cost","constraints":{"max_utilization":0.8}}'
Invoke-RestMethod http://localhost:8080/paths/1
Invoke-RestMethod http://localhost:8080/links/2/fail -Method Post
Invoke-RestMethod http://localhost:8080/paths/1
Invoke-RestMethod http://localhost:8080/paths/1/history
```

Expected behaviour: the first route uses links `[1,2]` with cost `2`; after link `2` fails it is automatically re-routed through link `[3]` with status `REROUTED` and cost `10`. If you fail link `3` too, the route becomes `UNROUTABLE`.

### Endpoint summary

`GET /health`, `GET|POST /topology`, `POST /paths/compute`, `GET /paths/{id}`, `GET /paths/{id}/history`, `PATCH /links/{id}`, `POST /links/{id}/fail`, and `POST /links/{id}/restore`.

### Current scope and next milestone

The running service is deliberately in-memory: it does not yet use the PostgreSQL container defined in `docker-compose.yml`. The schema is present, but libpqxx repository implementation, benchmark targets, and CI are still future work. Do not describe those as completed in a résumé until they are actually wired and verified.
