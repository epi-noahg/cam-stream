/**
 * Composant pour afficher l'historique des parties
 */

'use client';

import { trpc } from '@/lib/trpc';
import { useTheme } from '@/hooks/useTheme';
import Link from 'next/link';
import { Eye, Play } from 'lucide-react';

interface GameHistoryProps {
  playerId?: number;
  limit?: number;
}

export function GameHistory({ playerId, limit = 10 }: GameHistoryProps) {
  const theme = useTheme();

  // Charger les parties depuis le backend
  const { data: games, isLoading, error } = trpc.game.getAll.useQuery({
    playerId,
    limit,
    offset: 0
  });

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

  // Afficher les parties
  return (
    <div className={theme.common.card}>
      {games && games.length > 0 ? (
        <div className="space-y-4 bg-gray-900 p-4 rounded-lg">
          {games.map((game: any) => (
            <div 
              key={game.id}
              className="flex items-center justify-between p-4 rounded-lg bg-gray-800 hover:bg-gray-750 transition-colors"
            >
              <div className="flex-1">
                <div className="flex items-center gap-3 mb-2">
                  <span className="text-white font-semibold">
                    Partie #{game.id}
                  </span>
                  <span className={`px-2 py-1 rounded text-xs font-medium ${
                    game.status === 'FINISHED' ? 'bg-green-600 text-white' :
                    game.status === 'IN_PROGRESS' ? 'bg-blue-600 text-white' :
                    'bg-gray-600 text-white'
                  }`}>
                    {game.status === 'FINISHED' ? 'Terminée' :
                     game.status === 'IN_PROGRESS' ? 'En cours' : 'En attente'}
                  </span>
                </div>
                <div className="text-sm text-gray-400">
                  {game.participants.map((p: any) => p.player.nickname).join(', ')}
                </div>
                <div className="text-xs text-gray-500 mt-1">
                  {new Date(game.createdAt).toLocaleDateString('fr-FR')} • {game.mode}
                </div>
              </div>
              <div className="flex items-center gap-3">
                <div className="text-right">
                  {game.winner && (
                    <div className="text-sm font-semibold text-red-400">
                      🏆 {game.winner.nickname}
                    </div>
                  )}
                  <div className="text-xs text-gray-500">
                    {game._count?.turns || 0} tours
                  </div>
                </div>
                
                {/* Boutons d'action */}
                <div className="flex gap-2">
                  {game.status === 'FINISHED' ? (
                    <Link
                      href={`/x01/play/${game.id}?view=history`}
                      className="flex items-center gap-2 px-4 py-2 bg-gradient-to-r from-red-600/90 to-red-700/90 hover:from-red-500 hover:to-red-600 text-white text-sm font-medium rounded-lg transition-all duration-200 transform hover:scale-105 shadow-lg border border-red-500/30"
                    >
                      <Eye className="w-4 h-4" />
                      Historique
                    </Link>
                  ) : game.status === 'IN_PROGRESS' ? (
                    <Link
                      href={`/x01/play/${game.id}`}
                      className="flex items-center gap-2 px-4 py-2 bg-gradient-to-r from-green-600/90 to-green-700/90 hover:from-green-500 hover:to-green-600 text-white text-sm font-medium rounded-lg transition-all duration-200 transform hover:scale-105 shadow-lg border border-green-500/30"
                    >
                      <Play className="w-4 h-4" />
                      Continuer
                    </Link>
                  ) : null}
                </div>
              </div>
            </div>
          ))}
        </div>
      ) : (
        <div className="text-center py-8">
          <p className="text-gray-400 mb-2">
            Aucune partie trouvée
          </p>
          <p className="text-sm text-gray-500">
            Commencez votre première partie !
          </p>
        </div>
      )}
    </div>
  );
}