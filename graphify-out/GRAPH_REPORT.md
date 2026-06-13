# Graph Report - /Users/noahg/github/cam-stream  (2026-06-13)

## Corpus Check
- 49 files · ~108,280 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 237 nodes · 435 edges · 21 communities detected
- Extraction: 62% EXTRACTED · 38% INFERRED · 0% AMBIGUOUS · INFERRED: 165 edges (avg confidence: 0.8)
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
- [[_COMMUNITY_Community 20|Community 20]]

## God Nodes (most connected - your core abstractions)
1. `main()` - 34 edges
2. `main()` - 30 edges
3. `main()` - 27 edges
4. `feedFrame()` - 18 edges
5. `main()` - 17 edges
6. `processFrame()` - 15 edges
7. `main()` - 12 edges
8. `composite()` - 11 edges
9. `run()` - 10 edges
10. `main()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `updateFrame()`  [INFERRED]
  /Users/noahg/github/cam-stream/client/src/main.cpp → /Users/noahg/github/cam-stream/client/src/Display.cpp
- `main()` --calls--> `lineMergePerpPx()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → /Users/noahg/github/cam-stream/detection/src/Pipeline.cpp
- `main()` --calls--> `setLineMergePerpPx()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → /Users/noahg/github/cam-stream/detection/src/Pipeline.cpp
- `processFrame()` --calls--> `boundaryDistancePx()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/src/DartDetector.cpp → /Users/noahg/github/cam-stream/detection/src/ZoneMap.cpp
- `detectAuto()` --calls--> `run()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/src/BoardCalibrator.cpp → /Users/noahg/github/cam-stream/detection/src/AutoCalibrator.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.1
Nodes (38): describe(), humanIntervalOpen(), main(), loadFromFile(), refreshBackground(), reset(), main(), consumeBgRefreshRequest() (+30 more)

### Community 1 - "Community 1"
Cohesion: 0.08
Nodes (25): main(), onSignal(), parseArgs(), printUsage(), close(), makeFilename(), open(), Recorder() (+17 more)

### Community 2 - "Community 2"
Cohesion: 0.13
Nodes (17): main(), render(), saveAll(), angDiff(), blobMask(), boardPt(), collectBlobs(), run() (+9 more)

### Community 3 - "Community 3"
Cohesion: 0.18
Nodes (15): blitAspectFit(), composite(), drawCamTile(), pairwiseSpread(), Display, render(), updateFrame(), boardToCanonicalPx() (+7 more)

### Community 4 - "Community 4"
Cohesion: 0.19
Nodes (15): boardToImage(), imageToBoard(), localJacobian(), localScaleMmPerPx(), boardLooksCleared(), buildRoiMask(), dartAxisByMidpoints(), DartDetector() (+7 more)

### Community 5 - "Community 5"
Cohesion: 0.16
Nodes (14): cumSupportValid(), hasForegroundNear(), computeRoundStatus_(), feedFrame(), fusedVoteCount(), lineMergePerpPx(), maybeAutoReset(), Pipeline() (+6 more)

### Community 6 - "Community 6"
Cohesion: 0.22
Nodes (13): addHit(), confirm(), consistent(), covOf(), flush(), fuseVotes(), mahaSq(), MultiCamFusion() (+5 more)

### Community 7 - "Community 7"
Cohesion: 0.28
Nodes (5): setOnHitUpdated(), main(), roundOf(), canonicalZone(), zoneScore()

### Community 8 - "Community 8"
Cohesion: 0.25
Nodes (5): saveToFile(), detectAuto(), fromReferencePoints(), drawHud(), main()

### Community 9 - "Community 9"
Cohesion: 0.33
Nodes (2): CameraCapture, stop()

### Community 10 - "Community 10"
Cohesion: 0.38
Nodes (4): drain(), encode(), flush(), VideoEncoder()

### Community 11 - "Community 11"
Cohesion: 0.67
Nodes (1): DebugUI()

### Community 12 - "Community 12"
Cohesion: 0.67
Nodes (1): FrameSource

### Community 13 - "Community 13"
Cohesion: 1.0
Nodes (1): ZoneMapper

### Community 14 - "Community 14"
Cohesion: 1.0
Nodes (1): Renderer

### Community 15 - "Community 15"
Cohesion: 1.0
Nodes (1): BoardCalibrator

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (1): AutoCalibrator

### Community 17 - "Community 17"
Cohesion: 1.0
Nodes (1): ZoneMap

### Community 18 - "Community 18"
Cohesion: 1.0
Nodes (0): 

### Community 19 - "Community 19"
Cohesion: 1.0
Nodes (0): 

### Community 20 - "Community 20"
Cohesion: 1.0
Nodes (0): 

## Knowledge Gaps
- **5 isolated node(s):** `ZoneMapper`, `Renderer`, `BoardCalibrator`, `AutoCalibrator`, `ZoneMap`
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 13`** (2 nodes): `ZoneMapper.hpp`, `ZoneMapper`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 14`** (2 nodes): `Renderer`, `Renderer.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (2 nodes): `BoardCalibrator`, `BoardCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (2 nodes): `AutoCalibrator`, `AutoCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (2 nodes): `ZoneMap.hpp`, `ZoneMap`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (1 nodes): `Protocol.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 19`** (1 nodes): `BoardCalibration.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 20`** (1 nodes): `Types.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Community 1` to `Community 0`, `Community 10`, `Community 3`, `Community 6`?**
  _High betweenness centrality (0.218) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 0` to `Community 1`, `Community 2`, `Community 5`?**
  _High betweenness centrality (0.180) - this node is a cross-community bridge._
- **Why does `feedFrame()` connect `Community 5` to `Community 0`, `Community 4`, `Community 6`, `Community 7`?**
  _High betweenness centrality (0.172) - this node is a cross-community bridge._
- **Are the 31 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `isOpen()`) actually correct?**
  _`main()` has 31 INFERRED edges - model-reasoned connections that need verification._
- **Are the 29 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `isOpen()`) actually correct?**
  _`main()` has 29 INFERRED edges - model-reasoned connections that need verification._
- **Are the 24 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `setOnHit()`) actually correct?**
  _`main()` has 24 INFERRED edges - model-reasoned connections that need verification._
- **Are the 12 inferred relationships involving `feedFrame()` (e.g. with `main()` and `main()`) actually correct?**
  _`feedFrame()` has 12 INFERRED edges - model-reasoned connections that need verification._