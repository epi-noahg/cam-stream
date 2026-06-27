/**
 * Types du protocole WebSocket du serveur de fléchettes (dartserver).
 * Ces formes correspondent 1:1 au JSON émis par le serveur C++ autoritatif.
 */

export type Throw = {
  value: number;        // 1..20, 25, 50
  multiplier: 1 | 2 | 3;
  bust?: boolean;
};

export type InOutType = "ANY" | "DOUBLE" | "TRIPLE";

export type GameMode = "X01" | "CRICKET" | "ROUND_THE_CLOCK";

/**
 * Options émises/acceptées par le serveur, tous modes confondus. Seuls les
 * champs du mode courant (`mode`) sont significatifs ; les autres gardent leur
 * valeur par défaut côté serveur.
 */
export type GameOptions = {
  mode?: GameMode;
  // X01
  startingScore?: number;
  inType?: InOutType;
  outType?: InOutType;
  allowBust?: boolean;
  // Cricket
  cutThroat?: boolean;
  useBull?: boolean;
  // partagés
  scoringMode?: "BEST_OF" | "FIRST_TO";
  legs?: number;
  teams?: number;
};

/** Alias rétro-compatible. */
export type OptionsX01 = GameOptions;

export type PlayerState = {
  id: number;
  nickname: string;
  score: number;
  legsWon: number;
  team: number;
  throws: Throw[];
  /** Cricket : 7 marques (0..3) dans l'ordre [20,19,18,17,16,15,Bull]. */
  marks?: number[];
  /** Around the Clock : cible courante (1..20 ; 21 = terminé). */
  target?: number;
};

export type GameState = {
  mode?: GameMode;
  options: GameOptions;
  players: PlayerState[];
  currentIndex: number;
  dartIndex: number;
  turns: Throw[][];
  winner: number | null;
  finishedPlayers: number[];
  gameOver: boolean;
};

export type CamState = "warmup" | "normal" | "human" | "clean";

export type BoardStatus = {
  cams: { id: number; state: CamState; ready: boolean }[];
  round: { phase: "waiting" | "complete" | "resyncing"; nextDart: number; message: string };
  allReady: boolean;
};

export type DartDetected = {
  throwId: number;
  value: number;
  multiplier: number;
  score: number;
  zone: string;
  confidence: number;
  dartIndex: number;
  needsReview: boolean;
};

export type LeaderboardRow = {
  id: number;
  nickname: string;
  totalGames: number;
  totalWins: number;
  winRate: number;
  averageScore: number;
};

export type PlayerRow = {
  id: number;
  nickname: string;
  totalGames: number;
  totalWins: number;
  totalLosses: number;
};

export type SavedGame = {
  id: string;
  players: string;
  startingScore: number;
  updatedAt: string;
};

export type GameSummary = {
  id: number;
  mode: string;
  status: string;
  winnerId: number | null;
  winnerNickname: string;
  players: string;
  startingScore: number;
  finishedAt: string;
};

// ── Commandes client → serveur ──────────────────────────────────────────────
export type Command =
  | { type: "create_game"; players: { id?: number; nickname: string; team?: number }[]; options: Partial<GameOptions> }
  | { type: "get_saved_games" }
  | { type: "resume_game"; id: string }
  | { type: "manual_throw"; value: number; multiplier: number }
  | { type: "correct_throw"; turnIndex: number; throwIndex: number; value: number; multiplier: number }
  | { type: "clear_board" }
  | { type: "reset_round" }
  | { type: "undo" }
  | { type: "next_player" }
  | { type: "refresh_background" }
  | { type: "get_players" }
  | { type: "get_leaderboard"; limit?: number }
  | { type: "get_history"; limit?: number }
  | { type: "create_player"; nickname: string }
  | { type: "rename_player"; id: number; nickname: string }
  | { type: "delete_player"; id: number };
