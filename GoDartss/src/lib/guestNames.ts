/**
 * Pseudos d'invités "cool" pour éviter la saisie clavier sur la tablette.
 * On en propose un au hasard (non déjà utilisé) ; tap 🎲 pour en relancer un.
 */

const COOL_NAMES = [
  "Le Cobra", "La Foudre", "El Fuego", "Triple Menace", "Madame Bull",
  "Capitaine Checkout", "Dr. Double", "La Flèche", "Le Sniper", "Tonnerre",
  "La Comète", "Maverick", "Le Faucon", "Zorro", "La Panthère",
  "Le Vortex", "Phoenix", "La Tornade", "Le Requin", "Excalibur",
  "Le Dragon", "Ninja", "La Vipère", "Turbo", "Le Lynx",
  "Apollo", "La Tempête", "Goldarts", "Le Mamba", "Bullseye Kid",
];

/** Un pseudo cool non présent dans @p used (insensible à la casse). */
export function randomGuestName(used: Set<string>): string {
  const free = COOL_NAMES.filter((n) => !used.has(n.toLowerCase()));
  if (free.length > 0) return free[Math.floor(Math.random() * free.length)];
  // Tous pris : on suffixe un numéro.
  const base = COOL_NAMES[Math.floor(Math.random() * COOL_NAMES.length)];
  let i = 2;
  while (used.has(`${base} ${i}`.toLowerCase())) i++;
  return `${base} ${i}`;
}
