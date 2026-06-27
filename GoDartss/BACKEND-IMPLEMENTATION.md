# Backend Complet GoDarts - Guide d'Implémentation

## 🎯 Vue d'ensemble

Le backend de GoDarts a été entièrement implémenté avec les fonctionnalités suivantes :

- **Persistance des joueurs** avec statistiques complètes
- **Stockage des parties** avec sauvegarde automatique de l'état
- **Système de badges** avec détection automatique des achievements
- **Historique des parties** avec classements et statistiques
- **Reprise de partie** après refresh ou fermeture
- **APIs type-safe** avec tRPC

## 🗄️ Structure de la Base de Données

### Modèles Principaux

#### Player
```prisma
model Player {
  id           Int       @id @default(autoincrement())
  nickname     String    @unique
  email        String?   @unique
  avatar       String?
  totalGames   Int       @default(0)
  totalWins    Int       @default(0)
  totalLosses  Int       @default(0)
  bestFinish   Int?      // Meilleur checkout
  createdAt    DateTime  @default(now())
  lastActiveAt DateTime  @default(now())
  
  // Relations
  participants GameParticipant[]
  badges       PlayerBadge[]
  stats        PlayerStats?
  wonGames     Game[]    @relation("GameWinner")
}
```

#### Game
```prisma
model Game {
  id            Int        @id @default(autoincrement())
  mode          GameMode
  status        GameStatus @default(WAITING)
  startingScore Int?
  doubleOut     Boolean    @default(false)
  doubleIn      Boolean    @default(false)
  currentState  String?    // JSON sérialisé de l'état du jeu
  winnerId      Int?
  winnerRank    Json?      // Classement final des joueurs
  pausedAt      DateTime?
  createdAt     DateTime   @default(now())
  updatedAt     DateTime   @updatedAt
  finishedAt    DateTime?
  
  // Relations
  participants  GameParticipant[]
  turns         Turn[]
  winner        Player?    @relation("GameWinner", fields: [winnerId], references: [id])
}
```

#### PlayerBadge
```prisma
model PlayerBadge {
  id        Int       @id @default(autoincrement())
  player    Player    @relation(fields: [playerId], references: [id])
  playerId  Int
  type      BadgeType
  earnedAt  DateTime  @default(now())
  gameId    Int?
  metadata  Json?
  
  @@unique([playerId, type])
}
```

#### PlayerStats
```prisma
model PlayerStats {
  id                    Int      @id @default(autoincrement())
  player                Player   @relation(fields: [playerId], references: [id])
  playerId              Int      @unique
  
  // Statistiques générales
  averageScore          Float    @default(0)
  totalThrows           Int      @default(0)
  totalScore            Int      @default(0)
  
  // Statistiques de précision
  bull50Count           Int      @default(0)
  bull25Count           Int      @default(0)
  tripleCount           Int      @default(0)
  doubleCount           Int      @default(0)
  
  // Checkout statistics
  checkoutAttempts      Int      @default(0)
  checkoutSuccess       Int      @default(0)
  averageCheckoutScore  Float    @default(0)
  
  // Séries
  bestThreeDartAverage  Float    @default(0)
  longest180Series      Int      @default(0)
  total180s             Int      @default(0)
  
  updatedAt            DateTime  @updatedAt
}
```

### Énumérations

```prisma
enum GameStatus {
  WAITING      // En attente de joueurs
  IN_PROGRESS  // Partie en cours
  PAUSED       // Partie mise en pause
  FINISHED     // Partie terminée
  ABANDONED    // Partie abandonnée
}

enum BadgeType {
  TRIPLE_TWENTY_HAT_TRICK  // 3 triple 20 dans le même tour
  DOUBLE_OUT_FINISH        // Finir avec un double
  HIGH_FINISH             // Finir avec plus de 100 points
  PERFECT_LEG             // Leg en 9 fléchettes
  CONSISTENT_SCORER       // 5 tours consécutifs > 40 points
  BULL_MASTER            // 3 bulls dans le même tour
  COMEBACK_KING          // Gagner après avoir été dernier
}
```

## 🚀 APIs tRPC

### Structure des Routeurs

Le backend expose 5 routeurs principaux :

#### 1. Players Router (`/api/trpc/players`)
- `create` - Créer un nouveau joueur
- `getAll` - Récupérer tous les joueurs
- `getById` - Récupérer un joueur par ID
- `update` - Mettre à jour un joueur
- `getLeaderboard` - Classement des joueurs
- `delete` - Supprimer un joueur

#### 2. Games Router (`/api/trpc/games`)
- `create` - Créer une nouvelle partie
- `getAll` - Récupérer toutes les parties
- `getById` - Récupérer une partie par ID
- `getCurrentGame` - Partie en cours d'un joueur
- `start` - Démarrer une partie
- `updateState` - Mettre à jour l'état d'une partie
- `finish` - Terminer une partie
- `pause` - Mettre en pause une partie
- `resume` - Reprendre une partie
- `abandon` - Abandonner une partie
- `getHistory` - Historique des parties
- `delete` - Supprimer une partie

#### 3. Stats Router (`/api/trpc/stats`)
- `updatePlayerStats` - Mettre à jour les statistiques d'un joueur
- `getPlayerStats` - Récupérer les statistiques d'un joueur
- `getGlobalStats` - Statistiques globales de tous les joueurs
- `comparePlayerStats` - Comparer les statistiques entre joueurs
- `getRecords` - Records globaux
- `resetPlayerStats` - Réinitialiser les statistiques d'un joueur

#### 4. Badges Router (`/api/trpc/badges`)
- `award` - Attribuer un badge à un joueur
- `getByPlayerId` - Récupérer tous les badges d'un joueur
- `getAll` - Récupérer tous les badges
- `checkAndAwardForTurn` - Vérifier et attribuer automatiquement les badges pour un tour
- `checkAndAwardForGameEnd` - Vérifier et attribuer les badges en fin de partie
- `revoke` - Révoquer un badge
- `getStatistics` - Statistiques des badges

#### 5. Turns Router (`/api/trpc/turns`)
- `create` - Créer un nouveau tour
- `getByGameId` - Récupérer tous les tours d'une partie
- `getByParticipantId` - Récupérer tous les tours d'un participant
- `update` - Mettre à jour un tour
- `delete` - Supprimer un tour
- `addThrow` - Ajouter un lancer à un tour
- `deleteThrow` - Supprimer un lancer spécifique
- `getStatistics` - Statistiques d'un tour

## 🏆 Système de Badges

### Badges Disponibles

1. **Triple 20 Hat Trick** 🎯 (Rare)
   - 3 triple 20 dans le même tour

2. **Bull Master** 🎯 (Epic)
   - 3 bulls dans le même tour

3. **Double Out Master** 🏆 (Common)
   - Terminer une partie avec un double

4. **High Finisher** 💯 (Rare)
   - Terminer une partie avec plus de 100 points

5. **Perfect Leg** ✨ (Legendary)
   - Terminer une partie en exactement 9 fléchettes

6. **Consistent Scorer** 📈 (Common)
   - 5 tours consécutifs de plus de 40 points

7. **Comeback King** 👑 (Epic)
   - Gagner après avoir été en dernière position

### Détection Automatique

Le système de badges utilise une architecture modulaire :

```typescript
export interface BadgeDefinition {
  type: BadgeType;
  name: string;
  description: string;
  icon: string;
  rarity: 'common' | 'rare' | 'epic' | 'legendary';
  checkCondition: (context: BadgeCheckContext) => boolean;
}
```

Les badges sont vérifiés automatiquement :
- **Après chaque tour** pour les badges de performance
- **En fin de partie** pour les badges de victoire
- **Stockage des métadonnées** avec le contexte du badge

## 🔄 Persistance de l'État

### Sauvegarde Automatique

L'état du jeu est sauvegardé automatiquement :
- **Toutes les 30 secondes** pendant le jeu
- **Après chaque action importante** (lancer, tour suivant)
- **Avant fermeture** de la page (avec `beforeunload`)

### Format de Stockage

```typescript
interface GameState {
  options: OptionsX01;
  players: PlayerState[];
  currentIndex: number;
  dartIndex: number;
  turns: Throw[][];
  winner: number | null;
  finishedPlayers: number[];
  gameOver: boolean;
}
```

L'état est sérialisé en JSON dans le champ `currentState` de la table `Game`.

## 📊 Statistiques et Historique

### Statistiques Calculées

Pour chaque joueur :
- **Moyenne par fléchette**
- **Pourcentage de checkout**
- **Précision** (triples, doubles, bulls)
- **Records personnels**
- **Séries** (180s consécutifs)

### Historique des Parties

L'historique inclut :
- **Parties terminées** avec résultats détaillés
- **Durée des parties**
- **Classements finaux**
- **Statistiques de performance**

## 🔌 Intégration Frontend

### Hooks Personnalisés

#### `useGamePersistence`
```typescript
const {
  saveGameState,
  pauseCurrentGame,
  finishCurrentGame,
  isSaving
} = useGamePersistence({
  gameId,
  gameState,
  isGameActive
});
```

#### `useBadges`
```typescript
const {
  recentlyEarnedBadges,
  checkTurnBadges,
  checkGameEndBadges
} = useBadges();
```

#### `useGameResume`
```typescript
const {
  resumableGames,
  restoreGameState,
  resumeGameById
} = useGameResume();
```

### Composants UI

#### `<GameHistory>`
Affiche l'historique des parties avec filtres et pagination.

#### `<PlayerStats>`
Statistiques détaillées d'un joueur avec badges et records.

#### `<ResumeGameModal>`
Modal pour reprendre une partie interrompue.

#### `<BadgeNotification>`
Notifications animées pour les nouveaux badges.

## 🛠️ Configuration et Déploiement

### Prérequis

1. **Générer le client Prisma** :
```bash
pnpm db:generate
```

2. **Appliquer les migrations** :
```bash
pnpm db:migrate init_backend
```

3. **Démarrer le serveur** :
```bash
pnpm dev
```

### Variables d'Environnement

```env
DATABASE_URL="file:./dev.db"
```

### Structure des Fichiers

```
src/
├── app/
│   ├── api/trpc/[trpc]/route.ts  # API tRPC
│   └── providers.tsx             # Providers tRPC
├── server/
│   ├── routers/                  # Routeurs tRPC
│   ├── index.ts                  # Routeur principal
│   └── trpc.ts                   # Configuration tRPC
├── hooks/
│   ├── useGamePersistence.ts     # Persistance automatique
│   ├── useBadges.ts             # Gestion des badges
│   └── useGameResume.ts         # Reprise de partie
├── components/
│   ├── GameHistory.tsx          # Historique des parties
│   ├── PlayerStats.tsx          # Statistiques des joueurs
│   ├── ResumeGameModal.tsx      # Modal de reprise
│   └── BadgeNotification.tsx    # Notifications de badges
└── lib/
    ├── badgeSystem.ts           # Système de badges
    └── trpc.ts                  # Client tRPC
```

## 🚀 Prochaines Étapes

1. **Intégrer avec les pages existantes** :
   - Connecter `GameSetup.tsx` avec la création de partie
   - Modifier `GameContext.tsx` pour utiliser la persistance
   - Ajouter les composants de statistiques

2. **Tests et Validation** :
   - Tester la sauvegarde automatique
   - Valider la détection des badges
   - Vérifier la reprise de partie

3. **Optimisations** :
   - Cache des requêtes avec React Query
   - Pagination pour l'historique
   - Compression de l'état du jeu

Le backend est maintenant prêt à être utilisé ! Toutes les APIs sont type-safe et l'intégration avec le frontend existant peut commencer.