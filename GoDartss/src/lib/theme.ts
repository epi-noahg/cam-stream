/**
 * Système de thème unifié pour GoDarts
 * Palette : Noir, Blanc, Rouge, Gris
 */

export const colors = {
  // Couleurs principales
  primary: {
    50: '#fef2f2',
    100: '#fee2e2', 
    200: '#fecaca',
    300: '#fca5a5',
    400: '#f87171',
    500: '#ef4444',
    600: '#dc2626', // Rouge principal
    700: '#b91c1c',
    800: '#991b1b',
    900: '#7f1d1d',
  },
  
  // Couleurs neutres (noir/blanc/gris)
  neutral: {
    0: '#ffffff',    // Blanc pur
    50: '#f9fafb',
    100: '#f3f4f6',
    200: '#e5e7eb',
    300: '#d1d5db',
    400: '#9ca3af',
    500: '#6b7280',
    600: '#4b5563',
    700: '#374151',
    800: '#1f2937',
    900: '#111827',
    950: '#030712',
    1000: '#000000',  // Noir pur
  }
} as const;

export const theme = {
  // Backgrounds
  bg: {
    primary: 'bg-black',
    secondary: 'bg-gray-900',
    surface: 'bg-white',
    surfaceDark: 'bg-gray-800',
    accent: 'bg-red-600',
    accentHover: 'bg-red-700',
    muted: 'bg-gray-100',
    mutedDark: 'bg-gray-700',
  },
  
  // Textes
  text: {
    primary: 'text-white',           // Sur fond sombre
    secondary: 'text-gray-300',      // Texte secondaire sur fond sombre
    muted: 'text-gray-500',          // Texte atténué
    onSurface: 'text-black',         // Sur fond clair
    onSurfaceSecondary: 'text-gray-600', // Secondaire sur fond clair
    accent: 'text-red-600',          // Texte rouge
    accentLight: 'text-red-400',     // Rouge plus clair
    success: 'text-green-600',       // Succès
    warning: 'text-yellow-600',      // Avertissement
  },
  
  // Bordures
  border: {
    primary: 'border-gray-700',      // Bordure principale sur fond sombre
    secondary: 'border-gray-600',    // Bordure secondaire
    accent: 'border-red-600',        // Bordure rouge
    light: 'border-gray-200',        // Bordure sur fond clair
    muted: 'border-gray-300',        // Bordure atténuée
  },
  
  // Boutons
  button: {
    primary: 'bg-red-600 hover:bg-red-700 text-white border border-red-600 hover:border-red-700',
    secondary: 'bg-gray-700 hover:bg-gray-600 text-white border border-gray-600',
    ghost: 'bg-transparent hover:bg-gray-800 text-white border border-gray-700',
    danger: 'bg-red-600 hover:bg-red-700 text-white',
    disabled: 'bg-gray-800 text-gray-500 cursor-not-allowed border border-gray-800',
  },
  
  // Cards/Surfaces
  card: {
    default: 'bg-gray-800 border border-gray-700',
    light: 'bg-white border border-gray-200',
    active: 'bg-red-600/20 border-red-600',
    winner: 'bg-red-900/30 border-red-700',
    finished: 'bg-gray-700/50 border-gray-600',
  },
  
  // États
  state: {
    success: 'bg-green-600',
    warning: 'bg-yellow-600', 
    error: 'bg-red-600',
    info: 'bg-blue-600',
    neutral: 'bg-gray-600',
  }
} as const;

// Utilitaires pour combiner les classes
export const cn = (...classes: (string | undefined | false)[]): string => {
  return classes.filter(Boolean).join(' ');
};

// THEME_CLASSES : Constantes pour composants réutilisables
export const THEME_CLASSES = {
  // Boutons avec toutes leurs variantes
  buttons: {
    primary: 'bg-red-600 hover:bg-red-700 text-white border border-red-600 px-4 py-2 rounded font-medium transition-colors',
    secondary: 'bg-gray-700 hover:bg-gray-600 text-white border border-gray-600 px-4 py-2 rounded font-medium transition-colors',
    ghost: 'bg-transparent hover:bg-gray-800 text-white border border-gray-700 px-4 py-2 rounded font-medium transition-colors',
    danger: 'bg-red-600 hover:bg-red-700 text-white border border-red-600 px-4 py-2 rounded font-medium transition-colors',
    success: 'bg-green-600 hover:bg-green-700 text-white border border-green-600 px-4 py-2 rounded font-medium transition-colors',
  },
  
  // États des joueurs et du jeu
  states: {
    active: 'bg-red-600/20 border-red-600 text-red-100',
    current: 'bg-red-50 border border-red-200 text-red-800',
    winner: 'bg-red-100 border border-red-300 text-red-900',
    finished: 'bg-gray-100 border border-gray-300 text-gray-800',
    inactive: 'bg-gray-700/50 border border-gray-600 text-gray-300',
  },
  
  // Indicateurs et notifications
  indicators: {
    info: 'bg-gray-100 border border-gray-300 text-gray-800 p-3 rounded-lg',
    success: 'bg-green-100 border border-green-300 text-green-800 p-3 rounded-lg',
    warning: 'bg-yellow-100 border border-yellow-300 text-yellow-800 p-3 rounded-lg',
    error: 'bg-red-100 border border-red-300 text-red-800 p-3 rounded-lg',
  },
  
  // Cartes spécialisées
  cards: {
    player: 'bg-gray-800 border border-gray-700 rounded-lg p-4',
    playerActive: 'bg-red-600/20 border-2 border-red-500 rounded-lg p-4',
    score: 'bg-gray-900 border border-gray-600 rounded-xl p-6',
    dartboard: 'bg-gradient-to-br from-gray-900 to-gray-800 border border-gray-700 rounded-2xl p-8',
  }
} as const;

// Classes communes réutilisables
export const commonStyles = {
  // Transitions
  transition: 'transition-all duration-200 ease-in-out',
  
  // Shadows
  shadow: {
    sm: 'shadow-sm',
    md: 'shadow-md',
    lg: 'shadow-lg',
    glow: 'shadow-lg shadow-red-500/25',
  },
  
  // Animations
  animate: {
    pulse: 'animate-pulse',
    bounce: 'animate-bounce',
    spin: 'animate-spin',
  },
  
  // Layout
  layout: {
    container: 'container mx-auto px-4',
    flexCenter: 'flex items-center justify-center',
    flexBetween: 'flex items-center justify-between',
    grid2: 'grid grid-cols-2 gap-4',
    grid3: 'grid grid-cols-3 gap-4',
  }
} as const;

// FONCTIONS UTILITAIRES DU SYSTÈME DE THÈME

/**
 * Génère les classes CSS pour les boutons selon le variant et l'état
 */
export function getButtonClasses(
  variant: 'primary' | 'secondary' | 'ghost' | 'danger' | 'success' = 'primary',
  disabled = false,
  size: 'sm' | 'md' | 'lg' = 'md'
): string {
  const baseClasses = 'inline-flex items-center justify-center font-medium transition-all duration-200 focus:outline-none focus:ring-2 focus:ring-red-500';
  
  // Tailles
  const sizeClasses = {
    sm: 'px-3 py-2 text-sm rounded',
    md: 'px-4 py-2 text-base rounded-lg',
    lg: 'px-6 py-3 text-lg rounded-xl',
  };
  
  if (disabled) {
    return cn(baseClasses, sizeClasses[size], 'bg-gray-300 text-gray-500 cursor-not-allowed border border-gray-300');
  }
  
  return cn(baseClasses, sizeClasses[size], THEME_CLASSES.buttons[variant]);
}

/**
 * Génère les classes CSS pour les états des joueurs/jeu
 */
export function getStateClasses(
  state: 'active' | 'current' | 'winner' | 'finished' | 'inactive',
  additionalClasses = ''
): string {
  return cn(THEME_CLASSES.states[state], additionalClasses);
}

/**
 * Génère les classes CSS pour les indicateurs et notifications
 */
export function getIndicatorClasses(
  type: 'info' | 'success' | 'warning' | 'error',
  additionalClasses = ''
): string {
  return cn(THEME_CLASSES.indicators[type], additionalClasses);
}

/**
 * Génère les classes CSS pour les cartes spécialisées
 */
export function getCardClasses(
  type: 'player' | 'playerActive' | 'score' | 'dartboard',
  additionalClasses = ''
): string {
  return cn(THEME_CLASSES.cards[type], additionalClasses);
}

/**
 * Compose les classes de thème avec validation de contraste
 */
export function composeTheme(config: {
  background?: keyof typeof theme.bg;
  text?: keyof typeof theme.text;
  border?: keyof typeof theme.border;
  additionalClasses?: string;
}): string {
  const classes = [
    config.background ? theme.bg[config.background] : '',
    config.text ? theme.text[config.text] : '',
    config.border ? theme.border[config.border] : '',
    config.additionalClasses || ''
  ].filter(Boolean);
  
  return cn(...classes);
}

/**
 * Utilitaire pour les états interactifs avec contraste approprié
 */
export function getInteractiveClasses(
  baseBackground: string,
  textColor: 'light' | 'dark' = 'light'
): string {
  const hoverClasses = textColor === 'light' 
    ? 'hover:bg-gray-800'  // Assombrit pour texte blanc
    : 'hover:bg-gray-100'; // Éclaircit pour texte noir
    
  return cn(baseBackground, hoverClasses, 'transition-colors duration-200');
}

/**
 * Génère les classes pour les éléments avec focus/accessibilité
 */
export function getAccessibleClasses(
  role: 'button' | 'link' | 'interactive' = 'interactive'
): string {
  const baseA11y = 'focus:outline-none focus:ring-2 focus:ring-red-500 focus:ring-offset-2 focus:ring-offset-gray-900';
  
  const roleClasses = {
    button: 'cursor-pointer',
    link: 'cursor-pointer underline-offset-4 hover:underline',
    interactive: 'cursor-pointer'
  };
  
  return cn(baseA11y, roleClasses[role]);
}