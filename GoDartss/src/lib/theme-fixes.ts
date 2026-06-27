/**
 * CORRECTIONS SPÉCIFIQUES - Système de Thème GoDarts
 * 
 * Ce fichier contient les corrections à appliquer aux composants existants
 * pour les aligner avec le nouveau système de thème unifié.
 */

/* ================================================================ */
/* MAPPING DES CORRECTIONS                                          */
/* ================================================================ */

export const THEME_CORRECTIONS = {
  
  /* Messages et indicateurs d'état */
  statusMessages: {
    // Remplacement des couleurs bleues pour le joueur actuel
    before: 'bg-blue-100 border border-blue-300 text-blue-800',
    after: 'bg-red-50 border border-red-200 text-red-800',
    
    // Point d'animation pour le joueur actuel
    indicatorBefore: 'bg-blue-500 animate-pulse',
    indicatorAfter: 'bg-red-500 animate-pulse',
  },

  /* Messages de victoire */
  winnerMessages: {
    // Remplacement des couleurs jaunes/amber
    before: 'bg-yellow-100 border border-yellow-300 text-yellow-800',
    after: 'bg-red-100 border border-red-300 text-red-900',
    
    // Badges de victoire
    badgeBefore: 'bg-yellow-100 text-yellow-800',
    badgeAfter: 'bg-red-100 text-red-800',
  },

  /* Messages de fin de partie */
  endGameMessages: {
    // Messages de partie terminée en gris neutre
    before: 'bg-green-100 border border-green-300 text-green-800',
    after: 'bg-gray-100 border border-gray-300 text-gray-800',
  },

  /* Boutons d'action */
  buttons: {
    // Boutons principaux en rouge
    primaryBefore: 'bg-blue-600 hover:bg-blue-700 text-white',
    primaryAfter: 'bg-red-600 hover:bg-red-700 text-white',
    
    // Boutons de validation
    successBefore: 'bg-green-500 hover:bg-green-600 text-white',
    successAfter: 'bg-red-600 hover:bg-red-700 text-white',
    
    // Boutons destructifs (conservent le rouge)
    destructiveBefore: 'bg-red-500 hover:bg-red-600 text-white',
    destructiveAfter: 'bg-red-600 hover:bg-red-700 text-white',
  },

  /* États actifs */
  activeStates: {
    // Lignes actives dans les tableaux
    before: 'bg-primary/20',
    after: 'bg-red-600/10',
    
    // Éléments au hover
    hoverBefore: 'hover:bg-blue-100',
    hoverAfter: 'hover:bg-gray-50',
  },

  /* Textes de statut */
  statusTexts: {
    // Liens et textes d'accent
    accentBefore: 'text-blue-600',
    accentAfter: 'text-red-600',
    
    // Textes secondaires
    mutedBefore: 'text-gray-500',
    mutedAfter: 'text-gray-600', // Légèrement plus foncé pour améliorer le contraste
  }
} as const;

/* ================================================================ */
/* FONCTIONS D'AIDE POUR LES CORRECTIONS                           */
/* ================================================================ */

/**
 * Applique les corrections de thème à une chaîne de classes CSS
 */
export function applyThemeCorrections(classString: string): string {
  let correctedClasses = classString;

  // Messages d'état
  correctedClasses = correctedClasses.replace(
    /bg-blue-100 border border-blue-300 text-blue-800/g,
    THEME_CORRECTIONS.statusMessages.after
  );

  // Indicateurs d'animation
  correctedClasses = correctedClasses.replace(
    /bg-blue-500 animate-pulse/g,
    THEME_CORRECTIONS.statusMessages.indicatorAfter
  );

  // Messages de victoire
  correctedClasses = correctedClasses.replace(
    /bg-yellow-100 border border-yellow-300 text-yellow-800/g,
    THEME_CORRECTIONS.winnerMessages.after
  );

  // Messages de fin
  correctedClasses = correctedClasses.replace(
    /bg-green-100 border border-green-300 text-green-800/g,
    THEME_CORRECTIONS.endGameMessages.after
  );

  // Boutons
  correctedClasses = correctedClasses.replace(
    /bg-blue-600 hover:bg-blue-700/g,
    'bg-red-600 hover:bg-red-700'
  );

  correctedClasses = correctedClasses.replace(
    /bg-green-500 hover:bg-green-600/g,
    'bg-red-600 hover:bg-red-700'
  );

  // États actifs
  correctedClasses = correctedClasses.replace(
    /bg-primary\/20/g,
    'bg-red-600/10'
  );

  correctedClasses = correctedClasses.replace(
    /hover:bg-blue-100/g,
    'hover:bg-gray-50'
  );

  // Textes
  correctedClasses = correctedClasses.replace(
    /text-blue-600/g,
    'text-red-600'
  );

  return correctedClasses;
}

/**
 * Génère les classes pour un composant de message selon son type
 */
export function getMessageClasses(type: 'current' | 'winner' | 'finished' | 'error' | 'info'): string {
  const baseClasses = 'p-3 rounded-lg border';
  
  switch (type) {
    case 'current':
      return `${baseClasses} bg-red-50 border-red-200 text-red-800`;
    case 'winner':
      return `${baseClasses} bg-red-100 border-red-300 text-red-900`;
    case 'finished':
      return `${baseClasses} bg-gray-100 border-gray-300 text-gray-800`;
    case 'error':
      return `${baseClasses} bg-red-100 border-red-300 text-red-800`;
    case 'info':
    default:
      return `${baseClasses} bg-gray-100 border-gray-300 text-gray-800`;
  }
}

/**
 * Génère les classes pour un badge selon son type
 */
export function getBadgeClasses(type: 'winner' | 'current' | 'finished' | 'neutral'): string {
  const baseClasses = 'inline-flex items-center px-2 py-1 rounded-full text-xs font-medium';
  
  switch (type) {
    case 'winner':
      return `${baseClasses} bg-red-100 text-red-800`;
    case 'current':
      return `${baseClasses} bg-red-50 text-red-700`;
    case 'finished':
      return `${baseClasses} bg-gray-100 text-gray-700`;
    case 'neutral':
    default:
      return `${baseClasses} bg-gray-100 text-gray-600`;
  }
}

/* ================================================================ */
/* GUIDE DE MIGRATION PAR COMPOSANT                                */
/* ================================================================ */

export const COMPONENT_MIGRATION_GUIDE = {
  'EnhancedScoreboard': {
    description: 'Indicateurs de tour et de victoire',
    changes: [
      'bg-blue-100 border border-blue-300 → bg-red-50 border border-red-200',
      'text-blue-800 → text-red-800',
      'bg-blue-500 animate-pulse → bg-red-500 animate-pulse',
      'bg-yellow-100 border border-yellow-300 → bg-red-100 border border-red-300',
      'text-yellow-800 → text-red-900',
    ]
  },
  
  'UndoButton': {
    description: 'Bouton d\'annulation',
    changes: [
      'bg-red-600 hover:bg-red-700 → (conservé, déjà correct)',
      'bg-gray-200 text-gray-400 → (conservé pour l\'état désactivé)',
    ]
  },
  
  'EditableThrow': {
    description: 'Composant d\'édition de lancers',
    changes: [
      'bg-green-500 hover:bg-green-600 → bg-red-600 hover:bg-red-700',
      'bg-red-500 hover:bg-red-600 → bg-red-600 hover:bg-red-700',
      'hover:bg-blue-100 → hover:bg-gray-50',
    ]
  },
  
  'WinnerCelebration': {
    description: 'Animation de victoire',
    changes: [
      'Gradient jaune/orange → Gradient rouge',
      'text-yellow-* → text-red-*',
      'border-yellow-* → border-red-*',
    ]
  },
  
  'WinnerScoreboard': {
    description: 'Tableau des gagnants',
    changes: [
      'bg-yellow-50 to-amber-50 → bg-red-50',
      'border-yellow-200 → border-red-200',
      'text-yellow-800 → text-red-800',
      'bg-yellow-100 text-yellow-800 → bg-red-100 text-red-800',
    ]
  }
} as const;