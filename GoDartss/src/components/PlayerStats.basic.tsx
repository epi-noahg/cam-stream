/**
 * Composant simplifié pour afficher les statistiques des joueurs
 */

'use client';

import { useTheme } from '@/hooks/useTheme';

interface PlayerStatsProps {
  playerId: number;
}

export function PlayerStats({ playerId }: PlayerStatsProps) {
  const theme = useTheme();
  
  // Version simplifiée - données mockées
  const isLoading = false;

  if (isLoading) {
    return (
      <div className={theme.common.card}>
        <div className="animate-pulse space-y-4">
          <div className="h-6 bg-gray-300 rounded"></div>
          <div className="grid grid-cols-2 gap-4">
            <div className="h-16 bg-gray-300 rounded"></div>
            <div className="h-16 bg-gray-300 rounded"></div>
          </div>
        </div>
      </div>
    );
  }

  const StatCard = ({ title, value, subtitle }: { title: string; value: string; subtitle?: string }) => (
    <div className="bg-gray-50 rounded-lg p-4 text-center">
      <h4 className="text-sm font-medium text-gray-600 mb-1">{title}</h4>
      <p className="text-2xl font-bold text-black">{value}</p>
      {subtitle && <p className="text-xs text-gray-500 mt-1">{subtitle}</p>}
    </div>
  );

  return (
    <div className="space-y-6">
      {/* Header avec infos joueur */}
      <div className={theme.common.card}>
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-xl font-bold text-white">
            Statistiques du joueur {playerId}
          </h2>
          <div className="text-sm text-gray-400">
            En attente de données
          </div>
        </div>

        {/* Message d'attente */}
        <div className="text-center py-8">
          <p className="text-gray-400 mb-2">
            Les statistiques détaillées seront disponibles une fois le backend tRPC corrigé.
          </p>
          <p className="text-sm text-gray-500">
            Le système de badges et l&apos;historique des parties sont en cours d&apos;intégration.
          </p>
        </div>

        {/* Statistiques de base mockées */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
          <StatCard
            title="Moyenne"
            value="0.0"
            subtitle="par fléchette"
          />
          <StatCard
            title="Meilleur 3 darts"
            value="0.0"
          />
          <StatCard
            title="Total 180"
            value="0"
          />
          <StatCard
            title="Checkout %"
            value="0.0"
            subtitle="0/0"
          />
        </div>
      </div>
    </div>
  );
}