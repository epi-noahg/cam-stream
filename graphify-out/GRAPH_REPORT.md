# Graph Report - /Users/noahg/github/cam-stream  (2026-06-10)

## Corpus Check
- 41 files · ~41,428 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 178 nodes · 293 edges · 20 communities detected
- Extraction: 67% EXTRACTED · 33% INFERRED · 0% AMBIGUOUS · INFERRED: 97 edges (avg confidence: 0.8)
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
1. `main()` - 28 edges
2. `main()` - 25 edges
3. `main()` - 17 edges
4. `composite()` - 12 edges
5. `feedFrame()` - 11 edges
6. `processFrame()` - 10 edges
7. `close()` - 7 edges
8. `confirm()` - 7 edges
9. `drawCamTile()` - 7 edges
10. `main()` - 6 edges

## Surprising Connections (you probably didn't know these)
- `setInitPacket()` --calls--> `main()`  [INFERRED]
  /Users/noahg/github/cam-stream/server/src/StreamServer.cpp → /Users/noahg/github/cam-stream/client/src/main.cpp
- `main()` --calls--> `updateFrame()`  [INFERRED]
  /Users/noahg/github/cam-stream/client/src/main.cpp → /Users/noahg/github/cam-stream/client/src/Display.cpp
- `render()` --calls--> `composite()`  [INFERRED]
  /Users/noahg/github/cam-stream/client/src/Display.cpp → /Users/noahg/github/cam-stream/detection/src/DebugUI.cpp
- `main()` --calls--> `seek()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → /Users/noahg/github/cam-stream/detection/sources/FileSource.cpp
- `main()` --calls--> `diffThreshold()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → /Users/noahg/github/cam-stream/detection/src/Pipeline.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.12
Nodes (26): loadFromFile(), refreshBackground(), reset(), main(), consumeBgRefreshRequest(), consumeResetRequest(), consumeStepBackward(), consumeStepForward() (+18 more)

### Community 1 - "Community 1"
Cohesion: 0.11
Nodes (17): main(), onSignal(), parseArgs(), printUsage(), close(), makeFilename(), open(), Recorder() (+9 more)

### Community 2 - "Community 2"
Cohesion: 0.19
Nodes (12): computeRoundStatus_(), diffThreshold(), feedFrame(), fusedCentroid(), fusedVoteCount(), lineMergePerpPx(), maybeAutoReset(), Pipeline() (+4 more)

### Community 3 - "Community 3"
Cohesion: 0.27
Nodes (13): blitAspectFit(), composite(), drawCamTile(), fusedCentroid(), pairwiseSpread(), boardToCanonicalPx(), drawCalibrationOverlay(), drawCircleBoardMM() (+5 more)

### Community 4 - "Community 4"
Cohesion: 0.23
Nodes (8): acceptLoop(), buildVideoPacket(), push(), sendAll(), sendLoop(), setInitPacket(), stop(), StreamServer()

### Community 5 - "Community 5"
Cohesion: 0.2
Nodes (6): boardToImage(), imageToBoard(), saveToFile(), fromReferencePoints(), drawHud(), main()

### Community 6 - "Community 6"
Cohesion: 0.27
Nodes (8): addHit(), confirm(), dist(), flush(), MultiCamFusion(), reset(), tick(), lookup()

### Community 7 - "Community 7"
Cohesion: 0.33
Nodes (9): boardLooksCleared(), buildRoiMask(), dartAxisByMidpoints(), DartDetector(), extendBboxAlongAxis(), fillDartRegion(), labDistance(), processFrame() (+1 more)

### Community 8 - "Community 8"
Cohesion: 0.25
Nodes (5): main(), FileSource, next(), seek(), isOpen()

### Community 9 - "Community 9"
Cohesion: 0.33
Nodes (2): CameraCapture, stop()

### Community 10 - "Community 10"
Cohesion: 0.38
Nodes (4): drain(), encode(), flush(), VideoEncoder()

### Community 11 - "Community 11"
Cohesion: 0.33
Nodes (3): Display, render(), updateFrame()

### Community 12 - "Community 12"
Cohesion: 0.67
Nodes (1): DebugUI()

### Community 13 - "Community 13"
Cohesion: 0.67
Nodes (1): FrameSource

### Community 14 - "Community 14"
Cohesion: 1.0
Nodes (1): ZoneMapper

### Community 15 - "Community 15"
Cohesion: 1.0
Nodes (1): Renderer

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (1): BoardCalibrator

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
- **3 isolated node(s):** `ZoneMapper`, `Renderer`, `BoardCalibrator`
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 14`** (2 nodes): `ZoneMapper.hpp`, `ZoneMapper`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (2 nodes): `Renderer`, `Renderer.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (2 nodes): `BoardCalibrator`, `BoardCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (1 nodes): `Protocol.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (1 nodes): `BoardCalibration.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 19`** (1 nodes): `Types.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Community 1` to `Community 0`, `Community 4`, `Community 6`, `Community 10`, `Community 11`?**
  _High betweenness centrality (0.298) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 0` to `Community 1`, `Community 2`?**
  _High betweenness centrality (0.228) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 0` to `Community 8`, `Community 2`?**
  _High betweenness centrality (0.179) - this node is a cross-community bridge._
- **Are the 27 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `isOpen()`) actually correct?**
  _`main()` has 27 INFERRED edges - model-reasoned connections that need verification._
- **Are the 22 inferred relationships involving `main()` (e.g. with `loadFromFile()` and `setOnHit()`) actually correct?**
  _`main()` has 22 INFERRED edges - model-reasoned connections that need verification._
- **Are the 13 inferred relationships involving `main()` (e.g. with `setInitPacket()` and `start()`) actually correct?**
  _`main()` has 13 INFERRED edges - model-reasoned connections that need verification._
- **Are the 7 inferred relationships involving `composite()` (e.g. with `render()` and `renderCanonicalBoard()`) actually correct?**
  _`composite()` has 7 INFERRED edges - model-reasoned connections that need verification._