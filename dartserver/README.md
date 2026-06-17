# dartserver

Serveur de jeu de fléchettes **autoritatif** : capture les caméras, exécute la
détection (`../detection`), possède l'état du jeu **X01** et sa persistance
(SQLite), et expose une **API WebSocket + JSON** au front (`../GoDartss`).

## Architecture

```
3× caméras V4L2 ─► DetectionService ─► GameManager ─► WsServer ──► front (WS+JSON)
   (CameraCapture)   (camdetect::Pipeline)  (X01Engine)  │
                          FusedHit→Throw                  └─► Db (SQLite)
```

Couches (chacune une lib statique, testable isolément) :

| Lib           | Rôle                                                        |
|---------------|-------------------------------------------------------------|
| `dartgame`    | Règles X01 pures (`X01Engine`, `Checkout`) + `GameManager`  |
| `dartdetect`  | Pont `camdetect::Pipeline` → jeu, statut board, mode replay |
| `dartapi`     | Protocole WebSocket + JSON (`WsServer`, `Dispatcher`, `Json`)|
| `dartpersist` | Persistance SQLite (joueurs, matchs, stats, leaderboard)    |

La **source de vérité unique** est `GameManager` : toute mutation (fléchette
détectée, correction, clear, undo) modifie l'état puis est rediffusée à tous
les clients WS.

## Build

```bash
cmake -S dartserver -B dartserver/build
cmake --build dartserver/build -j
ctest --test-dir dartserver/build --output-on-failure
```

Dépendances : OpenCV, Boost (Beast), nlohmann/json, SQLite3, + `../detection`
(camdetect) et `../server/src/CameraCapture.cpp` (partagés par chemin).

## Lancer

```bash
# Live (caméras V4L2) — nécessite des calibrations (voir camdetect_autocalib)
./dartserver/build/dartserver --live cam0.yml cam1.yml cam2.yml

# Replay (vidéos enregistrées) — vérification sans matériel
./dartserver/build/dartserver --replay v0.mp4 v1.mp4 v2.mp4 cam0.yml cam1.yml cam2.yml
```

WebSocket sur le port **8080**. Base SQLite : `dartserver.db` (cwd).

## Protocole WebSocket

**Serveur → client** : `board_status`, `game_state` (+ `checkout`),
`dart_detected` (avec `needsReview` si confiance basse), `ack`/`error`,
`players`/`leaderboard`/`history`.

**Client → serveur** : `create_game`, `manual_throw`, `correct_throw`,
`clear_board`/`reset_round`, `undo`, `next_player`, `refresh_background`,
`create_player`, `get_players`, `get_leaderboard`, `get_history`.

Test d'intégration : `python3 dartserver/tests/ws_integration.py` (serveur lancé).

## Front

L'écran tactile est `../GoDartss/src/app/live/page.tsx` (route `/live`), piloté
par le store `src/store/dartStore.ts`. Il affiche le statut board, auto-remplit
les fléchettes détectées, met en avant celles à vérifier et permet de corriger
en un tap. Cible : tablette 10".
