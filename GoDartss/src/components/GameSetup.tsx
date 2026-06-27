"use client";

import React from "react";
import { cn } from "@/lib/utils";
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card";
import { Plus, Edit2, ArrowRight, ArrowLeft, Trophy, Shield, Settings } from "lucide-react";
import { theme } from "@/lib/theme";

////////////////////////////////////////////////////////////////////////////////
// TYPES GÉNÉRIQUES
////////////////////////////////////////////////////////////////////////////////

type Player = {
  id: number;
  nickname: string;
};

export function PlayerTransfer({
  players,
  inGame,
  onAdd,
  onRemove,
  onTeamChange,
  teams,
  onCreate,
  onRename,
}: {
  players: Player[];
  inGame: Map<number, number>;
  onAdd: (id: number) => void;
  onRemove: (id: number) => void;
  onTeamChange: (id: number, team: number) => void;
  teams: number;
  onCreate: (nick: string) => void;
  onRename: (id: number, nickname: string) => void;
}) {
  const [newNick, setNewNick] = React.useState("");
  const [editingId, setEditingId] = React.useState<number | null>(null);
  const [editingName, setEditingName] = React.useState("");
  const commitRename = () => {
    if (editingId !== null && editingName.trim()) {
      onRename(editingId, editingName.trim());
    }
    setEditingId(null);
    setEditingName("");
  };

  const addNew = () => {
    const trimmed = newNick.trim();
    if (!trimmed) return;
    onCreate(trimmed);
    setNewNick("");
  };

  const available = players.filter((p) => !inGame.has(p.id));
  const selected = players.filter((p) => inGame.has(p.id));

  return (
    <div className="grid gap-6 md:grid-cols-2">
      {/* Available players */}
      <Card className={cn(theme.card.default, "border-2")}>
        <CardHeader className="pb-4">
          <CardTitle className={cn("flex items-center gap-2", theme.text.primary)}>
            <Shield className="w-5 h-5" />
            Tous les joueurs
          </CardTitle>
        </CardHeader>
        <CardContent>
          <ul className="space-y-2">
            {available.map((p) => (
              <li key={p.id} className={cn(
                "flex items-center gap-3 p-3 rounded-lg transition-colors",
                "bg-gray-800 border border-gray-600 hover:bg-gray-700"
              )}>
                {editingId === p.id ? (
                  <>
                    <input
                      type="text"
                      value={editingName}
                      onChange={(e) => setEditingName(e.target.value)}
                      onKeyDown={(e) => {
                        if (e.key === "Enter") {
                          e.preventDefault();
                          commitRename();
                        } else if (e.key === "Escape") {
                          setEditingId(null);
                        }
                      }}
                      className={cn(
                        "flex-1 rounded-lg border-2 px-3 py-2",
                        "focus:border-red-500 focus:ring-red-500",
                        theme.text.primary
                      )}
                      autoFocus
                    />
                    <button
                      className={cn(
                        "rounded-lg px-3 py-2 font-medium transition-colors",
                        theme.bg.accent,
                        theme.text.primary,
                        "hover:bg-red-700"
                      )}
                      onClick={(e) => {
                        e.preventDefault();
                        e.stopPropagation();
                        commitRename();
                      }}
                    >
                      ✔
                    </button>
                  </>
                ) : (
                  <>
                    <span className={cn("flex-1 font-medium", theme.text.primary)}>{p.nickname}</span>
                    <button
                      className={cn(
                        "rounded-lg px-3 py-2 text-sm font-medium transition-colors",
                        theme.bg.mutedDark, theme.text.secondary, "hover:bg-gray-600"
                      )}
                      onClick={() => {
                        setEditingId(p.id);
                        setEditingName(p.nickname);
                      }}
                    >
                      <Edit2 className="w-4 h-4" />
                    </button>
                  </>
                )}
                <button
                  className={cn(
                    "rounded-lg px-3 py-2 font-medium transition-all duration-200",
                    theme.bg.accent,
                    theme.text.primary,
                    "hover:bg-red-700 hover:scale-105"
                  )}
                  onClick={() => onAdd(p.id)}
                >
                  <ArrowRight className="w-4 h-4" />
                </button>
              </li>
            ))}
          </ul>

          {/* Quick add */}
          <div className={cn("mt-6 pt-4 border-t", theme.border.primary)}>
            <div className="flex gap-3">
              <input
                type="text"
                className={cn(
                  "flex-1 rounded-lg border-2 px-4 py-3",
                  "focus:border-red-500 focus:ring-red-500",
                  theme.text.primary
                )}
                placeholder="Ajouter un nouveau joueur…"
                value={newNick}
                onChange={(e) => setNewNick(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === "Enter" && newNick.trim()) {
                    addNew();
                  }
                }}
              />
              <button
                onClick={addNew}
                disabled={!newNick.trim()}
                className={cn(
                  "rounded-lg px-4 py-3 font-medium transition-all duration-200 flex items-center gap-2",
                  newNick.trim()
                    ? cn(
                        theme.bg.accent,
                        theme.text.primary,
                        "hover:bg-red-700 hover:scale-105"
                      )
                    : cn(theme.bg.mutedDark, theme.text.muted, "cursor-not-allowed")
                )}
              >
                <Plus className="w-4 h-4" />
                Ajouter
              </button>
            </div>
          </div>
        </CardContent>
      </Card>

      {/* Selected players */}
      <Card className={cn(theme.card.default, "border-2 border-red-500/20")}>
        <CardHeader className="pb-4">
          <CardTitle className={cn("flex items-center gap-2", theme.text.primary)}>
            <Trophy className="w-5 h-5 text-red-600" />
            Joueurs dans la partie
            {selected.length > 0 && (
              <span className={cn(
                "ml-auto px-2 py-1 rounded-full text-xs font-medium",
                theme.bg.accent,
                theme.text.primary
              )}>
                {selected.length}
              </span>
            )}
          </CardTitle>
        </CardHeader>
        <CardContent>
          {selected.length === 0 && (
            <div className="text-center py-8">
              <Trophy className={cn("w-12 h-12 mx-auto mb-3", theme.text.muted)} />
              <p className={cn(theme.text.muted, "text-sm font-medium")}>Aucun joueur sélectionné</p>
              <p className={cn(theme.text.muted, "text-xs mt-1 opacity-75")}>Ajoutez des joueurs depuis la liste de gauche</p>
            </div>
          )}
          <ul className="space-y-2">
            {selected.map((p, index) => (
              <li key={p.id} className={cn(
                "flex items-center gap-3 p-3 rounded-lg transition-colors",
                "bg-gray-800 border-2 border-red-500 hover:bg-gray-700"
              )}>
                <div className={cn(
                  "w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold",
                  theme.bg.accent,
                  theme.text.primary
                )}>
                  {index + 1}
                </div>
                <button
                  className={cn(
                    "rounded-lg px-3 py-2 font-medium transition-all duration-200",
                    "bg-red-500 text-white hover:bg-red-600 hover:scale-105"
                  )}
                  onClick={() => onRemove(p.id)}
                >
                  <ArrowLeft className="w-4 h-4" />
                </button>
                {editingId === p.id ? (
                  <>
                    <input
                      type="text"
                      value={editingName}
                      onChange={(e) => setEditingName(e.target.value)}
                      onKeyDown={(e) => {
                        if (e.key === "Enter") {
                          e.preventDefault();
                          commitRename();
                        } else if (e.key === "Escape") {
                          setEditingId(null);
                        }
                      }}
                      className={cn(
                        "flex-1 rounded-lg border-2 px-3 py-2",
                        "focus:border-red-500 focus:ring-red-500",
                        theme.text.primary
                      )}
                      autoFocus
                    />
                    <button
                      className={cn(
                        "rounded-lg px-3 py-2 font-medium transition-colors",
                        theme.bg.accent,
                        theme.text.primary,
                        "hover:bg-red-700"
                      )}
                      onClick={(e) => {
                        e.preventDefault();
                        e.stopPropagation();
                        commitRename();
                      }}
                    >
                      ✔
                    </button>
                  </>
                ) : (
                  <>
                    <span className={cn("flex-1 font-medium", theme.text.primary)}>{p.nickname}</span>
                    <button
                      className={cn(
                        "rounded-lg px-3 py-2 text-sm font-medium transition-colors",
                        theme.bg.mutedDark, theme.text.secondary, "hover:bg-gray-600"
                      )}
                      onClick={() => {
                        setEditingId(p.id);
                        setEditingName(p.nickname);
                      }}
                    >
                      <Edit2 className="w-4 h-4" />
                    </button>
                  </>
                )}
                {teams > 1 && (
                  <select
                    value={inGame.get(p.id)}
                    onChange={(e) => onTeamChange(p.id, parseInt(e.target.value))}
                    className="rounded border p-1"
                  >
                    {Array.from({ length: teams }, (_, i) => i + 1).map((n) => (
                      <option key={n} value={n}>
                        Équipe {n}
                      </option>
                    ))}
                  </select>
                )}
              </li>
            ))}
          </ul>
        </CardContent>
      </Card>
    </div>
  );
}

////////////////////////////////////////////////////////////////////////////////
// PARAMÈTRES X01 – Réutilisable via props
////////////////////////////////////////////////////////////////////////////////

type X01SettingsProps = {
  startingScore: number;
  onStartingScore: (val: number) => void;

  // Type d'entrée
  inType: "ANY" | "DOUBLE" | "TRIPLE";
  onInType: (val: "ANY" | "DOUBLE" | "TRIPLE") => void;

  // Type de sortie
  outType: "ANY" | "DOUBLE" | "TRIPLE";
  onOutType: (val: "ANY" | "DOUBLE" | "TRIPLE") => void;

  // Format de manche
  scoringMode: "BEST_OF" | "FIRST_TO";
  onScoringMode: (val: "BEST_OF" | "FIRST_TO") => void;
  legs: number;
  onLegs: (val: number) => void;

  // Bust rule
  allowBust: boolean;
  onAllowBust: (val: boolean) => void;

  teams: number;
  onTeams: (val: number) => void;
  maxTeams: number;
};

export function X01Settings({
  startingScore,
  onStartingScore,
  inType,
  onInType,
  outType,
  onOutType,
  scoringMode,
  onScoringMode,
  legs,
  onLegs,
  allowBust,
  onAllowBust,
  teams,
  onTeams,
  maxTeams,
}: X01SettingsProps) {
  return (
    <Card className={cn(theme.card.default, "border-2")}>
      <CardHeader className="pb-4">
        <CardTitle className={cn("flex items-center gap-2", theme.text.primary)}>
          <Settings className="w-5 h-5 text-red-600" />
          Paramètres X01
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-6">
        {/* Score départ */}
        <div>
          <label className="block text-sm font-medium">Score de départ</label>
          <input
            type="number"
            min={101}
            step={100}
            value={startingScore}
            onChange={(e) => onStartingScore(parseInt(e.target.value))}
            className="mt-1 w-full rounded border p-2"
          />
        </div>
        {/* In type */}
        <div>
          <label className="block text-sm font-medium">Type d’entrée</label>
          <select
            value={inType}
            onChange={(e) => onInType(e.target.value as "ANY" | "DOUBLE" | "TRIPLE")}
            className="mt-1 w-full rounded border p-2"
          >
            <option value="ANY">Libre</option>
            <option value="DOUBLE">Double In</option>
            <option value="TRIPLE">Triple In</option>
          </select>
        </div>
        {/* Out type */}
        <div>
          <label className="block text-sm font-medium">Type de sortie</label>
          <select
            value={outType}
            onChange={(e) => onOutType(e.target.value as "ANY" | "DOUBLE" | "TRIPLE")}
            className="mt-1 w-full rounded border p-2"
          >
            <option value="ANY">Libre</option>
            <option value="DOUBLE">Double Out</option>
            <option value="TRIPLE">Triple Out</option>
          </select>
        </div>
        {/* Scoring format */}
        <div className="flex items-center gap-2">
          <select
            value={scoringMode}
            onChange={(e) => onScoringMode(e.target.value as "BEST_OF" | "FIRST_TO")}
            className="rounded border p-2"
          >
            <option value="BEST_OF">Meilleur de</option>
            <option value="FIRST_TO">Premier à</option>
          </select>
          <input
            type="number"
            min={1}
            value={legs}
            onChange={(e) => onLegs(parseInt(e.target.value))}
            className="w-20 rounded border p-2"
          />
          <span>manches</span>
        </div>
        {/* Bust rule */}
        <label className="flex items-center gap-2">
          <input
            type="checkbox"
            checked={allowBust}
            onChange={(e) => onAllowBust(e.target.checked)}
          />
          Autoriser dépassement (pas de bust)
        </label>
        {/* Teams */}
        <div>
          <label className="block text-sm font-medium">Nombre d’équipes</label>
          <input
            type="number"
            min={1}
            max={maxTeams}
            value={teams}
            onChange={(e) => onTeams(parseInt(e.target.value))}
            className={cn("mt-1 w-full rounded border p-2", maxTeams < 2 && "opacity-50")}
            disabled={maxTeams < 2}
          />
          <p className="text-xs text-muted-foreground mt-1">1 = chacun pour soi ; ≥2 pour jouer en équipes.</p>
        </div>
      </CardContent>
    </Card>
  );
}
