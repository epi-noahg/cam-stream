# Graph Report - /Users/noahg/github/cam-stream/detection  (2026-06-11)

## Corpus Check
- 29 files · ~81,255 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 208 nodes · 358 edges · 21 communities detected
- Extraction: 63% EXTRACTED · 35% INFERRED · 1% AMBIGUOUS · INFERRED: 127 edges (avg confidence: 0.81)
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
1. `main()` - 29 edges
2. `main()` - 23 edges
3. `camdetect (Static Library)` - 17 edges
4. `processFrame()` - 14 edges
5. `feedFrame()` - 10 edges
6. `composite()` - 10 edges
7. `cam0 Zones Overlay` - 10 edges
8. `confirm()` - 9 edges
9. `run()` - 9 edges
10. `Cam1 Green Zone Binary Mask` - 8 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `seek()`  [INFERRED]
  /Users/noahg/github/cam-stream/detection/tools/debug_viewer.cpp → sources/FileSource.cpp
- `cam0 Zones Overlay` --shares_data_with--> `cam0 Zones Base Mask`  [AMBIGUOUS]
  cam0_zones_overlay.png → cam0_zones.png
- `cam0 Zones Base Mask` --conceptually_related_to--> `Dartboard (cam0)`  [AMBIGUOUS]
  cam0_zones.png → cam0_zones_overlay.png
- `Green Channel Zone Mask` --semantically_similar_to--> `Red Channel Zone Mask`  [INFERRED] [semantically similar]
  cam0_zones_green.png → cam0_zones_red.png
- `Cam1 Green Zone Binary Mask` --semantically_similar_to--> `Cam1 Red Zone Binary Mask`  [INFERRED] [semantically similar]
  cam1_zones_green.png → cam1_zones_red.png

## Communities

### Community 0 - "Community 0"
Cohesion: 0.08
Nodes (40): loadFromFile(), refreshBackground(), reset(), main(), consumeBgRefreshRequest(), consumeResetRequest(), consumeStepBackward(), consumeStepForward() (+32 more)

### Community 1 - "Community 1"
Cohesion: 0.15
Nodes (15): boardToImage(), imageToBoard(), localScaleMmPerPx(), saveToFile(), drawHud(), main(), boardLooksCleared(), buildRoiMask() (+7 more)

### Community 2 - "Community 2"
Cohesion: 0.18
Nodes (10): main(), render(), saveAll(), boundaryDistancePx(), companionPath(), idColor(), idToResult(), lookup() (+2 more)

### Community 3 - "Community 3"
Cohesion: 0.14
Nodes (17): AutoCalibrator.cpp, BoardCalibration.cpp, BoardCalibrator.cpp, camdetect (Static Library), camdetect_autocalib (Tool Executable), camdetect_calibrate (Tool Executable), camdetect_debug (Tool Executable), camdetect_offline (Tool Executable) (+9 more)

### Community 4 - "Community 4"
Cohesion: 0.31
Nodes (12): blitAspectFit(), composite(), drawCamTile(), pairwiseSpread(), boardToCanonicalPx(), drawCalibrationOverlay(), drawCircleBoardMM(), drawDetectionOverlay() (+4 more)

### Community 5 - "Community 5"
Cohesion: 0.29
Nodes (13): Bullseye Zone, Dartboard (cam0), Double Ring Zone, Green Channel Zone Mask, Camera 0 Perspective, Red Channel Zone Mask, Scoring Segments (cam0), Triple Ring Zone (+5 more)

### Community 6 - "Community 6"
Cohesion: 0.33
Nodes (13): Cam1 Zones Base Image (Nearly Black), Cam1 Elliptical Zone Projection (Perspective), Cam1 Green Zone Center Point, Cam1 Green Zone Inner Ring, Cam1 Green Zone Binary Mask, Cam1 Green Zone Outer Ring, Cam1 Zone Calibration Colored Dots Overlay, Winmau Dartboard (Cam1 View) (+5 more)

### Community 7 - "Community 7"
Cohesion: 0.27
Nodes (9): addHit(), confirm(), dist(), flush(), MultiCamFusion(), reset(), tick(), boundaryMarginMM() (+1 more)

### Community 8 - "Community 8"
Cohesion: 0.4
Nodes (11): Cam2 Zones Base Frame, Binary Zone Mask (White on Black), Bullseye / Center Point, Camera 2 (Cam2), Dartboard (Cam2 Target), Elliptical Zone Rings (Perspective-Corrected), Cam2 Green Zone Detection Mask, Cam2 Dartboard Overlay (Camera View) (+3 more)

### Community 9 - "Community 9"
Cohesion: 0.31
Nodes (8): angDiff(), blobMask(), boardPt(), collectBlobs(), run(), toNormalized(), detectAuto(), fromReferencePoints()

### Community 10 - "Community 10"
Cohesion: 0.39
Nodes (8): camdetect_live (Tool Executable), camstream server (TCP stream), FFmpeg (libavcodec/libavutil/libswscale), Rationale: client StreamReceiver+VideoDecoder reused directly to avoid refactoring client into shared library, Rationale: FFmpeg marked optional so file-mode tools build without FFmpeg headers, StreamReceiver.cpp (client source), Threads (POSIX/system threading), VideoDecoder.cpp (client source)

### Community 11 - "Community 11"
Cohesion: 0.5
Nodes (2): FileSource, seek()

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
Nodes (1): AutoCalibrator

### Community 18 - "Community 18"
Cohesion: 1.0
Nodes (1): ZoneMap

### Community 19 - "Community 19"
Cohesion: 1.0
Nodes (0): 

### Community 20 - "Community 20"
Cohesion: 1.0
Nodes (0): 

## Ambiguous Edges - Review These
- `cam0 Zones Overlay` → `cam0 Zones Base Mask`  [AMBIGUOUS]
  cam0_zones.png · relation: shares_data_with
- `cam0 Zones Base Mask` → `Dartboard (cam0)`  [AMBIGUOUS]
  cam0_zones.png · relation: conceptually_related_to
- `Winmau Dartboard (Cam1 View)` → `Cam1 Zones Base Image (Nearly Black)`  [AMBIGUOUS]
  cam1_zones.png · relation: conceptually_related_to
- `Cam2 Zones Base Frame` → `Cam2 Green Zone Detection Mask`  [AMBIGUOUS]
  cam2_zones.png · relation: shares_data_with
- `Cam2 Zones Base Frame` → `Cam2 Red Zone Detection Mask`  [AMBIGUOUS]
  cam2_zones.png · relation: shares_data_with

## Knowledge Gaps
- **16 isolated node(s):** `ZoneMapper`, `Renderer`, `BoardCalibrator`, `AutoCalibrator`, `ZoneMap` (+11 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 14`** (2 nodes): `ZoneMapper.hpp`, `ZoneMapper`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (2 nodes): `Renderer.hpp`, `Renderer`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (2 nodes): `BoardCalibrator`, `BoardCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (2 nodes): `AutoCalibrator`, `AutoCalibrator.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (2 nodes): `ZoneMap.hpp`, `ZoneMap`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 19`** (1 nodes): `BoardCalibration.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 20`** (1 nodes): `Types.hpp`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **What is the exact relationship between `cam0 Zones Overlay` and `cam0 Zones Base Mask`?**
  _Edge tagged AMBIGUOUS (relation: shares_data_with) - confidence is low._
- **What is the exact relationship between `cam0 Zones Base Mask` and `Dartboard (cam0)`?**
  _Edge tagged AMBIGUOUS (relation: conceptually_related_to) - confidence is low._
- **What is the exact relationship between `Winmau Dartboard (Cam1 View)` and `Cam1 Zones Base Image (Nearly Black)`?**
  _Edge tagged AMBIGUOUS (relation: conceptually_related_to) - confidence is low._
- **What is the exact relationship between `Cam2 Zones Base Frame` and `Cam2 Green Zone Detection Mask`?**
  _Edge tagged AMBIGUOUS (relation: shares_data_with) - confidence is low._
- **What is the exact relationship between `Cam2 Zones Base Frame` and `Cam2 Red Zone Detection Mask`?**
  _Edge tagged AMBIGUOUS (relation: shares_data_with) - confidence is low._
- **Why does `main()` connect `Community 0` to `Community 2`, `Community 11`?**
  _High betweenness centrality (0.126) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 0` to `Community 2`?**
  _High betweenness centrality (0.082) - this node is a cross-community bridge._