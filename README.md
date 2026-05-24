# Administrative Grid

A C++17 SDL2 + OpenGL prototype for a top-down abstract logistics/city-builder. SDL2 handles the window and input; OpenGL handles batched grid rendering, shaders, smooth zoom, post-processing, fog, overlays, resource marks, and night lighting.

## Build And Run

```sh
cd "/Users/user/Documents/Nested Nav"
cmake -S . -B build
cmake --build build
./build/neon_civic
```

The first configure may download/build SDL2 if it is not installed.

## Controls

- `WASD` / arrow keys: pan camera
- Mouse wheel: smooth zoom
- Left mouse: paint selected tool
- Click a construction card: select that tool
- Left mouse drag with road/conveyor/rail: ghost and auto-orient a route path
- Right mouse drag: pan
- `Q` / `E`: rotate placement direction for roads, conveyors, and rails
- `Tab`: cycle analysis view (normal, stock, flow, stress)
- `1`: road
- `2`: conveyor
- `3`: rail
- `4`: block
- `5`: holding
- `6`: mine
- `7`: factory
- `8`: refinery
- `9`: port
- `0`: erase
- `Space`: pause simulation
- `S`: save current grid
- `L`: load saved grid
- `F`: toggle fog overlay
- `P`: toggle pollution overlay
- `N`: force night/day blend
- `R`: regenerate map
- `Esc`: quit

## Prototype Features

- Vast empty administrative grid
- Sparse logistics economy: blocks produce pink material, mines create fuel, factories make gold, refineries create export material, and ports sell it through rail access
- Roads, conveyors, rails, holding structures, production buildings, and ports
- Clickable construction cards with hover/selected states; UI clicks no longer place buildings behind panels
- Directional conveyors and rails with visible flow arrows
- Directional building outputs: structures face a direction and keep material when that output is blocked
- Hover inspector with building states such as running, starved, output blocked, no rail export, and storage full
- Construction costs, running upkeep, export income, cash pressure, and insufficient-funds alerts
- First quota: export 50 refined material while keeping stability above 60% and variance below 35%
- Audit alerts for quota completion or severe instability
- Save/load for grid layout, directions, stored resources, cash, quota state, and audit state
- Build preview with valid/invalid site coloring, output-direction preview, cost checks, and safer placement rules to prevent dragging routes over production buildings
- Smarter route placement with drag ghosting, inferred direction, and pre-click invalid-site feedback
- Right-side directive checklist that introduces the production chain: block, mine, factory, refinery, port export
- Throughput visualization with animated material pulses, blocked-flow marks, route flow readouts, and ledger totals for network flow and blocked output
- Moving resource packets on roads, conveyors, rails, and active structures, colored by material type: pink input, black fuel, gold output, and pale refined export
- Building state visuals: active structures pulse, holdings visibly fill, blocked/starved buildings show warning marks, and production buildings gain small detail as activity/stock rises
- Analysis map views for stored resources, moving throughput, blocked routes, pollution, and starved/blocked buildings
- Compact overview map showing route layout, active flow, blocked cells, resources, and the current camera footprint
- Recipe and operations readability: production buildings show input requirement marks, output ports, hover recipes, and an operations summary for running/starved/blocked/no-rail/full structures
- Directive checklist and milestone alerts for pink flow, fuel conversion, gold output, refined material, and first export
- Higher-fidelity rendering pass with tile scuffs, route shadows, bevels, richer production silhouettes, output ports, invalid-placement labels, and subtle fluorescent flicker
- Right-side metrics for profit, stability, purity, variance, and export count
- Pollution/variance pressure from production and stockpiling
- Detailed procedural OpenGL texture atlas: cracked concrete slabs, patched asphalt, ribbed conveyor rubber, rail sleepers and ballast, packed pink material, riveted holding metal, fractured carbon, factory plate, refinery brass and pipes, port slabs, brushed UI panels, ingot marks, refined foil, and irregular staining
- Detailed map style with textured resource strips, rail marks, conveyors, structure silhouettes, and surface wear
- Smooth camera pan/zoom
- Day/night lighting
- Fog and pollution overlays
- Restrained full-screen post-processing pass with grain, softer vignette, desaturated color, and reduced shine
- Minimal generated UI/text, no asset dependency
