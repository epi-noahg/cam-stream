import { Throw } from "@/types/game";

// Créer les throws triés par valeur décroissante pour optimiser la recherche
function createOptimalThrowsOrder(): Throw[] {
  const throws: Throw[] = [];
  
  // Ajouter tous les throws possibles avec leur valeur calculée
  const throwsWithValue: Array<Throw & { hitValue: number }> = [];
  
  // Singles, doubles et triples pour 1-20
  for (let v = 1; v <= 20; v++) {
    throwsWithValue.push({ value: v, multiplier: 1, hitValue: v });
    throwsWithValue.push({ value: v, multiplier: 2, hitValue: v * 2 });
    throwsWithValue.push({ value: v, multiplier: 3, hitValue: v * 3 });
  }
  
  // Bulls
  throwsWithValue.push({ value: 25, multiplier: 1, hitValue: 25 }); // outer bull
  throwsWithValue.push({ value: 50, multiplier: 1, hitValue: 50 }); // inner bull
  
  // Trier par valeur décroissante pour favoriser les solutions avec moins de flèches
  throwsWithValue.sort((a, b) => b.hitValue - a.hitValue);
  
  // Retourner sans la propriété hitValue
  return throwsWithValue.map(({ hitValue, ...thr }) => thr);
}

const allThrows = createOptimalThrowsOrder();

function isValidFinish(thr: Throw, outType: "ANY" | "DOUBLE" | "TRIPLE"): boolean {
  switch (outType) {
    case "ANY":
      return true; // N'importe quel lancer peut finir
    case "DOUBLE":
      return thr.multiplier === 2 || thr.value === 50; // Double ou inner bull
    case "TRIPLE":
      return thr.multiplier === 3; // Triple seulement
    default:
      return false;
  }
}

// Nouvelle fonction pour trouver la solution optimale (minimum de flèches)
function findOptimalCheckout(score: number, maxDarts: number, outType: "ANY" | "DOUBLE" | "TRIPLE" = "DOUBLE"): Throw[] | null {
  let bestSolution: Throw[] | null = null;
  
  // Essayer toutes les combinaisons possibles en commençant par le minimum de flèches
  for (let numDarts = 1; numDarts <= maxDarts; numDarts++) {
    const solution = backtrack(score, [], numDarts, outType);
    if (solution) {
      bestSolution = solution;
      break; // On a trouvé la solution avec le minimum de flèches
    }
  }
  
  return bestSolution;
}

// Backtracking limité au nombre exact de lancers
function backtrack(
  score: number,
  path: Throw[],
  exactDarts: number,
  outType: "ANY" | "DOUBLE" | "TRIPLE"
): Throw[] | null {
  if (path.length === exactDarts) {
    // On doit avoir utilisé exactement le nombre de flèches et finir à 0
    if (score === 0 && isValidFinish(path[path.length - 1], outType)) {
      return path;
    }
    return null;
  }
  
  if (score <= 0) return null;

  for (const thr of allThrows) {
    const hitValue =
      thr.value === 25 || thr.value === 50 ? thr.value : thr.value * thr.multiplier;
    
    // Ne pas dépasser le score
    if (hitValue > score) continue;
    
    // Si c'est la dernière flèche, elle doit respecter les règles de finish ET finir exactement à 0
    if (path.length === exactDarts - 1) {
      if (hitValue === score && isValidFinish(thr, outType)) {
        return [...path, thr];
      }
      continue;
    }
    
    const res = backtrack(score - hitValue, [...path, thr], exactDarts, outType);
    if (res) return res;
  }
  return null;
}

export function getCheckoutSuggestion(score: number, dartsLeft: number, outType: "ANY" | "DOUBLE" | "TRIPLE" = "DOUBLE"): Throw[] | null {
  if (score < 2 || score > 170) return null; // hors plage classique
  const maxDarts = Math.min(dartsLeft, 3);
  return findOptimalCheckout(score, maxDarts, outType);
}

/**
 * Check if a player can checkout with their current score
 * @param score The player's current score
 * @param outType The out type required ("ANY" | "DOUBLE" | "TRIPLE")
 * @returns true if the player can checkout, false otherwise
 */
export function canCheckout(score: number, outType: "ANY" | "DOUBLE" | "TRIPLE"): boolean {
  // Scores that are impossible to checkout
  if (score === 1 && outType !== "ANY") return false;
  
  // For "ANY" out type, any score > 0 can be checked out (given enough darts)
  if (outType === "ANY") return score > 0;
  
  // For "DOUBLE" and "TRIPLE" out types, use the optimal checkout function
  return findOptimalCheckout(score, 3, outType) !== null;
}