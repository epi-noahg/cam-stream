/**
 * Composant pour afficher les notifications de badges gagnés
 */

'use client';

import { useEffect, useState } from 'react';
import { BadgeDefinition } from '@/lib/badgeSystem';
import { useTheme } from '@/hooks/useTheme';

interface BadgeNotificationProps {
  badges: BadgeDefinition[];
  onClose: () => void;
}

export function BadgeNotification({ badges, onClose }: BadgeNotificationProps) {
  const [visible, setVisible] = useState(false);
  const theme = useTheme();

  useEffect(() => {
    if (badges.length > 0) {
      setVisible(true);
      const timer = setTimeout(() => {
        setVisible(false);
        setTimeout(onClose, 300); // Délai pour l'animation
      }, 4000);

      return () => clearTimeout(timer);
    }
  }, [badges, onClose]);

  if (badges.length === 0) return null;

  const getRarityColor = (rarity: BadgeDefinition['rarity']) => {
    switch (rarity) {
      case 'legendary': return 'border-yellow-400 bg-yellow-100 text-yellow-800';
      case 'epic': return 'border-purple-400 bg-purple-100 text-purple-800';
      case 'rare': return 'border-blue-400 bg-blue-100 text-blue-800';
      case 'common': return 'border-green-400 bg-green-100 text-green-800';
      default: return 'border-gray-400 bg-gray-100 text-gray-800';
    }
  };

  return (
    <div className={`fixed top-4 right-4 z-50 transition-all duration-300 ${
      visible ? 'opacity-100 translate-y-0' : 'opacity-0 -translate-y-4'
    }`}>
      <div className="space-y-2">
        {badges.map((badge, index) => (
          <div
            key={`${badge.type}-${index}`}
            className={`p-4 rounded-lg border-2 shadow-lg max-w-sm ${getRarityColor(badge.rarity)}`}
          >
            <div className="flex items-center gap-3">
              <div className="text-3xl">{badge.icon}</div>
              <div className="flex-1">
                <h3 className="font-bold text-sm">Badge débloqué !</h3>
                <p className="font-medium">{badge.name}</p>
                <p className="text-xs opacity-75 mt-1">{badge.description}</p>
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}