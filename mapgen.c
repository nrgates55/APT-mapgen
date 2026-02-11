#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAP_W 80
#define MAP_H 21

typedef struct {
  int x, y;
} Point;

static char map[MAP_H][MAP_W];

static int is_border(int x, int y) {
  return x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1;
}

static int rand_range(int lo, int hi) { // inclusive
  return lo + rand() % (hi - lo + 1);
}

static void init_map(void) {
  for (int y = 0; y < MAP_H; y++) {
    for (int x = 0; x < MAP_W; x++) {
      map[y][x] = ' '; // unset
    }
  }
}

static void print_map(void) {
  for (int y = 0; y < MAP_H; y++) {
    for (int x = 0; x < MAP_W; x++) {
      putchar(map[y][x]);
    }
    putchar('\n');
  }
}

static void make_border_and_exits(Point *top, Point *bottom, Point *left, Point *right) {
  // Fill border with boulders
  for (int y = 0; y < MAP_H; y++) {
    for (int x = 0; x < MAP_W; x++) {
      if (is_border(x, y)) map[y][x] = '%';
    }
  }

  // Choose exits (avoid corners)
  *top    = (Point){ rand_range(1, MAP_W - 2), 0 };
  *bottom = (Point){ rand_range(1, MAP_W - 2), MAP_H - 1 };
  *left   = (Point){ 0, rand_range(1, MAP_H - 2) };
  *right  = (Point){ MAP_W - 1, rand_range(1, MAP_H - 2) };

  // Mark exits as path
  map[top->y][top->x] = '#';
  map[bottom->y][bottom->x] = '#';
  map[left->y][left->x] = '#';
  map[right->y][right->x] = '#';
}

static void fill_interior(char c) {
  for (int y = 1; y < MAP_H - 1; y++) {
    for (int x = 1; x < MAP_W - 1; x++) {
      map[y][x] = c;
    }
  }
}

// Random-walk blob painter (easy and looks decent)
static void paint_blob(char terrain, int steps) {
  int x = rand_range(1, MAP_W - 2);
  int y = rand_range(1, MAP_H - 2);

  for (int i = 0; i < steps; i++) {
    if (!is_border(x, y) && map[y][x] != '#') {
      map[y][x] = terrain;
    }

    int dir = rand() % 4;
    int nx = x, ny = y;
    if (dir == 0) nx++;
    else if (dir == 1) nx--;
    else if (dir == 2) ny++;
    else ny--;

    // clamp to interior
    if (nx < 1) nx = 1;
    if (nx > MAP_W - 2) nx = MAP_W - 2;
    if (ny < 1) ny = 1;
    if (ny > MAP_H - 2) ny = MAP_H - 2;

    x = nx; y = ny;
  }
}

// helper
static int sign(int v) { return (v > 0) - (v < 0); }

/*
 * Directed carve that always moves one step on the dominant axis toward goal,
 * and has a small chance to wiggle on the secondary axis.
 *
 * wiggle_pct: total percent chance reserved for wiggle (e.g. 20 means 10% one way + 10% the other).
 *             Use small values (0..30). 0 => perfectly Manhattan path.
 */
static void carve_path(Point start, Point goal) {
  int x = start.x, y = start.y;

  /* prevent infinite loops if start==goal */
  if (x == goal.x && y == goal.y) {
    map[y][x] = '#';
    return;
  }

  while (x != goal.x || y != goal.y) {
    /* paint current cell as road (don't overwrite exits check is optional) */
    map[y][x] = '#';

    int dx = goal.x - x;
    int dy = goal.y - y;
    int adx = abs(dx);
    int ady = abs(dy);

    int step_x = 0, step_y = 0;

    /* Primary: always move one step along the dominant axis toward the goal */
    if (ady >= adx) {
      step_y = sign(dy);
    } else {
      step_x = sign(dx);
    }

    /* Secondary wiggle logic:
     * We'll give wiggle_pct total chance. If primary is Y, wiggle affects X:
     *   - wiggle_pct/2 : step_x = -1
     *   - wiggle_pct/2 : step_x = +1
     *
     * If primary is X, wiggle affects Y similarly.
     *
     * Example: wiggle_pct = 20 -> 10% left, 10% right
     */
    const int wiggle_pct = 20; /* adjust to taste (20 == 10% left + 10% right) */
    int r = rand() % 100;
    if (r < wiggle_pct) {
      int half = wiggle_pct / 2;
      int w = (r < half) ? -1 : 1;

      if (step_y != 0) {     /* primary was Y -> wiggle X */
        step_x += w;
      } else {               /* primary was X -> wiggle Y */
        step_y += w;
      }
    }

    /* Candidate new position */
    int nx = x + step_x;
    int ny = y + step_y;

    /* Validate candidate:
     * - must be inside map bounds [0 .. MAP_W-1], [0 .. MAP_H-1]
     * - must not create a new hole in the border (we only allow stepping on border if it's the goal
     *   OR if the tile already contains '#' (an exit), matching your old guard)
     *
     * If the wiggle made the candidate invalid, revert to the primary-only step.
     */
    int candidate_ok = 1;
    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) candidate_ok = 0;
    else if (is_border(nx, ny) && !(nx == goal.x && ny == goal.y) && map[ny][nx] != '#')
      candidate_ok = 0;

    if (!candidate_ok) {
      /* revert wiggle: recompute primary-only candidate */
      step_x = 0; step_y = 0;
      if (ady >= adx) step_y = sign(dy);
      else step_x = sign(dx);

      nx = x + step_x;
      ny = y + step_y;

      /* extra safety: if the primary-only candidate is somehow invalid (rare),
         try to move only along whichever axis still differs and is valid */
      if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H ||
          (is_border(nx, ny) && !(nx == goal.x && ny == goal.y) && map[ny][nx] != '#')) {
        /* attempt alternate single-axis moves (should avoid infinite loops) */
        int tried = 0;
        if (step_y != 0) {
          /* try moving on x toward goal if possible */
          int altx = x + sign(dx);
          if (!(altx < 0 || altx >= MAP_W || (is_border(altx,y) && !(altx==goal.x && y==goal.y) && map[y][altx] != '#'))) {
            nx = altx; ny = y; tried = 1;
          }
        } else if (step_x != 0) {
          /* try moving on y toward goal if possible */
          int alty = y + sign(dy);
          if (!(alty < 0 || alty >= MAP_H || (is_border(x,alty) && !(x==goal.x && alty==goal.y) && map[alty][x] != '#'))) {
            nx = x; ny = alty; tried = 1;
          }
        }
        if (!tried) {
          /* last resort: break to avoid infinite loop (shouldn't happen) */
          break;
        }
      }
    }

    /* apply step */
    x = nx; y = ny;
  }

  /* paint final goal cell as road */
  map[goal.y][goal.x] = '#';
}

static int can_place_building_2x2(int x, int y) {
  // x,y is top-left of 2x2 footprint
  if (x < 1 || y < 1 || x + 1 > MAP_W - 2 || y + 1 > MAP_H - 2) return 0;

  for (int dy = 0; dy < 2; dy++) {
    for (int dx = 0; dx < 2; dx++) {
      char t = map[y + dy][x + dx];
      if (t == '%' || t == '#') return 0;
    }
  }
  return 1;
}

static void clear_ring_to_dot(int x, int y) {
  // clear footprint + 1-tile ring to '.' (except roads)
  for (int yy = y - 1; yy <= y + 2; yy++) {
    for (int xx = x - 1; xx <= x + 2; xx++) {
      if (xx < 1 || yy < 1 || xx > MAP_W - 2 || yy > MAP_H - 2) continue;
      if (map[yy][xx] != '#') map[yy][xx] = '.';
    }
  }
}

static void place_building_near_road(char b) {
  for (int tries = 0; tries < 8000; tries++) {
    int rx = rand_range(1, MAP_W - 2);
    int ry = rand_range(1, MAP_H - 2);
    if (map[ry][rx] != '#') continue;

    // Try a few candidate top-left positions around this road tile
    const int cand[8][2] = {
      {rx - 2, ry - 1}, {rx + 1, ry - 1},
      {rx - 2, ry},     {rx + 1, ry},
      {rx - 1, ry - 2}, {rx,     ry - 2},
      {rx - 1, ry + 1}, {rx,     ry + 1}
    };

    for (int i = 0; i < 8; i++) {
      int x = cand[i][0];
      int y = cand[i][1];
      if (!can_place_building_2x2(x, y)) continue;

      // Ensure no tall grass is needed to reach it: make local area clearing
      clear_ring_to_dot(x, y);

      // Place 2x2 building
      map[y][x] = b;     map[y][x + 1] = b;
      map[y + 1][x] = b; map[y + 1][x + 1] = b;
      return;
    }
  }

  // If we ever failed (unlikely), do nothing; but you can treat this as an error if you want.
}

int main(int argc, char **argv) {
  unsigned seed = (unsigned) time(NULL);
  if (argc >= 2) {
    seed = (unsigned) strtoul(argv[1], NULL, 10); // reproducible seed
  }
  srand(seed);

  init_map();

  Point top, bottom, left, right;
  make_border_and_exits(&top, &bottom, &left, &right);

  // Base terrain: clearing
  fill_interior('.');

  // Required regions (>=2 tall grass, >=2 clearing, >=1 water)
  // Clearing is already everywhere; still add two tall-grass blobs + one water blob.
  paint_blob(':', 260);
  paint_blob(':', 260);
  paint_blob('~', 170);

  // Pick an intersection point (interior)
  Point inter = { rand_range(10, MAP_W - 11), rand_range(5, MAP_H - 6) };

  // Required paths: N-S and E-W that intersect
  carve_path(top, inter);
  carve_path(inter, bottom);
  carve_path(left, inter);
  carve_path(inter, right);

  // Place buildings near roads and ensure local reachability without tall grass
  place_building_near_road('C');
  place_building_near_road('M');

  print_map();
  return 0;
}
