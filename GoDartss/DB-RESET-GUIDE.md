# 🗃️ Guide de Reset de Base de Données

Ce guide explique comment utiliser les scripts de reset de la base de données SQLite.

## 🚀 Scripts Disponibles

### 1. Reset Sécurisé (Recommandé)
```bash
pnpm db:reset --force
```
- ✅ **Supprime toutes les données** en respectant l'ordre des dépendances
- ✅ **Reset les compteurs** auto-increment 
- ✅ **Recrée des joueurs de test** avec statistiques basiques
- ⚠️ **Demande confirmation** sauf avec `--force`

### 2. Reset Rapide (Dangereux)
```bash
pnpm db:quick-reset
```
- 🚨 **Vide TOUT immédiatement** sans confirmation
- ⚡ **Ultra rapide** mais sans données de test
- ⚠️ **À utiliser avec précaution !**

### 3. Peuplement avec Données Réalistes
```bash
pnpm db:seed
```
- 🌱 **Ajoute des joueurs** avec statistiques variées
- 📊 **Crée des parties d'exemple** (terminées + en cours)
- 🎯 **Parfait pour tester** les fonctionnalités

### 4. Reset + Peuplement (Combo)
```bash
pnpm db:fresh
```
- 🔄 **Reset rapide** + **peuplement automatique**
- 🎬 **Base propre et prête** pour les tests
- 📈 **Données réalistes** incluses

## 📋 Ordre de Suppression des Tables

Les scripts respectent cet ordre pour éviter les erreurs de foreign key :

1. `PlayerBadge` (références Player)
2. `Throw` (références Turn)  
3. `Turn` (références GameParticipant)
4. `GameParticipant` (références Game + Player)
5. `PlayerStats` (références Player)
6. `Game` (références Player pour winner)
7. `Player` (table principale)

## 🎯 Données de Test Créées

### Reset Standard (`pnpm db:reset --force`)
- **4 joueurs basiques** : Alice, Bob, Charlie, Diana
- **Statistiques vides** (0 parties, 0 victoires)
- **PlayerStats initialisés** à zéro

### Peuplement Réaliste (`pnpm db:seed`)
- **ProDart** : Expert (68.5 avg, 25 parties, 72% win rate)
- **MidPlayer** : Intermédiaire (45.2 avg, 15 parties, 47% win rate)  
- **Newbie** : Débutant (28.7 avg, 8 parties, 25% win rate)
- **RegularJoe** : Régulier (38.4 avg, 20 parties, 45% win rate)
- **Test1 & Test2** : Joueurs vides pour vos tests

### Parties d'Exemple
- **1 partie terminée** : ProDart vs MidPlayer (ProDart gagne)
- **1 partie en cours** : Test1 vs Test2 (pour tester la reprise)

## 🛡️ Sécurité

- **Confirmation obligatoire** sauf avec `--force`
- **Backup automatique** non inclus (SQLite simple)
- **Environnement de dev** uniquement recommandé

## 💡 Cas d'Usage Typiques

```bash
# Développement quotidien - Reset rapide
pnpm db:fresh

# Tests avec données réalistes
pnpm db:reset --force
pnpm db:seed

# Urgence - Vider immédiatement  
pnpm db:quick-reset

# Démo - Base propre avec exemples
pnpm db:fresh
```

## 🔧 Dépannage

Si vous avez des erreurs :

1. **Vérifiez Prisma** : `pnpm db:generate`
2. **Reset en force** : `pnpm db:quick-reset`
3. **Migrations** : `pnpm db:migrate dev`

---

💡 **Tip** : Utilisez `pnpm db:fresh` avant chaque session de développement pour une base propre !