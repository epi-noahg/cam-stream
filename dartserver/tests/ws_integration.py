import asyncio, json, sys
import websockets

async def main():
    uri = "ws://localhost:8080"
    async with websockets.connect(uri, compression=None) as ws:
        received = []

        async def recv_for(seconds):
            try:
                while True:
                    msg = await asyncio.wait_for(ws.recv(), timeout=seconds)
                    received.append(json.loads(msg))
            except asyncio.TimeoutError:
                pass

        # initial snapshot on connect
        await recv_for(0.5)

        async def send(obj):
            await ws.send(json.dumps(obj))
            await recv_for(0.4)

        await send({"type": "create_game",
                    "players": [{"id": 0, "nickname": "Alice"},
                                {"id": 1, "nickname": "Bob"}],
                    "options": {"startingScore": 501, "outType": "DOUBLE", "legs": 1}})
        await send({"type": "manual_throw", "value": 20, "multiplier": 3})  # 60 -> 441
        await send({"type": "correct_throw", "turnIndex": 0, "throwIndex": 0,
                    "value": 20, "multiplier": 1})  # now single 20 -> 481
        await send({"type": "undo"})  # back to 441

        # Analyze
        def last_state():
            for m in reversed(received):
                if m.get("type") == "game_state":
                    return m["state"]
            return None

        types = [m.get("type") for m in received]
        print("RECEIVED TYPES:", types)

        # Find the game_state right after each command by scanning order
        states = [m["state"] for m in received if m.get("type") == "game_state"]
        acks = [m for m in received if m.get("type") == "ack"]
        print("ACKS:", [(a["command"], a["ok"]) for a in acks])

        scores_p0 = [s["players"][0]["score"] for s in states]
        print("P0 SCORE PROGRESSION:", scores_p0)

        ok = True
        if not any(a["command"] == "create_game" and a["ok"] for a in acks):
            print("FAIL: create_game not acked"); ok = False
        # after manual_throw 441 should appear, after correct 481, after undo 441
        if 441 not in scores_p0 or 481 not in scores_p0:
            print("FAIL: expected 441 and 481 in progression"); ok = False
        if scores_p0[-1] != 441:
            print("FAIL: undo did not restore 441, got", scores_p0[-1]); ok = False
        # board_status present
        if "board_status" not in types:
            print("FAIL: no board_status received"); ok = False

        print("RESULT:", "PASS" if ok else "FAIL")
        sys.exit(0 if ok else 1)

asyncio.run(main())
