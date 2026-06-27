

/**
 * Types partagés pour la logique X01 (et autres modes à venir)
 */

/* Un lancer individuel */
export type Throw = {
  /** Valeur 1‑20, 25 (outer bull) ou 50 (inner bull) */
  value: number;
  /** Multiplicateur 1 (simple), 2 (double), 3 (triple) */
  multiplier: 1 | 2 | 3;

  bust?: boolean; // true si le lancer est un bust (dépassement de score)
};

/* État d’un joueur pendant une partie */
export type PlayerState = {
  id: number;
  nickname: string;
  /** Score restant (ex. 501 → 0) */
  score: number;
  /** Legs gagnés dans le match en cours */
  legsWon: number;
  /** Historique complet de tous ses lancers */
  throws: Throw[];
};

/* Configuration spécifique au jeu X01 */
export type OptionsX01 = {
  startingScore: number;              // 301 / 501 …
  inType: "ANY" | "DOUBLE" | "TRIPLE"; // Double In, Triple In, Libre
  outType: "ANY" | "DOUBLE" | "TRIPLE";// Double/Triple Out, Libre
  scoringMode: "BEST_OF" | "FIRST_TO"; // Format des manches
  legs: number;                       // nombre de legs (bestOf/firstTo)
  allowBust: boolean;                 // true = dépassement autorisé
  teams: number;                      // 1 = solo, ≥2 = équipes
};

/* État global d'une partie (stocké dans React Context) */
export type GameState = {
  options: OptionsX01;
  players: PlayerState[];
  /** Index du joueur dont c'est le tour */
  currentIndex: number;
  /** Numéro de fléchette dans le tour (0‑2) */
  dartIndex: number;
  /** Historique des tours (tableau de tableau de Throw) */
  turns: Throw[][];
  /** ID du vrai gagnant (premier à finir) - null si pas encore de gagnant */
  winner: number | null;
  /** Ordered list of finished player IDs for ranking (including the winner) */
  finishedPlayers: number[];
  /** Is the game completely over (all players finished or only one left) */
  gameOver: boolean;
  /** Historique des états précédents pour l'undo (max 20 états) */
  stateHistory?: GameState[];
};