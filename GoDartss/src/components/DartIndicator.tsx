"use client";

import { cn, theme } from "@/lib/theme";

interface DartIndicatorProps {
  /** Index de la fléchette actuelle (0-2) */
  currentDart: number;
  /** Nombre total de fléchettes par tour */
  totalDarts?: number;
  /** Taille des indicateurs */
  size?: 'sm' | 'md' | 'lg';
  /** Style de l'indicateur */
  variant?: 'dots' | 'bars' | 'darts';
  /** Classes CSS additionnelles */
  className?: string;
}

/**
 * Composant d'indication visuelle des fléchettes
 * Montre clairement combien de fléchettes ont été utilisées et restantes
 */
export default function DartIndicator({ 
  currentDart, 
  totalDarts = 3, 
  size = 'md',
  variant = 'dots',
  className 
}: DartIndicatorProps) {
  
  // Configuration des tailles
  const sizeConfig = {
    sm: { dot: 'w-2 h-2', bar: 'w-6 h-1', dart: 'w-3 h-3', gap: 'gap-1', text: 'text-xs' },
    md: { dot: 'w-4 h-4', bar: 'w-8 h-2', dart: 'w-4 h-4', gap: 'gap-2', text: 'text-sm' },
    lg: { dot: 'w-6 h-6', bar: 'w-12 h-3', dart: 'w-6 h-6', gap: 'gap-3', text: 'text-base' }
  };
  
  const config = sizeConfig[size];
  
  // Génération des indicateurs
  const indicators = Array.from({ length: totalDarts }, (_, index) => ({
    index,
    used: index < currentDart,
    current: index === currentDart,
    remaining: index > currentDart
  }));

  const renderDots = () => (
    <div className={cn("flex items-center", config.gap, className)}>
      {indicators.map((indicator) => (
        <div
          key={indicator.index}
          className={cn(
            config.dot,
            "rounded-full transition-all duration-300",
            indicator.used && "bg-red-600 shadow-lg shadow-red-600/50",
            indicator.current && "bg-red-500 animate-pulse ring-2 ring-red-400 ring-opacity-75",
            indicator.remaining && cn(theme.bg.mutedDark, "border-2", theme.border.secondary)
          )}
        />
      ))}
      <span className={cn(config.text, theme.text.secondary, "ml-2")}>
        ({currentDart + 1}/{totalDarts})
      </span>
    </div>
  );

  const renderBars = () => (
    <div className={cn("flex items-center", config.gap, className)}>
      {indicators.map((indicator) => (
        <div
          key={indicator.index}
          className={cn(
            config.bar,
            "rounded-full transition-all duration-300",
            indicator.used && "bg-red-600",
            indicator.current && "bg-red-500 animate-pulse",
            indicator.remaining && theme.bg.mutedDark
          )}
        />
      ))}
    </div>
  );

  const renderDarts = () => (
    <div className={cn("flex items-center", config.gap, className)}>
      {indicators.map((indicator) => (
        <div
          key={indicator.index}
          className={cn(
            config.dart,
            "relative transition-all duration-300"
          )}
        >
          {/* Dart SVG Icon */}
          <svg
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            className={cn(
              "w-full h-full",
              indicator.used && "text-red-600",
              indicator.current && "text-red-500 animate-pulse",
              indicator.remaining && theme.text.muted
            )}
          >
            <path d="M12 2L14 8L12 14L10 8L12 2Z" />
            <path d="M12 14L12 22" />
          </svg>
          
          {/* Glow effect pour fléchette actuelle */}
          {indicator.current && (
            <div className="absolute inset-0 bg-red-500/20 rounded-full blur-sm animate-pulse" />
          )}
        </div>
      ))}
    </div>
  );

  // Rendu selon le variant
  switch (variant) {
    case 'bars':
      return renderBars();
    case 'darts':
      return renderDarts();
    case 'dots':
    default:
      return renderDots();
  }
}

/**
 * Composant compact d'indicateur pour header
 */
export function CompactDartIndicator({ currentDart, totalDarts = 3 }: { currentDart: number; totalDarts?: number }) {
  return (
    <div className="inline-flex items-center gap-1">
      {Array.from({ length: totalDarts }, (_, index) => (
        <div
          key={index}
          className={cn(
            "w-2 h-2 rounded-full transition-colors",
            index < currentDart ? "bg-red-600" : 
            index === currentDart ? "bg-red-500 animate-pulse" : 
            theme.bg.mutedDark
          )}
        />
      ))}
    </div>
  );
}

/**
 * Composant d'indicateur avec animation pour celebrations
 */
export function AnimatedDartIndicator({ currentDart, totalDarts = 3, celebrating = false }: { 
  currentDart: number; 
  totalDarts?: number; 
  celebrating?: boolean; 
}) {
  return (
    <div className={cn(
      "flex items-center gap-2",
      celebrating && "animate-bounce"
    )}>
      {Array.from({ length: totalDarts }, (_, index) => (
        <div
          key={index}
          className={cn(
            "w-4 h-4 rounded-full transition-all duration-500",
            index < currentDart && !celebrating && "bg-red-600 shadow-lg shadow-red-600/50",
            index < currentDart && celebrating && "bg-green-500 shadow-lg shadow-green-500/50 animate-pulse",
            index === currentDart && !celebrating && "bg-red-500 animate-pulse ring-2 ring-red-400",
            index === currentDart && celebrating && "bg-green-400 animate-ping",
            index > currentDart && theme.bg.mutedDark
          )}
        />
      ))}
    </div>
  );
}