"use client";

/**
 * Écran de jeu live (tablette) — piloté par le serveur autoritatif.
 *
 * Réutilise les composants stylés existants (cible, scoreboard éditable avec
 * historique par manche, affichage joueur courant) via ServerGameProvider, et
 * ajoute le statut board (caméras prêtes) + la file de fléchettes à vérifier.
 * Aucun scoring local : tout vient du serveur, les corrections y retournent.
 */

import { useEffect, useState } from "react";
import Link from "next/link";
import { useDartStore } from "@/store/dartStore";
import type { GameMode, Throw } from "@/lib/dartTypes";
import { randomGuestName } from "@/lib/guestNames";
import { ServerGameProvider } from "@/context/ServerGameProvider";
import BoardStatusBar from "@/components/live/BoardStatusBar";
import ThrowPad from "@/components/live/ThrowPad";
import ThrowChip from "@/components/live/ThrowChip";
import DartboardResponsive from "@/components/DartboardResponsive";
import CricketBoard from "@/components/live/CricketBoard";
import RoundClockBoard from "@/components/live/RoundClockBoard";

const MODE_LABEL: Record<GameMode, string> = {
  X01: "X01",
  CRICKET: "Cricket",
  ROUND_THE_CLOCK: "Around the Clock",
};

type PadState =
  | { mode: "manual" }
  | { mode: "correct"; turnIndex: number; throwIndex: number; value: number; multiplier: number }
  | null;

function fmtCheckout(t: Throw): string {
  if (t.value === 25 || t.value === 50) return String(t.value);
  return `${t.multiplier === 3 ? "T" : t.multiplier === 2 ? "D" : ""}${t.value}`;
}

export default function LivePage() {
  const s = useDartStore();
  const [pad, setPad] = useState<PadState>(null);
  // "Nouvelle partie" demandée (accueil ou écran de victoire) : on affiche la
  // config et on IGNORE la partie serveur en cours, qui reste sauvegardée et
  // reprenable depuis l'accueil.
  const [newGame, setNewGame] = useState(false);
  // Id de la partie remplacée en lançant une nouvelle : on patiente tant que le
  // serveur n'a pas renvoyé une partie d'id différent (évite d'afficher
  // brièvement l'ancienne partie / l'écran de victoire).
  const [replacedId, setReplacedId] = useState<string | null | undefined>(undefined);
  // Mode choisi pour la prochaine "nouvelle partie" (depuis l'accueil).
  const [setupMode, setSetupMode] = useState<GameMode>("X01");

  useEffect(() => {
    s.connect();
    // Arrivé via un mode depuis l'accueil → forcer l'écran Nouvelle partie
    // (ne pas rester bloqué sur l'écran de victoire de la partie précédente).
    if (typeof window !== "undefined") {
      const params = new URLSearchParams(window.location.search);
      if (params.get("new")) setNewGame(true);
      const m = params.get("mode");
      if (m === "CRICKET" || m === "ROUND_THE_CLOCK" || m === "X01") setSetupMode(m);
    }
    return () => s.disconnect();
  }, []);

  const game = s.game;

  // Lance une nouvelle partie : mémorise l'id remplacé puis envoie la commande.
  // L'ancienne partie reste persistée côté serveur (reprenable depuis l'accueil),
  // même si elle n'est pas terminée.
  const launchGame = (
    players: { nickname: string; team?: number }[],
    opts: Record<string, unknown>,
  ) => {
    setReplacedId(s.gameId);
    setNewGame(false);
    s.createGame(players, opts);
  };

  // En attente de la nouvelle partie tant que le serveur renvoie l'id remplacé.
  const waitingNew = replacedId !== undefined && s.gameId === replacedId;

  // Écran courant. En mode "nouvelle partie" on reste sur la config sans se
  // laisser détourner par la partie serveur en cours.
  const screen: "setup" | "loading" | "game" | "victory" = newGame
    ? "setup"
    : waitingNew
    ? "loading"
    : game && !game.gameOver
    ? "game"
    : game && game.gameOver
    ? "victory"
    : "setup";
  // Alias type-guard : restreint `game` à non-null dans la branche de jeu.
  const showGame = !!game && !game.gameOver && !newGame && !waitingNew;

  // Libellé du gagnant : pseudo, ou "Équipe N — membres" en mode équipe.
  let winnerLabel = "";
  if (game && game.winner != null) {
    const wp = game.players.find((p) => p.id === game.winner);
    if (wp) {
      winnerLabel =
        (game.options.teams ?? 1) >= 2 && wp.team !== 0
          ? `Équipe ${wp.team} — ${game.players.filter((p) => p.team === wp.team).map((p) => p.nickname).join(" & ")}`
          : wp.nickname;
    }
  }

  const replaySamePlayers = () => {
    if (!game) return;
    // Les options reçues du serveur contiennent déjà le mode et ses réglages.
    launchGame(
      game.players.map((p) => ({ nickname: p.nickname, team: p.team })),
      { ...game.options, mode: game.mode ?? game.options.mode ?? "X01" }
    );
  };

  const openCorrect = (turnIndex: number, throwIndex: number, t: Throw) =>
    setPad({ mode: "correct", turnIndex, throwIndex, value: t.value, multiplier: t.multiplier });

  return (
    <div className="h-screen overflow-hidden bg-black text-white p-2 flex flex-col gap-2">
      {/* Barre du haut (compacte) */}
      <div className="flex items-center gap-2 shrink-0">
        <div className="flex-1 min-w-0">
          <BoardStatusBar board={s.board} connected={s.connected} />
        </div>
        <Link href="/" className="min-h-11 px-3 flex items-center rounded-xl bg-gray-800 border border-gray-700 active:scale-95">🏠</Link>
        <Link href="/leaderboard" className="min-h-11 px-3 flex items-center rounded-xl bg-gray-800 border border-gray-700 active:scale-95">📊</Link>
      </div>

      {!showGame ? (
        <div className="flex-1 min-h-0 overflow-auto">
          {screen === "victory" ? (
            <Victory label={winnerLabel} onReplay={replaySamePlayers} onNew={() => setNewGame(true)} />
          ) : screen === "loading" ? (
            <NewGameLoading />
          ) : (
            <Setup mode={setupMode} onStart={launchGame} />
          )}
        </div>
      ) : (
        <ServerGameProvider>
          <div className="flex-1 min-h-0 flex gap-2">
            {/* Cible (gauche) — tap pour ajouter une fléchette manuelle */}
            <div className="w-[44%] rounded-xl bg-gray-900 border border-gray-700 p-2 flex flex-col min-h-0">
              <div className="flex-1 min-h-0">
                <DartboardResponsive />
              </div>
            </div>

            {/* Joueurs + tour courant + historique (droite) */}
            <div className="flex-1 min-h-0 flex flex-col gap-2">
              {(() => {
                const n = game.players.length;
                const lastIdx = game.turns.length - 1;
                const curTurn = lastIdx >= 0 ? game.turns[lastIdx] : [];
                const rounds = Math.ceil(game.turns.length / Math.max(1, n));
                const isTeams = (game.options.teams ?? 1) >= 2;
                const curTeam = game.players[game.currentIndex]?.team;
                const teamIds = Array.from(new Set(game.players.map((p) => p.team))).sort((a, b) => a - b);

                // Dernier lancer enregistré (n'importe quel tour) + fiabilité caméra.
                let last: { turnIndex: number; throwIndex: number; thr: Throw; nickname: string } | null = null;
                for (let ti = game.turns.length - 1; ti >= 0 && !last; ti--) {
                  const turn = game.turns[ti];
                  if (turn.length > 0)
                    last = {
                      turnIndex: ti,
                      throwIndex: turn.length - 1,
                      thr: turn[turn.length - 1],
                      nickname: game.players[ti % n]?.nickname ?? "",
                    };
                }
                const det = s.lastDetected;
                const detMatches =
                  !!last && !!det && det.value === last.thr.value && det.multiplier === last.thr.multiplier;
                const badge = !last
                  ? null
                  : detMatches && det!.needsReview
                  ? { text: "⚠ à vérifier", cls: "text-amber-400", card: "border-amber-500 bg-amber-500/10" }
                  : detMatches
                  ? { text: `caméra ${Math.round(det!.confidence * 100)}%`, cls: "text-green-400", card: "border-gray-700 bg-gray-900" }
                  : { text: "saisi manuellement", cls: "text-gray-400", card: "border-gray-700 bg-gray-900" };

                return (
                  <>
                    {/* Dernier lancer (vérif rapide caméra + correction) */}
                    {last && badge && (
                      <button
                        onClick={() => openCorrect(last!.turnIndex, last!.throwIndex, last!.thr)}
                        className={`shrink-0 rounded-xl border p-2 flex items-center gap-3 active:scale-[0.99] ${badge.card}`}
                      >
                        <div className="w-20"><ThrowChip thr={last.thr} size="lg" /></div>
                        <div className="flex-1 text-left">
                          <div className="text-[11px] uppercase tracking-wider text-gray-400">
                            Dernier lancer · {last.nickname}
                          </div>
                          <div className={`text-base font-bold ${badge.cls}`}>{badge.text}</div>
                          <div className="text-xs text-gray-500">Toucher pour corriger</div>
                        </div>
                        <div className="text-2xl pr-1">{detMatches && det!.needsReview ? "⚠" : "✎"}</div>
                      </button>
                    )}

                    {/* Scores + tour courant */}
                    <div className="rounded-xl bg-gray-900 border border-gray-700 p-3 shrink-0">
                      {game.mode === "CRICKET" ? (
                        <CricketBoard game={game} />
                      ) : game.mode === "ROUND_THE_CLOCK" ? (
                        <RoundClockBoard game={game} />
                      ) : (
                      <div className="flex flex-col gap-1.5">
                        {!isTeams
                          ? game.players.map((p, i) => {
                              const active = i === game.currentIndex;
                              const finished = game.finishedPlayers.includes(p.id);
                              return (
                                <div key={p.id}
                                  className={`flex items-center justify-between rounded-lg px-3 py-1.5 ${active ? "bg-red-600/20 border border-red-600" : "bg-gray-800"}`}>
                                  <span className="flex items-center gap-2 text-lg font-semibold truncate">
                                    {active && <span className="text-red-500">▶</span>}
                                    {p.nickname}
                                    {finished && <span className="text-xs text-green-400">✓</span>}
                                  </span>
                                  <span className={`font-mono font-black ${active ? "text-4xl" : "text-2xl text-gray-300"}`}>
                                    {p.score}
                                  </span>
                                </div>
                              );
                            })
                          : teamIds.map((tid) => {
                              const members = game.players.filter((p) => p.team === tid);
                              const active = tid === curTeam;
                              const finished = members.every((m) => game.finishedPlayers.includes(m.id));
                              const teamScore = members[0]?.score ?? 0;
                              return (
                                <div key={tid}
                                  className={`flex items-center justify-between rounded-lg px-3 py-1.5 ${active ? "bg-red-600/20 border border-red-600" : "bg-gray-800"}`}>
                                  <span className="flex flex-col min-w-0">
                                    <span className="flex items-center gap-2 text-base font-bold">
                                      {active && <span className="text-red-500">▶</span>}
                                      Équipe {tid}
                                      {finished && <span className="text-xs text-green-400">✓</span>}
                                    </span>
                                    <span className="text-xs text-gray-400 truncate">
                                      {members.map((m) => (
                                        <span key={m.id} className={m.id === game.players[game.currentIndex]?.id ? "text-white font-semibold" : ""}>
                                          {m.nickname}
                                          {m.id !== members[members.length - 1].id ? " · " : ""}
                                        </span>
                                      ))}
                                    </span>
                                  </span>
                                  <span className={`font-mono font-black ${active ? "text-4xl" : "text-2xl text-gray-300"}`}>
                                    {teamScore}
                                  </span>
                                </div>
                              );
                            })}
                      </div>
                      )}

                      {/* Fléchettes du tour en cours */}
                      <div className="grid grid-cols-3 gap-2 mt-2">
                        {[0, 1, 2].map((i) => (
                          <ThrowChip key={i} thr={curTurn[i]} size="lg"
                            onTap={curTurn[i] ? () => openCorrect(lastIdx, i, curTurn[i]) : undefined} />
                        ))}
                      </div>

                      {s.checkout && s.checkout.length > 0 && (
                        <div className="mt-2 text-center text-emerald-300 text-sm">
                          Checkout&nbsp;: <span className="font-mono font-bold">{s.checkout.map(fmtCheckout).join(" · ")}</span>
                        </div>
                      )}
                    </div>

                    {/* Historique par manche (scroll interne) */}
                    <div className="flex-1 min-h-0 rounded-xl bg-gray-900 border border-gray-700 p-2 overflow-y-auto">
                      <div className="text-[11px] uppercase tracking-wider text-gray-500 mb-1">Historique</div>
                      <div className="flex flex-col gap-1.5">
                        {Array.from({ length: rounds }).map((_, r) => (
                          <div key={r} className="flex items-stretch gap-2">
                            <div className="w-6 shrink-0 text-[10px] text-gray-600 flex items-center">{r + 1}</div>
                            <div className="flex-1 grid gap-2" style={{ gridTemplateColumns: `repeat(${n}, minmax(0,1fr))` }}>
                              {game.players.map((p, pi) => {
                                const ti = r * n + pi;
                                const turn = game.turns[ti] || [];
                                return (
                                  <div key={p.id} className="flex gap-1">
                                    {turn.length > 0 ? (
                                      turn.map((t, j) => (
                                        <ThrowChip key={j} thr={t} size="sm" onTap={() => openCorrect(ti, j, t)} />
                                      ))
                                    ) : (
                                      <span className="text-gray-700 text-xs px-1">·</span>
                                    )}
                                  </div>
                                );
                              })}
                            </div>
                          </div>
                        ))}
                      </div>
                    </div>
                  </>
                );
              })()}
            </div>
          </div>

          {/* Barre d'actions (compacte) */}
          <div className="grid grid-cols-4 gap-2 shrink-0">
            <button onClick={() => setPad({ mode: "manual" })}
              className="min-h-14 rounded-lg bg-blue-600 text-lg font-bold active:scale-95">+ Fléchette</button>
            <button onClick={() => s.clearBoard()}
              className="min-h-14 rounded-lg bg-gray-700 text-lg font-bold active:scale-95">Clear</button>
            <button onClick={() => s.undo()}
              className="min-h-14 rounded-lg bg-gray-700 text-lg font-bold active:scale-95">Annuler</button>
            <button onClick={() => s.nextPlayer()}
              className="min-h-14 rounded-lg bg-gray-700 text-lg font-bold active:scale-95">Joueur suivant</button>
          </div>
        </ServerGameProvider>
      )}

      {pad && (
        <ThrowPad
          title={pad.mode === "manual" ? "Ajouter une fléchette" : "Corriger la fléchette"}
          initialValue={pad.mode === "correct" ? pad.value : undefined}
          initialMultiplier={pad.mode === "correct" ? pad.multiplier : 1}
          onCancel={() => setPad(null)}
          onSubmit={(value, multiplier) => {
            if (pad.mode === "manual") s.manualThrow(value, multiplier);
            else s.correctThrow(pad.turnIndex, pad.throwIndex, value, multiplier);
            setPad(null);
          }}
        />
      )}
    </div>
  );
}

// ── Écran de victoire ────────────────────────────────────────────────────────
function Victory({ label, onReplay, onNew }: { label: string; onReplay: () => void; onNew: () => void }) {
  return (
    <div className="flex-1 flex items-center justify-center p-2 min-h-full">
      <div className="w-full max-w-md rounded-2xl bg-gray-900 border border-green-600 p-8 flex flex-col items-center gap-5 text-center">
        <div className="text-6xl">🏆</div>
        <div>
          <div className="text-3xl font-black text-green-400">{label || "Partie terminée"}</div>
          <div className="text-gray-400 mt-1">remporte la partie !</div>
        </div>
        <div className="flex flex-col gap-3 w-full">
          <button onClick={onReplay} className="min-h-16 rounded-lg bg-green-600 text-xl font-bold active:scale-95">
            ↻ Rejouer (mêmes joueurs)
          </button>
          <button onClick={onNew} className="min-h-16 rounded-lg bg-gray-700 text-xl font-bold active:scale-95">
            Nouvelle partie
          </button>
        </div>
      </div>
    </div>
  );
}

// ── Écran d'attente : nouvelle partie en cours de création ────────────────────
function NewGameLoading() {
  return (
    <div className="flex-1 flex items-center justify-center p-2 min-h-full">
      <div className="flex flex-col items-center gap-4 text-gray-400">
        <div className="h-10 w-10 rounded-full border-4 border-gray-700 border-t-red-500 animate-spin" />
        <div className="text-lg font-semibold">Nouvelle partie…</div>
      </div>
    </div>
  );
}

// ── Écran de configuration ───────────────────────────────────────────────────
type LineupEntry = { key: string; nickname: string; guest: boolean; playerId?: number; team: number };

function Setup({
  mode,
  onStart,
}: {
  mode: GameMode;
  onStart: (players: { nickname: string; team?: number }[], opts: Record<string, unknown>) => void;
}) {
  const connected = useDartStore((s) => s.connected);
  const dbPlayers = useDartStore((s) => s.players);
  const getPlayers = useDartStore((s) => s.getPlayers);

  const [lineup, setLineup] = useState<LineupEntry[]>([]);
  const [score, setScore] = useState(501);
  const [doubleOut, setDoubleOut] = useState(true);
  const [cutThroat, setCutThroat] = useState(false);  // Cricket
  const [useBull, setUseBull] = useState(true);       // Cricket
  const [teams, setTeams] = useState(0); // 0 = solo, sinon nombre d'équipes
  const [editKey, setEditKey] = useState<string | null>(null);
  const [editVal, setEditVal] = useState("");

  // Charge les joueurs de la DB (pas besoin du clavier pour les rejouer).
  useEffect(() => {
    if (connected) getPlayers();
  }, [connected, getPlayers]);

  const used = new Set(lineup.map((p) => p.nickname.toLowerCase()));
  const nextTeam = (count: number) => (teams >= 2 ? (count % teams) + 1 : 0);

  const addDb = (p: { id: number; nickname: string }) => {
    if (used.has(p.nickname.toLowerCase())) return;
    setLineup([...lineup, { key: `db${p.id}`, nickname: p.nickname, guest: false, playerId: p.id, team: nextTeam(lineup.length) }]);
  };
  const addGuest = () =>
    setLineup([...lineup, { key: `g${Date.now()}`, nickname: randomGuestName(used), guest: true, team: nextTeam(lineup.length) }]);
  const reroll = (key: string) =>
    setLineup(lineup.map((p) => (p.key === key && p.guest ? { ...p, nickname: randomGuestName(used) } : p)));
  const remove = (key: string) => setLineup(lineup.filter((p) => p.key !== key));
  const rename = (key: string, name: string) =>
    setLineup(lineup.map((p) => (p.key === key ? { ...p, nickname: name } : p)));
  const cycleTeam = (key: string) =>
    setLineup(lineup.map((p) => (p.key === key ? { ...p, team: (p.team % teams) + 1 } : p)));
  const setTeamCount = (t: number) => {
    setTeams(t);
    setLineup((prev) => prev.map((p, i) => ({ ...p, team: t >= 2 ? (i % t) + 1 : 0 })));
  };

  const available = dbPlayers.filter((p) => !used.has(p.nickname.toLowerCase()));

  return (
    <div className="flex-1 flex items-center justify-center p-2">
      <div className="w-full max-w-2xl rounded-2xl bg-gray-900 border border-gray-700 p-5 flex flex-col gap-4">
        <h1 className="text-2xl font-bold">Nouvelle partie {MODE_LABEL[mode]}</h1>

        {/* Joueurs de cette partie */}
        <div>
          <div className="text-sm text-gray-400 mb-1">Cette partie</div>
          {lineup.length === 0 ? (
            <p className="text-gray-500 text-sm py-2">
              Ajoute des joueurs ci-dessous (enregistrés ou invités).
            </p>
          ) : (
            <div className="flex flex-col gap-2">
              {lineup.map((p) => (
                <div key={p.key} className="flex items-center gap-2 rounded-lg bg-gray-800 px-3 py-2">
                  {editKey === p.key ? (
                    <>
                      <input
                        value={editVal}
                        onChange={(e) => setEditVal(e.target.value)}
                        className="flex-1 min-h-10 rounded-lg bg-gray-700 px-3 text-lg"
                        autoFocus
                      />
                      <button onClick={() => { if (editVal.trim()) rename(p.key, editVal.trim()); setEditKey(null); }}
                        className="min-h-10 px-3 rounded-lg bg-green-600 active:scale-95">✓</button>
                      <button onClick={() => setEditKey(null)}
                        className="min-h-10 px-3 rounded-lg bg-gray-600 active:scale-95">✗</button>
                    </>
                  ) : (
                    <>
                      <span className="text-lg font-semibold flex-1">{p.nickname}</span>
                      {teams >= 2 && (
                        <button onClick={() => cycleTeam(p.key)} title="Changer d'équipe"
                          className="min-h-10 px-3 rounded-lg bg-red-600/30 border border-red-600 text-sm font-bold active:scale-95">
                          Éq.{p.team}
                        </button>
                      )}
                      {p.guest && (
                        <>
                          <span className="text-xs text-gray-400">invité</span>
                          <button onClick={() => reroll(p.key)} title="Autre nom"
                            className="min-h-10 px-3 rounded-lg bg-gray-700 active:scale-95">🎲</button>
                        </>
                      )}
                      <button onClick={() => { setEditKey(p.key); setEditVal(p.nickname); }} title="Renommer"
                        className="min-h-10 px-3 rounded-lg bg-gray-700 active:scale-95">✎</button>
                      <button onClick={() => remove(p.key)}
                        className="min-h-10 px-3 rounded-lg bg-gray-700 active:scale-95">✕</button>
                    </>
                  )}
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Joueurs enregistrés (tap pour ajouter) + invité */}
        <div>
          <div className="text-sm text-gray-400 mb-1">Joueurs enregistrés</div>
          <div className="flex flex-wrap gap-2">
            {available.map((p) => (
              <button key={p.id} onClick={() => addDb(p)}
                className="min-h-11 px-4 rounded-lg bg-gray-800 border border-gray-700 active:scale-95">
                {p.nickname}
              </button>
            ))}
            <button onClick={addGuest}
              className="min-h-11 px-4 rounded-lg bg-blue-600 font-semibold active:scale-95">
              ➕ Invité
            </button>
          </div>
          {available.length === 0 && dbPlayers.length === 0 && (
            <p className="text-gray-500 text-xs mt-1">
              Aucun joueur enregistré pour l’instant — les invités sont sauvegardés en fin de partie.
            </p>
          )}
        </div>

        {/* Réglages spécifiques au mode */}
        {mode === "X01" && (
          <>
            <div className="flex gap-2">
              {[301, 501, 701].map((sc) => (
                <button key={sc} onClick={() => setScore(sc)}
                  className={`flex-1 min-h-12 rounded-lg text-lg font-bold ${score === sc ? "bg-red-600" : "bg-gray-800"}`}>
                  {sc}
                </button>
              ))}
            </div>
            <label className="flex items-center gap-3 text-lg">
              <input type="checkbox" checked={doubleOut}
                onChange={(e) => setDoubleOut(e.target.checked)} className="h-6 w-6" />
              Double out
            </label>
          </>
        )}

        {mode === "CRICKET" && (
          <div className="flex flex-col gap-3">
            <label className="flex items-center gap-3 text-lg">
              <input type="checkbox" checked={cutThroat}
                onChange={(e) => setCutThroat(e.target.checked)} className="h-6 w-6" />
              Cut-throat <span className="text-sm text-gray-400">(les points vont aux adversaires, le plus bas gagne)</span>
            </label>
            <label className="flex items-center gap-3 text-lg">
              <input type="checkbox" checked={useBull}
                onChange={(e) => setUseBull(e.target.checked)} className="h-6 w-6" />
              Inclure le Bull (25)
            </label>
          </div>
        )}

        {mode === "ROUND_THE_CLOCK" && (
          <p className="text-sm text-gray-400">
            Touchez 1 → 20 dans l&apos;ordre. N&apos;importe quel impact (simple, double, triple) sur la cible fait avancer. Premier à finir gagne.
          </p>
        )}

        {/* Équipes */}
        <div>
          <div className="text-sm text-gray-400 mb-1">Équipes</div>
          <div className="flex gap-2">
            {[{ t: 0, l: "Chacun pour soi" }, { t: 2, l: "2 équipes" }, { t: 3, l: "3" }, { t: 4, l: "4" }].map(({ t, l }) => (
              <button key={t} onClick={() => setTeamCount(t)}
                className={`flex-1 min-h-12 rounded-lg font-bold ${teams === t ? "bg-red-600" : "bg-gray-800"}`}>
                {l}
              </button>
            ))}
          </div>
        </div>

        <button
          disabled={lineup.length === 0}
          onClick={() => {
            const teamCount = teams >= 2 ? teams : 1;
            const players = lineup.map((p) => ({ nickname: p.nickname, team: teams >= 2 ? p.team : 0 }));
            const opts =
              mode === "CRICKET"
                ? { mode, cutThroat, useBull, legs: 1, teams: teamCount }
                : mode === "ROUND_THE_CLOCK"
                ? { mode, legs: 1, teams: teamCount }
                : { mode: "X01", startingScore: score, outType: doubleOut ? "DOUBLE" : "ANY", legs: 1, teams: teamCount };
            onStart(players, opts);
          }}
          className={`min-h-16 rounded-lg text-xl font-bold active:scale-95 ${
            lineup.length === 0 ? "bg-gray-700 opacity-40" : "bg-green-600"
          }`}
        >
          Démarrer
        </button>
      </div>
    </div>
  );
}
