"use client";

/**
 * Classement + historique récent — lus depuis le serveur autoritatif
 * (commandes get_leaderboard / get_history). Lecture seule, tactile.
 */

import { useEffect } from "react";
import Link from "next/link";
import { useDartStore } from "@/store/dartStore";

export default function LeaderboardPage() {
  const connect = useDartStore((s) => s.connect);
  const connected = useDartStore((s) => s.connected);
  const getLeaderboard = useDartStore((s) => s.getLeaderboard);
  const getHistory = useDartStore((s) => s.getHistory);
  const leaderboard = useDartStore((s) => s.leaderboard);
  const history = useDartStore((s) => s.history);

  useEffect(() => {
    connect();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (connected) {
      getLeaderboard();
      getHistory();
    }
  }, [connected, getLeaderboard, getHistory]);

  return (
    <div className="min-h-screen bg-black text-white p-4 flex flex-col gap-4">
      <div className="flex items-center justify-between">
        <h1 className="text-3xl font-bold">Statistiques</h1>
        <div className="flex items-center gap-3">
          <span className={`h-3 w-3 rounded-full ${connected ? "bg-green-500" : "bg-red-600"}`} />
          <Link href="/live" className="min-h-12 px-5 flex items-center rounded-lg bg-red-600 font-bold active:scale-95">
            ← Jeu
          </Link>
        </div>
      </div>

      {/* Classement */}
      <section className="rounded-xl bg-gray-900 border border-gray-700 p-4">
        <h2 className="text-xl font-bold mb-3">🏆 Classement</h2>
        {leaderboard.length === 0 ? (
          <p className="text-gray-400">Aucun joueur enregistré pour le moment.</p>
        ) : (
          <div className="overflow-auto">
            <table className="w-full text-left">
              <thead className="text-gray-400 text-sm">
                <tr>
                  <th className="py-2 pr-3">#</th>
                  <th className="py-2 pr-3">Joueur</th>
                  <th className="py-2 pr-3 text-right">Victoires</th>
                  <th className="py-2 pr-3 text-right">Parties</th>
                  <th className="py-2 pr-3 text-right">% Victoire</th>
                  <th className="py-2 text-right">Moy./fléch.</th>
                </tr>
              </thead>
              <tbody>
                {leaderboard.map((r, i) => (
                  <tr key={r.id} className={`border-t border-gray-800 ${i === 0 ? "text-yellow-300" : ""}`}>
                    <td className="py-3 pr-3 text-xl font-bold">{i + 1}</td>
                    <td className="py-3 pr-3 text-lg font-semibold">{r.nickname}</td>
                    <td className="py-3 pr-3 text-right text-lg">{r.totalWins}</td>
                    <td className="py-3 pr-3 text-right">{r.totalGames}</td>
                    <td className="py-3 pr-3 text-right">{(r.winRate * 100).toFixed(0)}%</td>
                    <td className="py-3 text-right">{r.averageScore.toFixed(1)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </section>

      {/* Historique récent */}
      <section className="rounded-xl bg-gray-900 border border-gray-700 p-4">
        <h2 className="text-xl font-bold mb-3">📜 Parties récentes</h2>
        {history.length === 0 ? (
          <p className="text-gray-400">Aucune partie terminée pour le moment.</p>
        ) : (
          <div className="flex flex-col gap-2">
            {history.map((g) => (
              <div key={g.id} className="flex items-center justify-between rounded-lg bg-gray-800 px-4 py-3">
                <div>
                  <div className="font-semibold">
                    {g.mode} {g.startingScore > 0 ? `· ${g.startingScore}` : ""}
                  </div>
                  <div className="text-sm text-gray-400">{g.players}</div>
                </div>
                <div className="text-right">
                  {g.winnerNickname ? (
                    <div className="text-green-400 font-bold">🏆 {g.winnerNickname}</div>
                  ) : (
                    <div className="text-gray-500">{g.status}</div>
                  )}
                  <div className="text-xs text-gray-500">{g.finishedAt}</div>
                </div>
              </div>
            ))}
          </div>
        )}
      </section>
    </div>
  );
}
