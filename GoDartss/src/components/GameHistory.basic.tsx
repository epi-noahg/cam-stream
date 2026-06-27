/**
 * Composant simplifié pour afficher l'historique des parties
 */

'use client';

import { useTheme } from '@/hooks/useTheme';

interface GameHistoryProps {
  playerId?: number;
  limit?: number;
}

export function GameHistory({ limit = 10 }: GameHistoryProps) {
  const theme = useTheme();

  // Version simplifiée - données mockées
  const gameHistory: never[] = [];
  const isLoading = false;
  const error = null;

  if (isLoading) {
    return (
      <div className={theme.common.card}>
        <div className="animate-pulse">
          <div className="h-4 bg-gray-300 rounded mb-2"></div>
          <div className="h-4 bg-gray-300 rounded mb-2"></div>
          <div className="h-4 bg-gray-300 rounded"></div>
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div className={theme.common.card}>
        <p className="text-red-600">Erreur lors du chargement de l&apos;historique</p>
      </div>
    );
  }

  if (!gameHistory || gameHistory.length === 0) {
    return (
      <div className={theme.common.card}>
        <p className="text-gray-500">Aucune partie dans l&apos;historique</p>
        <p className="text-sm text-gray-400 mt-2">
          Les données d&apos;historique seront disponibles une fois le backend tRPC corrigé.
        </p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      <div className="flex justify-between items-center">
        <h3 className="text-lg font-semibold text-white">Historique des parties</h3>
      </div>

      <div className="space-y-3">
        {/* Les parties apparaîtront ici une fois le backend connecté */}
      </div>
    </div>
  );
}