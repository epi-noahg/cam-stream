/**
 * Hook useTheme - Facilite l'utilisation du système de thème unifié
 */

import { useMemo } from 'react';
import { 
  THEME_CLASSES, 
  getButtonClasses, 
  getStateClasses, 
  getIndicatorClasses,
  composeTheme 
} from '@/lib/theme';
import { 
  getMessageClasses, 
  getBadgeClasses, 
  applyThemeCorrections 
} from '@/lib/theme-fixes';

export interface UseThemeReturn {
  // Classes de base
  classes: typeof THEME_CLASSES;
  
  // Fonctions utilitaires
  getButton: typeof getButtonClasses;
  getState: typeof getStateClasses;
  getIndicator: typeof getIndicatorClasses;
  getMessage: typeof getMessageClasses;
  getBadge: typeof getBadgeClasses;
  compose: typeof composeTheme;
  correct: typeof applyThemeCorrections;
  
  // Classes couramment utilisées
  common: {
    card: string;
    cardTitle: string;
    input: string;
    label: string;
    link: string;
    divider: string;
  };
  
  // Classes pour les états de jeu
  game: {
    currentPlayer: string;
    winner: string;
    finished: string;
    activeRow: string;
    inactiveRow: string;
  };
}

/**
 * Hook principal pour utiliser le système de thème
 */
export function useTheme(): UseThemeReturn {
  const common = useMemo(() => ({
    card: 'bg-white border border-gray-200 rounded-lg shadow-sm',
    cardTitle: 'text-lg font-semibold text-black mb-2',
    input: 'w-full px-3 py-2 border border-gray-300 rounded-md focus:border-red-500 focus:ring-1 focus:ring-red-500 text-black',
    label: 'block text-sm font-medium text-gray-700 mb-1',
    link: 'text-red-600 hover:text-red-700 underline transition-colors',
    divider: 'border-t border-gray-200 my-4',
  }), []);

  const game = useMemo(() => ({
    currentPlayer: getStateClasses('current', 'p-3 rounded-lg border'),
    winner: getStateClasses('winner', 'p-4 rounded-xl border'),
    finished: getStateClasses('finished', 'p-3 rounded-lg border'),
    activeRow: getStateClasses('active'),
    inactiveRow: 'opacity-75',
  }), []);

  return {
    classes: THEME_CLASSES,
    getButton: getButtonClasses,
    getState: getStateClasses,
    getIndicator: getIndicatorClasses,
    getMessage: getMessageClasses,
    getBadge: getBadgeClasses,
    compose: composeTheme,
    correct: applyThemeCorrections,
    common,
    game,
  };
}

/**
 * Hook spécialisé pour les composants de jeu
 */
export function useGameTheme() {
  const theme = useTheme();
  
  return {
    ...theme,
    
    // Classes spécifiques au jeu
    scoreboard: {
      table: 'min-w-full border border-gray-300 text-sm',
      header: 'border border-gray-300 px-2 py-1 bg-gray-50 font-medium text-black',
      cell: 'border border-gray-300 px-2 py-1',
      activePlayerRow: theme.game.activeRow,
      finishedPlayerRow: 'bg-gray-50 opacity-75',
    },
    
    // Messages de statut du jeu
    status: {
      currentTurn: theme.getMessage('current'),
      gameWon: theme.getMessage('winner'),
      gameFinished: theme.getMessage('finished'),
    },
    
    // Boutons spécifiques au jeu
    gameButtons: {
      primary: theme.getButton('primary'),
      secondary: theme.getButton('secondary'),
      undo: theme.getButton('danger'),
      disabled: theme.getButton('primary', true),
    },
  };
}

/**
 * Hook pour les animations et transitions
 */
export function useThemeAnimations() {
  return {
    // Transitions communes
    transition: 'transition-all duration-200 ease-in-out',
    transitionColors: 'transition-colors duration-200',
    transitionTransform: 'transition-transform duration-200',
    
    // Animations de pulse
    pulseRed: 'pulse-red',
    animatePulse: 'animate-pulse',
    
    // Effets de hover
    hoverLift: 'hover-lift hover:shadow-md',
    hoverScale: 'hover:scale-105 transition-transform duration-200',
    
    // Focus
    focusRing: 'focus:outline-none focus:ring-2 focus:ring-red-500 focus:ring-opacity-50',
    focusVisible: 'focus-visible-red',
  };
}

/**
 * Hook pour les classes responsive
 */
export function useResponsiveTheme() {
  return {
    // Layout responsive
    container: 'container mx-auto px-4 sm:px-6 lg:px-8',
    grid: 'grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4',
    flexStack: 'flex flex-col space-y-4',
    flexInline: 'flex flex-col sm:flex-row sm:space-x-4 sm:space-y-0 space-y-4',
    
    // Texte responsive
    headingLarge: 'text-2xl sm:text-3xl lg:text-4xl font-bold text-black',
    headingMedium: 'text-xl sm:text-2xl font-semibold text-black',
    headingSmall: 'text-lg font-medium text-black',
    
    // Espacement responsive
    paddingResponsive: 'p-4 sm:p-6 lg:p-8',
    marginResponsive: 'm-4 sm:m-6 lg:m-8',
  };
}