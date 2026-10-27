// The emotion graph: expressions are nodes, edges are the transitions that
// look natural. To go from one face to another the engine walks the shortest
// path, so a furious robot calms down step by step before it smiles.
#ifndef FACE_EXPRESSIONGRAPH_H
#define FACE_EXPRESSIONGRAPH_H

#include "../core/MathUtil.h"
#include "FaceTypes.h"

struct GraphEdge {
  uint8_t a, b;
  uint16_t ms;   // duration of the transition (also its cost)
  uint8_t easing;  // Ease
};

#define GRAPH_MAX_PATH 12

namespace ExpressionGraph {

int edgeCount();
const GraphEdge &edge(int i);

// Edge between two nodes, or nullptr when they are not connected.
const GraphEdge *find(uint8_t a, uint8_t b);

// Fills `out` with the nodes to visit after `from` (so `from` is not included
// and `to` is the last one). Returns the number of hops, 0 if from == to.
int path(uint8_t from, uint8_t to, uint8_t *out, int maxLen);

// Neighbours of a node. Returns how many were written.
int neighbours(uint8_t node, uint8_t *out, int maxLen);

// Hub expression of a mood.
uint8_t hub(uint8_t mood);

}  // namespace ExpressionGraph

#endif
