#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Grafcik {

using VertexId = std::size_t;
using EdgeId = std::size_t;

template <typename G>
class GraphBuilder;

/// @brief Immutable adjacency-list graph.
/// @details The graph is immutable after construction; build it via
/// `GraphBuilder`.
template <typename EdgedataType = std::monostate,
          typename VertexdataType = std::monostate>
class Graph {
public:
    using VertexData = VertexdataType;
    using EdgeData = EdgedataType;

    struct Edge {
        VertexId from = 0;
        VertexId to = 0;
        [[no_unique_address]] EdgeData data;
    };

    void ForEachEdgeFrom(VertexId vertex, auto&& fn) const {
        for (EdgeId edge_id : kAdjacentEdges[vertex]) {
            fn(edge_id, kEdges[edge_id]);
        }
    }

    [[nodiscard]] bool HasEdge(EdgeId edge_id) const {
        return edge_id < kEdges.size();
    }

    [[nodiscard]] const Edge& GetEdge(EdgeId edge_id) const {
        if (!HasEdge(edge_id)) {
            throw std::out_of_range("Invalid edge_id");
        }

        return kEdges[edge_id];
    }

    [[nodiscard]] const VertexData& GetVertexData(VertexId vertex_id) const {
        return kVertices[vertex_id];
    }

    [[nodiscard]] std::vector<EdgeId> GetAdjacentEdges(
        VertexId vertex_id) const {
        return kAdjacentEdges[vertex_id];
    }

    [[nodiscard]] std::size_t VertexCount() const { return kVertices.size(); }

    [[nodiscard]] std::size_t EdgeCount() const { return kEdges.size(); }

private:
    Graph() = delete;

    Graph(std::vector<VertexData> vertices, std::vector<Edge> edges,
          std::vector<std::vector<EdgeId>> adjacent_edges)
        : kVertices(std::move(vertices)),
          kEdges(std::move(edges)),
          kAdjacentEdges(std::move(adjacent_edges)) {}

    const std::vector<VertexData> kVertices;
    const std::vector<Edge> kEdges;
    const std::vector<std::vector<EdgeId>> kAdjacentEdges;

    friend class GraphBuilder<Graph>;
};

/// @brief Builder for immutable graphs.
template <typename G>
class GraphBuilder {
public:
    using VertexData = typename G::VertexData;
    using EdgeData = typename G::EdgeData;
    using Edge = typename G::Edge;

    explicit GraphBuilder(std::size_t vertex_count)
        : vertices_(vertex_count), adjacent_edges_(vertex_count) {}

    void AddEdge(VertexId from, VertexId to, EdgeData data = {}) {
        EdgeId edge_id = edges_.size();
        edges_.emplace_back(Edge{from, to, std::move(data)});
        adjacent_edges_[from].emplace_back(edge_id);
    }

    [[nodiscard]] G Build() {
        return G(std::move(vertices_), std::move(edges_),
                 std::move(adjacent_edges_));
    }

private:
    std::vector<VertexData> vertices_;
    std::vector<Edge> edges_;
    std::vector<std::vector<EdgeId>> adjacent_edges_;
};

template <typename T, typename E>
concept EdgePredicate = requires(const T& pred, const E& edge, EdgeId edge_id) {
    { pred(edge, edge_id) } -> std::convertible_to<bool>;
};

struct AlwaysTrue {
    bool operator()(const auto& /*edge*/, EdgeId /*edge_id*/) const {
        return true;
    }
};

/// @brief Filtered view over a graph's edges.
/// @details Holds references to `graph` and `pred`; both must outlive the view.
template <typename G, typename Pred = AlwaysTrue>
    requires EdgePredicate<Pred, typename G::Edge>
class GraphView {
public:
    using Edge = typename G::Edge;

    GraphView(const G& graph, Pred pred)
        : graph_(graph), pred_(std::move(pred)) {}

    template <typename Fn>
    void ForEachEdgeFrom(VertexId vertex, Fn&& fn) const {
        graph_.ForEachEdgeFrom(vertex, [&](EdgeId edge_id, const Edge& edge) {
            if (pred_(edge, edge_id)) {
                fn(edge_id, edge);
            }
        });
    }

    [[nodiscard]] bool HasEdge(EdgeId edge_id) const {
        return graph_.HasEdge(edge_id) &&
               pred_(graph_.GetEdge(edge_id), edge_id);
    }

    [[nodiscard]] const Edge& GetEdge(EdgeId edge_id) const {
        return graph_.GetEdge(edge_id);
    }

    [[nodiscard]] const auto& GetVertexData(VertexId vertex_id) const {
        return graph_.GetVertexData(vertex_id);
    }

    [[nodiscard]] std::vector<EdgeId> GetAdjacentEdges(
        VertexId vertex_id) const {
        std::vector<EdgeId> result;
        graph_.ForEachEdgeFrom(vertex_id,
                               [&](EdgeId edge_id, const Edge& edge) {
                                   if (pred_(edge, edge_id)) {
                                       result.emplace_back(edge_id);
                                   }
                               });
        return result;
    }

    [[nodiscard]] std::size_t VertexCount() const {
        return graph_.VertexCount();
    }

    [[nodiscard]] std::size_t EdgeCount() const {
        std::size_t count = 0;
        for (VertexId vertex_id = 0; vertex_id < graph_.VertexCount();
             ++vertex_id) {
            count += GetAdjacentEdges(vertex_id).size();
        }
        return count;
    }

private:
    const G& graph_;
    Pred pred_;
};

/// @brief Helper for `GraphView` construction with type deduction.
template <typename G, typename Pred>
auto MakeGraphView(const G& graph, Pred&& pred) {
    return GraphView<G, std::decay_t<Pred>>(graph, std::forward<Pred>(pred));
}

namespace Traverses {

template <typename Vis, typename G>
concept GraphVisitor = requires(Vis&& visitor, VertexId vertex_id,
                                EdgeId edge_id, const typename G::Edge& edge) {
    visitor.DiscoverVertex(vertex_id);
    visitor.ExamineVertex(vertex_id);
    visitor.FinishVertex(vertex_id);
    visitor.ExamineEdge(edge_id, edge);
};

template <typename G>
class NoOpVisitor {
public:
    void DiscoverVertex(VertexId /*vertex_id*/) {}
    void ExamineVertex(VertexId /*vertex_id*/) {}
    void FinishVertex(VertexId /*vertex_id*/) {}
    void ExamineEdge(EdgeId /*edge_id*/, const typename G::Edge& /*edge*/) {}
};

template <typename G, typename Vis = NoOpVisitor<G>>
    requires GraphVisitor<Vis, G>
inline void BreadthFirstSearch(const G& graph, Vis&& visitor, VertexId start) {
    if (start >= graph.VertexCount()) [[unlikely]] {
        return;
    }

    using BoolType = std::uint8_t;

    std::vector<BoolType> visited(graph.VertexCount(), false);
    std::queue<VertexId> queue;
    visited[start] = true;
    queue.emplace(start);
    visitor.DiscoverVertex(start);

    while (!queue.empty()) {
        VertexId vertex = queue.front();
        queue.pop();
        visitor.ExamineVertex(vertex);
        graph.ForEachEdgeFrom(vertex, [&](EdgeId edge_id, const auto& edge) {
            visitor.ExamineEdge(edge_id, edge);
            if (!visited[edge.to]) {
                visited[edge.to] = true;
                queue.emplace(edge.to);
                visitor.DiscoverVertex(edge.to);
            }
        });
        visitor.FinishVertex(vertex);
    }
}

template <typename G, typename Vis = NoOpVisitor<G>>
    requires GraphVisitor<Vis, G>
inline void DepthFirstSearch(const G& graph, Vis&& visitor, VertexId start) {
    if (start >= graph.VertexCount()) {
        return;
    }

    using BoolType = std::uint8_t;
    struct StackFrame {
        VertexId vertex;
        std::size_t edge_idx = 0;
    };

    std::vector<BoolType> visited(graph.VertexCount(), false);
    std::vector<StackFrame> stack;

    visited[start] = true;
    stack.emplace_back(start, 0);
    visitor.DiscoverVertex(start);

    while (!stack.empty()) {
        std::size_t current_frame_index = stack.size() - 1;

        VertexId vertex = stack[current_frame_index].vertex;

        if (stack[current_frame_index].edge_idx == 0) {
            visitor.ExamineVertex(vertex);
        }

        const auto& adjacent_edges = graph.GetAdjacentEdges(vertex);
        if (stack[current_frame_index].edge_idx < adjacent_edges.size()) {
            EdgeId edge_id =
                adjacent_edges[stack[current_frame_index].edge_idx];
            const auto& edge = graph.GetEdge(edge_id);
            visitor.ExamineEdge(edge_id, edge);
            if (!visited[edge.to]) {
                visited[edge.to] = true;
                stack.emplace_back(edge.to, 0);
                visitor.DiscoverVertex(edge.to);
            }
            ++stack[current_frame_index].edge_idx;
        } else {
            visitor.FinishVertex(vertex);
            stack.pop_back();
        }
    }
}
}  // namespace Traverses

namespace FlowNetwork {

struct FlowEdgeData {
    std::int64_t capacity = 0;
};

constexpr std::int32_t kUnvisitedLevel = -1;

using NetworkGraph = Graph<FlowEdgeData>;

class FlowNetworkBuilder {
public:
    explicit FlowNetworkBuilder(std::size_t vertex_count)
        : builder_(vertex_count) {}

    void AddEdge(VertexId from, VertexId to, std::int64_t capacity) {
        builder_.AddEdge(from, to, FlowEdgeData{capacity});
        builder_.AddEdge(to, from, FlowEdgeData{0});
    }

    NetworkGraph Build() { return builder_.Build(); }

private:
    GraphBuilder<NetworkGraph> builder_;
};

class Dinic {
public:
    explicit Dinic(const NetworkGraph& graph, VertexId source, VertexId sink)
        : graph_(graph),
          source_(source),
          sink_(sink),
          level_(graph.VertexCount(), kUnvisitedLevel),
          it_(graph.VertexCount(), 0),
          residual_cap_(graph.EdgeCount(), 0) {
        for (EdgeId edge_id = 0; edge_id < graph_.EdgeCount(); ++edge_id) {
            residual_cap_[edge_id] = graph_.GetEdge(edge_id).data.capacity;
        }
    }

    std::int64_t MaxFlow() {
        if (computed_) {
            return max_flow_;
        }
        std::int64_t flow = 0;
        while (BuildLevelGraph()) {
            std::fill(it_.begin(), it_.end(), 0);
            while (true) {
                std::int64_t pushed = AugmentingDepthFirstSearch(source_, kInf);
                if (pushed == 0) {
                    break;
                }
                flow += pushed;
            }
        }
        max_flow_ = flow;
        computed_ = true;
        return max_flow_;
    }

private:
    static constexpr std::int64_t kInf =
        std::numeric_limits<std::int64_t>::max() / 4;

    /// @brief Builds level graph using BFS and stores vertex levels in
    /// `level_`.
    /// @return `true` if sink is reachable from source in the residual graph,
    /// `false` otherwise.
    bool BuildLevelGraph() {
        if (source_ >= graph_.VertexCount() || sink_ >= graph_.VertexCount()) {
            return false;
        }
        std::fill(level_.begin(), level_.end(), kUnvisitedLevel);
        level_[source_] = 0;
        auto pred = [this](const NetworkGraph::Edge& /*edge*/, EdgeId edge_id) {
            return residual_cap_[edge_id] > 0;
        };
        auto active_edges = MakeGraphView(graph_, pred);

        struct LevelVisitor {
            std::vector<int>& level;
            VertexId current = 0;

            void DiscoverVertex(VertexId /*vertex_id*/) {}
            void ExamineVertex(VertexId vertex_id) { current = vertex_id; }
            void FinishVertex(VertexId /*vertex_id*/) {}
            void ExamineEdge(EdgeId /*edge_id*/,
                             const NetworkGraph::Edge& edge) {
                if (level[edge.to] == kUnvisitedLevel) {
                    level[edge.to] = level[current] + 1;
                }
            }
        };

        Traverses::BreadthFirstSearch(active_edges, LevelVisitor{level_},
                                      source_);
        return level_[sink_] != kUnvisitedLevel;
    }

    // NOTE: Dinic's DFS returns pushed flow and mutates residuals, so it cannot
    // use the generic traversal helper directly. I tried, really I did.
    // The choice here is between code duplication or modifying the canonical
    // DFS API and it will eventually look like it's not DFS at all, so I went
    // with code duplication.

    /// @brief Finds an augmenting path in the level graph using DFS and pushes
    /// flow along it, mutating `residual_cap_` accordingly.
    std::int64_t AugmentingDepthFirstSearch(VertexId vertex,
                                            std::int64_t pushed) {
        if (pushed == 0) {
            return 0;
        }
        if (vertex == sink_) {
            return pushed;
        }
        std::size_t& idx = it_[vertex];
        const auto& adjacent = graph_.GetAdjacentEdges(vertex);
        while (idx < adjacent.size()) {
            EdgeId edge_id = adjacent[idx];
            const auto& edge = graph_.GetEdge(edge_id);
            if (residual_cap_[edge_id] <= 0 ||
                level_[edge.to] != level_[vertex] + 1) {
                ++idx;
                continue;
            }
            std::int64_t pushed_next = AugmentingDepthFirstSearch(
                edge.to, std::min(pushed, residual_cap_[edge_id]));
            if (pushed_next == 0) {
                ++idx;
                continue;
            }

            residual_cap_[edge_id] -= pushed_next;
            residual_cap_[edge_id ^ 1] += pushed_next;
            return pushed_next;
        }
        return 0;
    }

    const NetworkGraph& graph_;
    VertexId source_;
    VertexId sink_;
    std::vector<std::int32_t> level_;
    std::vector<std::size_t> it_;
    std::vector<std::int64_t> residual_cap_;
    std::int64_t max_flow_ = 0;
    bool computed_ = false;
};

}  // namespace FlowNetwork

}  // namespace Grafcik
