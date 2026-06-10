# Graph Report - /Users/noahg/github/cam-stream/detection  (2026-06-11)

## Corpus Check
- 29 files · ~71,332 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 145 nodes · 251 edges · 20 communities detected
- Extraction: 65% EXTRACTED · 35% INFERRED · 0% AMBIGUOUS · INFERRED: 88 edges (avg confidence: 0.8)
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
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]

## God Nodes (most connected - your core abstractions)
1. `main()` - 29 edges
2. `main()` - 23 edges
3. `feedFrame()` - 11 edges
4. `composite()` - 11 edges
5. `processFrame()` - 10 edges
6. `run()` - 9 edges
7. `main()` - 7 edges
8. `confirm()` - 7 edges
9. `drawCamTile()` - 7 edges
10. `companionPath()` - 6 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `seek()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → sources/FileSource.cpp
- `main()` --calls--> `next()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → sources/FileSource.cpp
- `main()` --calls--> `fromReferencePoints()`  [INFERRED]
  tools/calibrate.cpp → /Users/noahg/github/cam-stream/detection/src/BoardCalibrator.cpp
- `processFrame()` --calls--> `lookup()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/src/DartDetector.cpp → src/ZoneMapper.cpp
- `boardToImage()` --calls--> `drawCalibrationOverlay()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/src/BoardCalibration.cpp → src/Renderer.cpp

## Hyperedges (group relationships)
- **** — cmakelists_camdetect_calibrate, cmakelists_camdetect_offline, cmakelists_camdetect_debug, cmakelists_camdetect [EXTRACTED 1.00]

## Communities

### Community 0 - "Community 0"
Cohesion: 0.19
Nodes (9): main(), render(), saveAll(), companionPath(), idColor(), idToResult(), lookup(), overlay() (+1 more)

### Community 1 - "Community 1"
Cohesion: 0.18
Nodes (13): addHit(), confirm(), dist(), flush(), MultiCamFusion(), reset(), tick(), feedFrame() (+5 more)

### Community 2 - "Community 2"
Cohesion: 0.23
Nodes (12): main(), consumeStepBackward(), consumeStepForward(), handleClick(), isPaused(), onMouseStatic(), pairwiseSpread(), setCamDelay() (+4 more)

### Community 3 - "Community 3"
Cohesion: 0.25
Nodes (12): boardLooksCleared(), buildRoiMask(), dartAxisByMidpoints(), DartDetector(), extendBboxAlongAxis(), fillDartRegion(), labDistance(), processFrame() (+4 more)

### Community 4 - "Community 4"
Cohesion: 0.27
Nodes (13): blitAspectFit(), composite(), drawCamTile(), fusedCentroid(), render(), boardToCanonicalPx(), drawCalibrationOverlay(), drawCircleBoardMM() (+5 more)

### Community 5 - "Community 5"
Cohesion: 0.17
Nodes (9): camViz(), computeRoundStatus_(), diffThreshold(), lineMergePerpPx(), Pipeline(), resetRound(), roundHits(), roundStatus() (+1 more)

### Community 6 - "Community 6"
Cohesion: 0.24
Nodes (8): consumeBgRefreshRequest(), consumeResetRequest(), setRoundProgress(), isNumeric(), main(), printUsage(), dartsInRound(), setOnHit()

### Community 7 - "Community 7"
Cohesion: 0.24
Nodes (6): boardToImage(), imageToBoard(), loadFromFile(), saveToFile(), drawHud(), main()

### Community 8 - "Community 8"
Cohesion: 0.31
Nodes (8): angDiff(), blobMask(), boardPt(), collectBlobs(), run(), toNormalized(), detectAuto(), fromReferencePoints()

### Community 9 - "Community 9"
Cohesion: 0.25
Nodes (5): setZoneMap(), main(), FileSource, next(), seek()

### Community 10 - "Community 10"
Cohesion: 0.67
Nodes (1): FrameSource

### Community 11 - "Community 11"
Cohesion: 0.67
Nodes (1): DebugUI()

### Community 12 - "Community 12"
Cohesion: 1.0
Nodes (1): ZoneMapper

### Community 13 - "Community 13"
Cohesion: 1.0
Nodes (1): Renderer

### Community 14 - "Community 14"
Cohesion: 1.0
Nodes (1): BoardCalibrator

### Community 15 - "Community 15"
Cohesion: 1.0
Nodes (1): AutoCalibrator

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (1): ZoneMap

### Community 17 - "Community 17"
Cohesion: 1.0
Nodes (0): 

### Community 18 - "Community 18"
Cohesion: 1.0
Nodes (0): 

### Community 19 - "Community 19"
Cohesion: 1.0
Nodes (0): 

## Knowledge Gaps
- **5 isolated node(s):** `ZoneMapper`, `Renderer`, `BoardCalibrator`, `AutoCalibrator`, `ZoneMap`
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 12`** (2 nodes): `ZoneMapper.hpp`, `ZoneMapper`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 13`** (2 nodes): `Renderer.hpp`, `Renderer`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 14`** (2 nodes): `BoardCalibrator`, `BoardCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (2 nodes): `AutoCalibrator`, `AutoCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (2 nodes): `ZoneMap.hpp`, `ZoneMap`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (2 nodes): `onSeekTrackbar()`, `debug_viewer.cpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (1 nodes): `BoardCalibration.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 19`** (1 nodes): `Types.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Community 2` to `Community 0`, `Community 1`, `Community 3`, `Community 4`, `Community 5`, `Community 6`, `Community 7`, `Community 9`, `Community 17`?**
  _High betweenness centrality (0.279) - this node is a cross-community bridge._
- **Why does `companionPath()` connect `Community 0` to `Community 9`, `Community 2`, `Community 6`?**
  _High betweenness centrality (0.206) - this node is a cross-community bridge._
- **Why does `feedFrame()` connect `Community 1` to `Community 2`, `Community 3`, `Community 5`, `Community 6`, `Community 9`?**
  _High betweenness centrality (0.195) - this node is a cross-community bridge._
- **Are the 28 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `companionPath()`) actually correct?**
  _`main()` has 28 INFERRED edges - model-reasoned connections that need verification._
- **Are the 20 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `setOnHit()`) actually correct?**
  _`main()` has 20 INFERRED edges - model-reasoned connections that need verification._
- **Are the 6 inferred relationships involving `feedFrame()` (e.g. with `main()` and `main()`) actually correct?**
  _`feedFrame()` has 6 INFERRED edges - model-reasoned connections that need verification._
- **Are the 6 inferred relationships involving `composite()` (e.g. with `renderCanonicalBoard()` and `drawHitOnCanonical()`) actually correct?**
  _`composite()` has 6 INFERRED edges - model-reasoned connections that need verification._