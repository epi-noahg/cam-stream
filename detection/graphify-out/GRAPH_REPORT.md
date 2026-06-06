# Graph Report - /Users/noahg/github/cam-stream/detection  (2026-06-06)

## Corpus Check
- 23 files · ~18,689 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 103 nodes · 160 edges · 14 communities detected
- Extraction: 67% EXTRACTED · 33% INFERRED · 0% AMBIGUOUS · INFERRED: 53 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]

## God Nodes (most connected - your core abstractions)
1. `main()` - 25 edges
2. `composite()` - 11 edges
3. `processFrame()` - 9 edges
4. `feedFrame()` - 7 edges
5. `confirm()` - 7 edges
6. `drawCamTile()` - 7 edges
7. `main()` - 5 edges
8. `main()` - 4 edges
9. `maybeAutoReset()` - 4 edges
10. `reset()` - 4 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `seek()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → sources/FileSource.cpp
- `main()` --calls--> `feedFrame()`  [INFERRED]
  tools/detect_offline.cpp → /Users/noahg/github/cam-stream/detection/src/Pipeline.cpp
- `main()` --calls--> `next()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → sources/FileSource.cpp
- `processFrame()` --calls--> `lookup()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/src/DartDetector.cpp → src/ZoneMapper.cpp
- `boardToImage()` --calls--> `drawCalibrationOverlay()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/src/BoardCalibration.cpp → src/Renderer.cpp

## Hyperedges (group relationships)
- **** — cmakelists_camdetect_calibrate, cmakelists_camdetect_offline, cmakelists_camdetect_debug, cmakelists_camdetect [EXTRACTED 1.00]

## Communities

### Community 0 - "Community 0"
Cohesion: 0.18
Nodes (14): main(), consumeBgRefreshRequest(), consumeResetRequest(), consumeStepBackward(), consumeStepForward(), handleClick(), isPaused(), onMouseStatic() (+6 more)

### Community 1 - "Community 1"
Cohesion: 0.27
Nodes (13): blitAspectFit(), composite(), drawCamTile(), fusedCentroid(), pairwiseSpread(), boardToCanonicalPx(), drawCalibrationOverlay(), drawCircleBoardMM() (+5 more)

### Community 2 - "Community 2"
Cohesion: 0.24
Nodes (11): boardLooksCleared(), buildRoiMask(), DartDetector(), labDistance(), lineExtendByMask(), processFrame(), refreshBackground(), reset() (+3 more)

### Community 3 - "Community 3"
Cohesion: 0.27
Nodes (9): addHit(), confirm(), dist(), flush(), MultiCamFusion(), reset(), tick(), feedFrame() (+1 more)

### Community 4 - "Community 4"
Cohesion: 0.2
Nodes (6): boardToImage(), imageToBoard(), saveToFile(), fromReferencePoints(), drawHud(), main()

### Community 5 - "Community 5"
Cohesion: 0.18
Nodes (7): camViz(), dartsInRound(), diffThreshold(), lineMergePerpPx(), Pipeline(), roundHits(), setLineMergePerpPx()

### Community 6 - "Community 6"
Cohesion: 0.22
Nodes (6): loadFromFile(), main(), FileSource, next(), seek(), setOnHit()

### Community 7 - "Community 7"
Cohesion: 0.67
Nodes (1): DebugUI()

### Community 8 - "Community 8"
Cohesion: 0.67
Nodes (1): FrameSource

### Community 9 - "Community 9"
Cohesion: 1.0
Nodes (1): ZoneMapper

### Community 10 - "Community 10"
Cohesion: 1.0
Nodes (1): Renderer

### Community 11 - "Community 11"
Cohesion: 1.0
Nodes (1): BoardCalibrator

### Community 12 - "Community 12"
Cohesion: 1.0
Nodes (0): 

### Community 13 - "Community 13"
Cohesion: 1.0
Nodes (0): 

## Knowledge Gaps
- **3 isolated node(s):** `ZoneMapper`, `Renderer`, `BoardCalibrator`
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 9`** (2 nodes): `ZoneMapper.hpp`, `ZoneMapper`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 10`** (2 nodes): `Renderer.hpp`, `Renderer`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 11`** (2 nodes): `BoardCalibrator`, `BoardCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 12`** (1 nodes): `BoardCalibration.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 13`** (1 nodes): `Types.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Community 0` to `Community 2`, `Community 3`, `Community 5`, `Community 6`?**
  _High betweenness centrality (0.408) - this node is a cross-community bridge._
- **Why does `feedFrame()` connect `Community 3` to `Community 0`, `Community 2`, `Community 5`, `Community 6`?**
  _High betweenness centrality (0.200) - this node is a cross-community bridge._
- **Why does `processFrame()` connect `Community 2` to `Community 3`, `Community 4`?**
  _High betweenness centrality (0.122) - this node is a cross-community bridge._
- **Are the 24 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `setOnHit()`) actually correct?**
  _`main()` has 24 INFERRED edges - model-reasoned connections that need verification._
- **Are the 6 inferred relationships involving `composite()` (e.g. with `renderCanonicalBoard()` and `drawHitOnCanonical()`) actually correct?**
  _`composite()` has 6 INFERRED edges - model-reasoned connections that need verification._
- **Are the 3 inferred relationships involving `processFrame()` (e.g. with `feedFrame()` and `imageToBoard()`) actually correct?**
  _`processFrame()` has 3 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `feedFrame()` (e.g. with `main()` and `main()`) actually correct?**
  _`feedFrame()` has 5 INFERRED edges - model-reasoned connections that need verification._