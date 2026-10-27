#include "ExpressionGraph.h"

#define CALM EASE_IN_OUT
#define SNAP EASE_OUT
#define BOING EASE_OUT_BACK

static const GraphEdge EDGES[] = {
  // ---- hubs (the dashed lines of the map): slow, direct mood changes
  {EX_NORMAL, EX_HAPPY, 380, BOING},
  {EX_NORMAL, EX_SAD, 650, CALM},
  {EX_NORMAL, EX_ANGRY, 520, CALM},
  {EX_NORMAL, EX_SURPRISED, 150, BOING},
  {EX_NORMAL, EX_SLEEPY, 800, CALM},

  // ---- calm cluster
  {EX_NORMAL, EX_CURIOUS, 300, BOING},
  {EX_NORMAL, EX_FOCUSED, 280, CALM},
  {EX_NORMAL, EX_SKEPTIC, 320, CALM},
  {EX_NORMAL, EX_SQUINT, 260, CALM},
  {EX_NORMAL, EX_UNIMPRESSED, 420, CALM},
  {EX_NORMAL, EX_CONFUSED, 340, BOING},
  {EX_NORMAL, EX_OVAL, 260, CALM},
  {EX_CURIOUS, EX_CONFUSED, 300, CALM},
  {EX_CURIOUS, EX_FOCUSED, 280, CALM},
  {EX_FOCUSED, EX_SQUINT, 240, CALM},
  {EX_SKEPTIC, EX_SUSPICIOUS, 320, CALM},
  {EX_SKEPTIC, EX_UNIMPRESSED, 320, CALM},
  {EX_SUSPICIOUS, EX_SQUINT, 240, CALM},
  {EX_UNIMPRESSED, EX_BORED, 500, CALM},

  // ---- happy cluster
  {EX_HAPPY, EX_GLEE, 260, BOING},
  {EX_HAPPY, EX_EXCITED, 260, BOING},
  {EX_HAPPY, EX_WINK, 200, SNAP},
  {EX_HAPPY, EX_PROUD, 340, CALM},
  {EX_HAPPY, EX_SHY, 380, CALM},
  {EX_HAPPY, EX_SINGING, 320, CALM},
  {EX_GLEE, EX_LAUGH, 220, BOING},
  {EX_GLEE, EX_EXCITED, 240, BOING},
  {EX_EXCITED, EX_AWE, 280, BOING},
  {EX_AWE, EX_LOVE, 320, BOING},
  {EX_SHY, EX_LOVE, 360, CALM},
  {EX_CURIOUS, EX_EXCITED, 300, BOING},
  {EX_CURIOUS, EX_AWE, 320, BOING},

  // ---- sad cluster
  {EX_NORMAL, EX_WORRIED, 400, CALM},
  {EX_WORRIED, EX_SAD, 450, CALM},
  {EX_SAD, EX_LONELY, 600, CALM},
  {EX_SAD, EX_PLEADING, 380, CALM},
  {EX_SAD, EX_CRYING, 420, CALM},
  {EX_SAD, EX_GUILTY, 420, CALM},
  {EX_PLEADING, EX_CRYING, 380, CALM},
  {EX_GUILTY, EX_SHY, 400, CALM},
  {EX_LONELY, EX_BORED, 550, CALM},
  {EX_UNIMPRESSED, EX_LONELY, 550, CALM},

  // ---- angry cluster
  {EX_NORMAL, EX_ANNOYED, 340, CALM},
  {EX_SKEPTIC, EX_ANNOYED, 300, CALM},
  {EX_SUSPICIOUS, EX_ANNOYED, 300, CALM},
  {EX_ANNOYED, EX_ANGRY, 300, SNAP},
  {EX_ANNOYED, EX_FRUSTRATED, 320, CALM},
  {EX_FRUSTRATED, EX_ANGRY, 300, SNAP},
  {EX_FRUSTRATED, EX_SAD, 500, CALM},
  {EX_ANGRY, EX_FURIOUS, 240, BOING},

  // ---- surprise / fear cluster
  {EX_SURPRISED, EX_SCARED, 220, SNAP},
  {EX_SURPRISED, EX_AWE, 300, CALM},
  {EX_SURPRISED, EX_CONFUSED, 340, CALM},
  {EX_SURPRISED, EX_CURIOUS, 320, CALM},
  {EX_SURPRISED, EX_DIZZY, 300, CALM},
  {EX_SCARED, EX_WORRIED, 400, CALM},
  {EX_CONFUSED, EX_DIZZY, 340, CALM},
  {EX_DIZZY, EX_KO, 300, CALM},

  // ---- sleep cluster
  {EX_BORED, EX_SLEEPY, 700, CALM},
  {EX_UNIMPRESSED, EX_SLEEPY, 650, CALM},
  {EX_SLEEPY, EX_YAWN, 500, CALM},
  {EX_SLEEPY, EX_ASLEEP, 900, CALM},
  {EX_YAWN, EX_ASLEEP, 800, CALM},

  // ---- second wave of faces
  {EX_NORMAL, EX_ATTENTIVE, 260, CALM},
  {EX_ATTENTIVE, EX_FOCUSED, 260, CALM},
  {EX_ATTENTIVE, EX_LISTENING, 240, CALM},
  {EX_ATTENTIVE, EX_CURIOUS, 280, CALM},
  {EX_LISTENING, EX_CURIOUS, 280, CALM},
  {EX_NORMAL, EX_THINKING, 320, CALM},
  {EX_THINKING, EX_CONFUSED, 320, CALM},
  {EX_THINKING, EX_SKEPTIC, 320, CALM},
  {EX_NORMAL, EX_DETERMINED, 300, CALM},
  {EX_FOCUSED, EX_DETERMINED, 260, CALM},
  {EX_DETERMINED, EX_ANGRY, 300, SNAP},
  {EX_HAPPY, EX_SMUG, 320, CALM},
  {EX_SMUG, EX_PROUD, 300, CALM},
  {EX_SMUG, EX_SKEPTIC, 320, CALM},
  {EX_SMUG, EX_MISCHIEF, 280, CALM},
  {EX_MISCHIEF, EX_GLEE, 280, BOING},
  {EX_MISCHIEF, EX_ANGRY, 340, CALM},
  {EX_AWE, EX_STARSTRUCK, 300, BOING},
  {EX_STARSTRUCK, EX_EXCITED, 300, BOING},
  {EX_STARSTRUCK, EX_LOVE, 320, BOING},
  {EX_WINK, EX_CHEEKY, 220, BOING},
  {EX_CHEEKY, EX_LAUGH, 260, BOING},
  {EX_CHEEKY, EX_HAPPY, 280, CALM},
  {EX_HAPPY, EX_RELIEVED, 380, CALM},
  {EX_RELIEVED, EX_WORRIED, 420, CALM},
  {EX_SAD, EX_DESPAIR, 450, CALM},
  {EX_DESPAIR, EX_CRYING, 380, CALM},
  {EX_SHY, EX_EMBARRASSED, 320, CALM},
  {EX_EMBARRASSED, EX_GUILTY, 360, CALM},
  {EX_EMBARRASSED, EX_WORRIED, 380, CALM},
  {EX_ANNOYED, EX_DISGUST, 300, CALM},
  {EX_DISGUST, EX_SKEPTIC, 320, CALM},
  {EX_DISGUST, EX_FRUSTRATED, 300, CALM},
  {EX_ANGRY, EX_FERAL, 260, BOING},
  {EX_FERAL, EX_FURIOUS, 260, SNAP},
  {EX_SCARED, EX_HORRIFIED, 200, SNAP},
  {EX_SURPRISED, EX_HORRIFIED, 220, SNAP},
  {EX_CONFUSED, EX_CROSSEYED, 300, CALM},
  {EX_CROSSEYED, EX_DIZZY, 300, CALM},
  {EX_DIZZY, EX_HYPNO, 320, CALM},
  {EX_SLEEPY, EX_TIRED, 500, CALM},
  {EX_TIRED, EX_BORED, 500, CALM},
  {EX_TIRED, EX_YAWN, 450, CALM},

  // ---- body feelings
  {EX_NORMAL, EX_HOT, 600, CALM},
  {EX_HOT, EX_SLEEPY, 700, CALM},
  {EX_HOT, EX_FRUSTRATED, 450, CALM},
  {EX_NORMAL, EX_COLD, 500, CALM},
  {EX_COLD, EX_SCARED, 350, CALM},
  {EX_COLD, EX_WORRIED, 450, CALM},
};

static const int EDGE_COUNT = (int)(sizeof(EDGES) / sizeof(EDGES[0]));

static const uint8_t HUBS[MOOD_COUNT] = {EX_NORMAL, EX_HAPPY, EX_SAD, EX_ANGRY, EX_SURPRISED, EX_SLEEPY};

namespace ExpressionGraph {

int edgeCount() { return EDGE_COUNT; }

const GraphEdge &edge(int i) { return EDGES[i]; }

const GraphEdge *find(uint8_t a, uint8_t b) {
  for (int i = 0; i < EDGE_COUNT; i++) {
    if ((EDGES[i].a == a && EDGES[i].b == b) || (EDGES[i].a == b && EDGES[i].b == a)) return &EDGES[i];
  }
  return nullptr;
}

// Dijkstra over a handful of nodes; runs only when a new target is requested.
int path(uint8_t from, uint8_t to, uint8_t *out, int maxLen) {
  if (from >= EX_COUNT || to >= EX_COUNT || from == to) return 0;

  uint32_t dist[EX_COUNT];
  uint8_t prev[EX_COUNT];
  bool done[EX_COUNT];
  for (int i = 0; i < EX_COUNT; i++) {
    dist[i] = 0xFFFFFFFFu;
    prev[i] = EX_NONE;
    done[i] = false;
  }
  dist[from] = 0;

  for (;;) {
    int u = -1;
    uint32_t best = 0xFFFFFFFFu;
    for (int i = 0; i < EX_COUNT; i++) {
      if (!done[i] && dist[i] < best) {
        best = dist[i];
        u = i;
      }
    }
    if (u < 0 || u == to) break;
    done[u] = true;
    for (int i = 0; i < EDGE_COUNT; i++) {
      int v = -1;
      if (EDGES[i].a == u) v = EDGES[i].b;
      else if (EDGES[i].b == u) v = EDGES[i].a;
      if (v < 0 || done[v]) continue;
      // Every hop also costs a little, so short chains win over long ones.
      uint32_t nd = dist[u] + EDGES[i].ms + 120;
      if (nd < dist[v]) {
        dist[v] = nd;
        prev[v] = (uint8_t)u;
      }
    }
  }

  if (prev[to] == EX_NONE) {
    // Not reachable (should not happen): jump straight to the target.
    out[0] = to;
    return 1;
  }

  uint8_t rev[EX_COUNT];
  int n = 0;
  for (uint8_t v = to; v != from && n < EX_COUNT; v = prev[v]) rev[n++] = v;
  if (n > maxLen) {
    // Keep the tail of the route, the first steps are skipped.
    n = maxLen;
  }
  for (int i = 0; i < n; i++) out[i] = rev[n - 1 - i];
  return n;
}

int neighbours(uint8_t node, uint8_t *out, int maxLen) {
  int n = 0;
  for (int i = 0; i < EDGE_COUNT && n < maxLen; i++) {
    if (EDGES[i].a == node) out[n++] = EDGES[i].b;
    else if (EDGES[i].b == node) out[n++] = EDGES[i].a;
  }
  return n;
}

uint8_t hub(uint8_t mood) { return mood < MOOD_COUNT ? HUBS[mood] : (uint8_t)EX_NORMAL; }

}  // namespace ExpressionGraph
