# grafcik

A tiny header-only C++23 library for immutable graphs and flow networks.

## Usage example (max flow with Dinic)

```cpp
#include <cstdint>
#include <iostream>

#include "grafcik.hpp"

int main() {
        using Grafcik::VertexId;
        using Grafcik::FlowNetwork::Dinic;
        using Grafcik::FlowNetwork::FlowNetworkBuilder;

        // Build a flow network with 4 vertices (0..3).
        FlowNetworkBuilder builder(4);
        builder.AddEdge(0, 1, 10);
        builder.AddEdge(0, 2, 5);
        builder.AddEdge(1, 2, 15);
        builder.AddEdge(1, 3, 10);
        builder.AddEdge(2, 3, 10);

        auto graph = builder.Build();
        Dinic dinic(graph, VertexId{0}, VertexId{3});

        std::int64_t max_flow = dinic.MaxFlow();
        std::cout << "Maximum flow from vertex 0 to vertex 3: " << max_flow
                            << '\n';

        return 0;
}
```

## Notes

- Namespace is `Grafcik` (capital G).
- Graphs are immutable after construction; use `GraphBuilder` or
    `FlowNetworkBuilder`.
- Traversals are provided via `Traverses::BreadthFirstSearch` and
    `Traverses::DepthFirstSearch`.
