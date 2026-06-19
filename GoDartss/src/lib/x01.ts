import { GameState, Throw, OptionsX01 } from "@/types/game";

/* ------------ Helpers ------------ */

/** Returns true if the throw satisfies the given in/out type rule */
function matchesMultiplier(
  thr: Throw,
  requirement: "ANY" | "DOUBLE" | "TRIPLE"
): boolean {
  if (requirement === "ANY") return true;
  if (requirement === "DOUBLE") return thr.multiplier === 2 || thr.value === 50; // inner bull counts as double
  if (requirement === "TRIPLE") return thr.multiplier === 3;
  return true;
}

/** Bust rules for classic X01 */
function isBust(
  currentScore: number,
  hitValue: number,
  thr: Throw,
  opts: OptionsX01
): boolean {
  const newScore = currentScore - hitValue;

  // Negative or 1 left = bust
  if (newScore < 0 || newScore === 1) return true;

  // Reaches 0 but doesn't satisfy out-type → bust (unless bust disabled)
  if (
    newScore === 0 &&
    !matchesMultiplier(thr, opts.outType) &&
    !opts.allowBust
  )
    return true;

  return false;
}

/* ------------ Core reducer helper ------------ */

// Helper function to find next player who hasn't finished
function findNextActivePlayer(state: GameState, startIndex: number): number {
  let nextIndex = startIndex;
  let attempts = 0;
  
  while (attempts < state.players.length) {
    const player = state.players[nextIndex];
    if (!state.finishedPlayers.includes(player.id)) {
      return nextIndex;
    }
    nextIndex = (nextIndex + 1) % state.players.length;
    attempts++;
  }
  
  // All players have finished, return current index
  return state.currentIndex;
}


export function applyThrow(
  state: GameState,
  thr: Throw
): { state: GameState; bust: boolean; finished: boolean } {
  const opts = state.options;
  const player = state.players[state.currentIndex];
  
  // Check if current player has already finished
  if (state.finishedPlayers.includes(player.id)) {
    // Player has already finished, ignore the throw
    return { state, bust: false, finished: false };
  }
  
  // If this throw is already marked as bust, ignore it completely for scoring
  if (thr.bust) {
    // Still add it to throws history for display, but don't change score
    const newState: GameState = {
      ...state,
      players: state.players.map((p) =>
        p.id === player.id ? { ...p, throws: [...p.throws, thr] } : p
      ),
      dartIndex: state.dartIndex + 1,
    };
    return { state: newState, bust: true, finished: false };
  }
  
  const hitValue =
    thr.value === 25 || thr.value === 50 ? thr.value : thr.value * thr.multiplier;

  // Check if player is "in" (for double/triple in). We store a flag on player via throws length >0 OR separate flag.
  const playerOpened =
    opts.inType === "ANY" ||
    player.score !== opts.startingScore || // already scored before
    matchesMultiplier(thr, opts.inType);

  // If not opened yet and hit doesn't match inType -> ignore (no score change)
  if (!playerOpened) {
    const newState: GameState = {
      ...state,
      players: state.players.map((p) =>
        p.id === player.id ? { ...p, throws: [...p.throws, thr] } : p
      ),
      dartIndex: state.dartIndex + 1,
    };
    return { state: newState, bust: false, finished: false };
  }

  // Evaluate bust
  const bust = !opts.allowBust && isBust(player.score, hitValue, thr, opts);

  if (bust) {
    // Calculer le score au DÉBUT de la manche courante
    const currentTurn = state.turns[state.turns.length - 1] || [];
    let scoreAtStartOfTurn = player.score;
    
    // Ajouter les points des throws déjà joués dans ce tour
    for (const turnThrow of currentTurn) {
      if (!turnThrow.bust) {
        const turnHitValue = turnThrow.value === 25 || turnThrow.value === 50 
          ? turnThrow.value 
          : turnThrow.value * turnThrow.multiplier;
        scoreAtStartOfTurn += turnHitValue;
      }
    }
    
    const bustedThrow = { ...thr, bust: true };

    // Ajouter le throw de bust au tour actuel pour l'historique
    const lastIdx = state.turns.length - 1;
    const updatedTurns =
      lastIdx >= 0
        ? [...state.turns.slice(0, lastIdx), [...state.turns[lastIdx], bustedThrow], []]
        : [[bustedThrow], []];

    // RESTAURER le score au début de la manche
    const updatedPlayer = { 
      ...player, 
      score: scoreAtStartOfTurn,
      throws: [...player.throws, bustedThrow] 
    };
    
    // Find next player who hasn't won
    const nextPlayerIndex = findNextActivePlayer(state, (state.currentIndex + 1) % state.players.length);
    
    const nextState: GameState = {
      ...state,
      players: state.players.map((p) =>
        p.id === player.id ? updatedPlayer : p
      ),
      currentIndex: nextPlayerIndex,
      dartIndex: 0,
      turns: updatedTurns,
    };
    return { state: nextState, bust: true, finished: false };
  }

  // Normal scoring
  const newScore = player.score - hitValue;
  const finished =
    newScore === 0 && matchesMultiplier(thr, opts.outType) && !bust;

  // Check if player wins the leg
  let updatedPlayers = [...state.players];
  let legWon = false;
  
  if (finished) {
    // Only award a leg win if no one has won yet (first to finish wins the leg)
    const shouldWinLeg = state.winner === null;
    
    // Update player 
    const updatedPlayer = {
      ...player,
      score: newScore,
      throws: [...player.throws, thr],
      legsWon: shouldWinLeg ? player.legsWon + 1 : player.legsWon, // Only first winner gets the leg
    };
    
    updatedPlayers = updatedPlayers.map((p) =>
      p.id === player.id ? updatedPlayer : p
    );
    
    legWon = shouldWinLeg;
  } else {
    // Normal update
    const updatedPlayer = {
      ...player,
      score: newScore,
      throws: [...player.throws, thr],
    };
    
    updatedPlayers = updatedPlayers.map((p) =>
      p.id === player.id ? updatedPlayer : p
    );
  }

  let nextState: GameState = {
    ...state,
    players: updatedPlayers,
    dartIndex: (state.dartIndex + 1) % 3,
  };

  // Check if game is won (based on legs needed to win)
  const gameWon = legWon && updatedPlayers.some(p => p.legsWon >= state.options.legs);

  // End of turn (3 darts) or finished
  const endOfTurn = nextState.dartIndex === 0 || finished || gameWon;

  if (endOfTurn) {
    // Add thr to the current turn, then prepare an empty array for next player
    const lastIdx = nextState.turns.length - 1;
    const completedTurn =
      lastIdx >= 0
        ? [...nextState.turns[lastIdx], thr]
        : [thr];
    const updatedTurns = [
      ...nextState.turns.slice(0, lastIdx >= 0 ? lastIdx : 0),
      completedTurn,
    ];

    // If the game isn't finished, start an empty turn for next player
    if (!finished && !gameWon) {
      updatedTurns.push([]);
    }

    // Find next player who hasn't won
    const nextPlayerIndex = finished || gameWon
      ? state.currentIndex
      : findNextActivePlayer(state, (state.currentIndex + 1) % state.players.length);
    
    nextState = {
      ...nextState,
      currentIndex: nextPlayerIndex,
      turns: updatedTurns,
      dartIndex: 0,
    };
  } else {
    // Add thr to the in‑progress turn (immutable)
    const lastIdx = nextState.turns.length - 1;
    const updatedTurns =
      lastIdx >= 0
        ? [
            ...nextState.turns.slice(0, lastIdx),
            [...nextState.turns[lastIdx], thr],
          ]
        : [[thr]];

    nextState = { ...nextState, turns: updatedTurns };
  }

  return { state: nextState, bust: false, finished };
}