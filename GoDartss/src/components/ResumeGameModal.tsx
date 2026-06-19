/**
 * Modal pour reprendre une partie interrompue
 */

'use client';

import { trpc } from '@/lib/trpc';
import { useTheme } from '@/hooks/useTheme';

interface ResumeGameModalProps {
  isOpen: boolean;
  onClose: () => void;
  onGameSelected: (gameId: number, gameState: Record<string, unknown>) => void;
  playerId?: number;
}

export function ResumeGameModal({ isOpen, onClose, onGameSelected, playerId }: ResumeGameModalProps) {
  const theme = useTheme();

  // Charger les parties en cours (non terminées)
  const { data: games, isLoading } = trpc.game.getAll.useQuery(
    { 
      limit: 10, 
      offset: 0,
      playerId 
    },
    { enabled: isOpen } // Ne charger que quand le modal est ouvert
  );

  if (!isOpen) return null;

  // Filtrer pour ne garder que les parties en cours
  const activeGames = games?.filter((game: any) => 
    game.status === 'IN_PROGRESS' || game.status === 'WAITING'
  ) || [];
  return (
    <div className="fixed inset-0 bg-black/70 backdrop-blur-sm flex items-center justify-center z-50 p-4">
      <div className="bg-gray-900 border border-gray-700 rounded-2xl shadow-2xl max-w-lg w-full max-h-[80vh] overflow-hidden">
        {/* Header */}
        <div className="flex items-center justify-between p-6 border-b border-gray-700">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-full bg-red-600/20 flex items-center justify-center">
              <svg className="w-5 h-5 text-red-500" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M14.828 14.828a4 4 0 01-5.656 0M9 10h1m4 0h1m-6 4h6m2 5H7a2 2 0 01-2-2V9a2 2 0 012-2h10a2 2 0 012 2v8a2 2 0 01-2 2z" />
              </svg>
            </div>
            <div>
              <h2 className="text-xl font-bold text-white">Reprendre une partie</h2>
              <p className="text-sm text-gray-400">Continuez là où vous vous êtes arrêté</p>
            </div>
          </div>
          <button
            onClick={onClose}
            className="w-8 h-8 rounded-full hover:bg-gray-700 flex items-center justify-center transition-colors text-gray-400 hover:text-white"
          >
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>

        {/* Content */}
        <div className="overflow-y-auto max-h-[60vh]">

          {isLoading ? (
            <div className="text-center py-12 px-6">
              <div className="relative mx-auto mb-6">
                <div className="w-12 h-12 border-4 border-gray-600 rounded-full animate-spin"></div>
                <div className="w-12 h-12 border-4 border-red-600 rounded-full animate-spin absolute top-0 left-0" style={{borderTopColor: 'transparent', borderRightColor: 'transparent'}}></div>
              </div>
              <h3 className="text-lg font-semibold text-white mb-2">Recherche en cours...</h3>
              <p className="text-gray-400">Chargement de vos parties en cours</p>
            </div>
          ) : activeGames.length > 0 ? (
            <div className="p-6">
              <div className="mb-4">
                <p className="text-sm text-gray-400">
                  {activeGames.length} partie{activeGames.length > 1 ? 's' : ''} en cours trouvée{activeGames.length > 1 ? 's' : ''}
                </p>
              </div>
              <div className="space-y-3">
                {activeGames.map((game: any, index: number) => (
                  <div 
                    key={game.id}
                    onClick={() => {
                      let parsedState = {};
                      if (game.currentState) {
                        try {
                          parsedState = JSON.parse(game.currentState);
                        } catch (error) {
                          console.error('Error parsing saved state:', error);
                        }
                      }
                      onGameSelected(game.id, parsedState);
                    }}
                    className="group relative p-4 rounded-xl bg-gray-800/50 hover:bg-gray-800 cursor-pointer transition-all duration-200 border border-gray-700 hover:border-red-500/50 hover:shadow-lg hover:shadow-red-500/10"
                  >
                    <div className="flex items-center justify-between">
                      <div className="flex-1">
                        <div className="flex items-center gap-3 mb-2">
                          <div className="flex items-center gap-2">
                            <div className="w-8 h-8 rounded-full bg-red-600/20 flex items-center justify-center text-red-500 font-bold text-sm">
                              {index + 1}
                            </div>
                            <div>
                              <h3 className="text-white font-semibold">
                                Partie #{game.id}
                              </h3>
                              <div className="flex items-center gap-2">
                                <span className={`px-2 py-1 rounded-full text-xs font-medium ${
                                  game.status === 'IN_PROGRESS' 
                                    ? 'bg-green-600/20 text-green-400 border border-green-600/20' 
                                    : 'bg-yellow-600/20 text-yellow-400 border border-yellow-600/20'
                                }`}>
                                  {game.status === 'IN_PROGRESS' ? 'En cours' : 'En attente'}
                                </span>
                                <span className="text-xs text-gray-500">
                                  {game.mode}
                                </span>
                              </div>
                            </div>
                          </div>
                        </div>
                        
                        <div className="flex flex-wrap items-center gap-2 mb-2">
                          {game.participants.slice(0, 3).map((p: any, i: number) => (
                            <div key={p.player.id} className="flex items-center">
                              <span className="text-sm text-gray-300 font-medium">
                                {p.player.nickname}
                              </span>
                              {i < game.participants.length - 1 && i < 2 && (
                                <span className="text-gray-500 mx-1">•</span>
                              )}
                            </div>
                          ))}
                          {game.participants.length > 3 && (
                            <span className="text-sm text-gray-500">
                              +{game.participants.length - 3} autres
                            </span>
                          )}
                        </div>
                        
                        <div className="text-xs text-gray-500">
                          Créée le {new Date(game.createdAt).toLocaleDateString('fr-FR', {
                            day: 'numeric',
                            month: 'short',
                            hour: '2-digit',
                            minute: '2-digit'
                          })}
                        </div>
                      </div>
                      
                      <div className="flex items-center gap-2">
                        <div className="w-10 h-10 rounded-full bg-red-600/20 group-hover:bg-red-600/30 flex items-center justify-center transition-colors">
                          <svg className="w-5 h-5 text-red-500" fill="currentColor" viewBox="0 0 24 24">
                            <path d="M8 5v14l11-7z"/>
                          </svg>
                        </div>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          ) : (
            <div className="text-center py-12 px-6">
              <div className="w-16 h-16 rounded-full bg-gray-700/50 flex items-center justify-center mx-auto mb-4">
                <svg className="w-8 h-8 text-gray-500" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9.172 16.172a4 4 0 015.656 0M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V9a2 2 0 012-2h10a2 2 0 012 2v8a2 2 0 01-2 2z" />
                </svg>
              </div>
              <h3 className="text-lg font-semibold text-white mb-2">Aucune partie en cours</h3>
              <p className="text-gray-400 mb-4">
                Toutes vos parties sont terminées ou vous n'avez pas encore commencé.
              </p>
              <p className="text-sm text-gray-500">
                Créez une nouvelle partie pour commencer à jouer !
              </p>
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="border-t border-gray-700 px-6 py-4 bg-gray-900/50">
          <div className="flex justify-end gap-3">
            <button
              onClick={onClose}
              className="px-4 py-2 rounded-lg bg-gray-700 hover:bg-gray-600 text-gray-300 hover:text-white transition-all duration-200 font-medium"
            >
              Fermer
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}