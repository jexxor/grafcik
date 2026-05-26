# grafcik

A tiny header-only library focused on construction of flow networks.

## Usage example

```cpp
#include "grafcik.hpp"

int main() {
    using namespace grafcik;

    // Create a flow network with 4 vertices
    FlowNetwork graph(4);

    // Add edges with capacities
    graph.AddEdge(0, 1, 10);
    graph.AddEdge(0, 2, 5);
    graph.AddEdge(1, 2, 15);
    graph.AddEdge(1, 3, 10);
    graph.AddEdge(2, 3, 10);

    // Compute the maximum flow from vertex 0 to vertex 3
    int max_flow = graph.MaxFlow(0, 3);

    std::cout << "Maximum flow from vertex 0 to vertex 3: " << max_flow << std::endl;

    return 0;
}
```
