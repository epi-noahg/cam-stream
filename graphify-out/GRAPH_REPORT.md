# Graph Report - /Users/noahg/github/cam-stream  (2026-06-05)

## Corpus Check
- 40 files · ~23,047 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 159 nodes · 233 edges · 18 communities detected
- Extraction: 71% EXTRACTED · 29% INFERRED · 0% AMBIGUOUS · INFERRED: 68 edges (avg confidence: 0.8)
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

## God Nodes (most connected - your core abstractions)
1. `main()` - 25 edges
2. `main()` - 17 edges
3. `composite()` - 11 edges
4. `processFrame()` - 9 edges
5. `close()` - 7 edges
6. `feedFrame()` - 7 edges
7. `confirm()` - 7 edges
8. `main()` - 6 edges
9. `main()` - 5 edges
10. `CameraCapture` - 4 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `setInitPacket()`  [INFERRED]
  /Users/noahg/github/cam-stream/client/src/main.cpp → /Users/noahg/github/cam-stream/server/src/StreamServer.cpp
- `main()` --calls--> `updateFrame()`  [INFERRED]
  /Users/noahg/github/cam-stream/client/src/main.cpp → /Users/noahg/github/cam-stream/client/src/Display.cpp
- `render()` --calls--> `composite()`  [INFERRED]
  /Users/noahg/github/cam-stream/client/src/Display.cpp → /Users/noahg/github/cam-stream/detection/src/DebugUI.cpp
- `main()` --calls--> `seek()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → /Users/noahg/github/cam-stream/detection/sources/FileSource.cpp
- `processFrame()` --calls--> `imageToBoard()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/src/DartDetector.cpp → /Users/noahg/github/cam-stream/detection/src/BoardCalibration.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.11
Nodes (17): main(), onSignal(), parseArgs(), printUsage(), close(), makeFilename(), open(), Recorder() (+9 more)

### Community 1 - "Community 1"
Cohesion: 0.11
Nodes (19): main(), consumeBgRefreshRequest(), consumeResetRequest(), consumeStepBackward(), consumeStepForward(), DebugUI(), isPaused(), render() (+11 more)

### Community 2 - "Community 2"
Cohesion: 0.16
Nodes (8): boardToImage(), imageToBoard(), loadFromFile(), saveToFile(), fromReferencePoints(), drawHud(), main(), drawCalibrationOverlay()

### Community 3 - "Community 3"
Cohesion: 0.24
Nodes (11): boardLooksCleared(), buildRoiMask(), DartDetector(), labDistance(), lineExtendByMask(), processFrame(), refreshBackground(), reset() (+3 more)

### Community 4 - "Community 4"
Cohesion: 0.23
Nodes (8): acceptLoop(), buildVideoPacket(), push(), sendAll(), sendLoop(), setInitPacket(), stop(), StreamServer()

### Community 5 - "Community 5"
Cohesion: 0.26
Nodes (9): addHit(), confirm(), dist(), flush(), MultiCamFusion(), reset(), tick(), feedFrame() (+1 more)

### Community 6 - "Community 6"
Cohesion: 0.31
Nodes (10): composite(), fusedCentroid(), pairwiseSpread(), boardToCanonicalPx(), drawCircleBoardMM(), drawDetectionOverlay(), drawFullDart(), drawHitOnCanonical() (+2 more)

### Community 7 - "Community 7"
Cohesion: 0.22
Nodes (6): main(), FileSource, next(), seek(), setOnHit(), isOpen()

### Community 8 - "Community 8"
Cohesion: 0.33
Nodes (2): CameraCapture, stop()

### Community 9 - "Community 9"
Cohesion: 0.38
Nodes (4): drain(), encode(), flush(), VideoEncoder()

### Community 10 - "Community 10"
Cohesion: 0.33
Nodes (3): Display, render(), updateFrame()

### Community 11 - "Community 11"
Cohesion: 0.67
Nodes (1): FrameSource

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
Nodes (0): 

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (0): 

### Community 17 - "Community 17"
Cohesion: 1.0
Nodes (0): 

## Knowledge Gaps
- **3 isolated node(s):** `ZoneMapper`, `Renderer`, `BoardCalibrator`
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 12`** (2 nodes): `ZoneMapper.hpp`, `ZoneMapper`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 13`** (2 nodes): `Renderer`, `Renderer.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 14`** (2 nodes): `BoardCalibrator`, `BoardCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (1 nodes): `Protocol.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (1 nodes): `BoardCalibration.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (1 nodes): `Types.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Community 0` to `Community 1`, `Community 4`, `Community 5`, `Community 9`, `Community 10`?**
  _High betweenness centrality (0.400) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 1` to `Community 2`, `Community 3`, `Community 5`, `Community 7`?**
  _High betweenness centrality (0.302) - this node is a cross-community bridge._
- **Why does `render()` connect `Community 1` to `Community 0`, `Community 6`?**
  _High betweenness centrality (0.263) - this node is a cross-community bridge._
- **Are the 24 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `isOpen()`) actually correct?**
  _`main()` has 24 INFERRED edges - model-reasoned connections that need verification._
- **Are the 13 inferred relationships involving `main()` (e.g. with `setInitPacket()` and `start()`) actually correct?**
  _`main()` has 13 INFERRED edges - model-reasoned connections that need verification._
- **Are the 7 inferred relationships involving `composite()` (e.g. with `render()` and `drawCalibrationOverlay()`) actually correct?**
  _`composite()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **Are the 3 inferred relationships involving `processFrame()` (e.g. with `feedFrame()` and `imageToBoard()`) actually correct?**
  _`processFrame()` has 3 INFERRED edges - model-reasoned connections that need verification._