# Guide d'Application du Système de Thème - GoDarts

## Vue d'ensemble

Ce guide détaille l'application du système de thème unifié pour GoDarts, basé sur une palette de **Noir, Blanc, Rouge et Gris**.

## Palette de couleurs

### Couleurs principales
- **Noir** (`#000000`) : Arrière-plans principaux, textes sur fond clair
- **Blanc** (`#ffffff`) : Textes sur fond sombre, surfaces claires
- **Rouge** (`#dc2626`) : Accents, actions importantes, gagnants, éléments actifs
- **Gris** (échelle 50-900) : Textes secondaires, bordures, états désactivés

### Couleurs spécialisées
- **Dartboard** : Conservation des couleurs traditionnelles (rouge `#ff0037`, vert `#009245`)

## Application par type de composant

### 1. Layout et Navigation
```typescript
// Arrière-plan principal
className="bg-black text-white"

// Navigation secondaire
className="bg-gray-900 text-gray-300"
```

### 2. Boutons

#### Bouton principal (CTA)
```typescript
import { getButtonClasses } from '@/lib/theme';
className={getButtonClasses('primary')}
// Résultat : bg-red-600 hover:bg-red-700 text-white border border-red-600 px-4 py-2 rounded font-medium transition-colors
```

#### Bouton secondaire
```typescript
className={getButtonClasses('secondary')}
// Résultat : bg-white hover:bg-gray-50 text-black border border-gray-300 px-4 py-2 rounded font-medium transition-colors
```

#### Bouton désactivé
```typescript
className={getButtonClasses('primary', true)}
// Résultat : bg-gray-200 text-gray-400 cursor-not-allowed border border-gray-200 px-4 py-2 rounded font-medium transition-colors
```

### 3. Indicateurs d'état

#### Joueur actuel
```typescript
import { getStateClasses } from '@/lib/theme';
className={getStateClasses('currentPlayer', 'p-3 rounded-lg border')}
// Résultat : bg-red-50 border-red-200 text-red-800 p-3 rounded-lg border
```

#### Gagnant
```typescript
className={getStateClasses('winner', 'p-4 rounded-xl')}
// Résultat : bg-red-100 border-red-300 text-red-900 p-4 rounded-xl
```

#### Partie terminée
```typescript
className={getStateClasses('finished', 'p-3 rounded-lg')}
// Résultat : bg-gray-100 border-gray-300 text-gray-700 p-3 rounded-lg
```

### 4. Messages et notifications

#### Information neutre
```typescript
import { getIndicatorClasses } from '@/lib/theme';
className={getIndicatorClasses('info')}
// Résultat : bg-gray-100 border-gray-300 text-gray-800 p-3 rounded-lg border
```

#### Erreur
```typescript
className={getIndicatorClasses('error')}
// Résultat : bg-red-100 border-red-300 text-red-800 p-3 rounded-lg border
```

### 5. Formulaires

#### Champ de saisie
```typescript
className="border-gray-300 focus:border-red-500 focus:ring-red-500 text-black"
```

#### Label
```typescript
className="text-gray-700 font-medium"
```

### 6. Cartes et surfaces

#### Carte principale
```typescript
className="bg-white border border-gray-200 rounded-lg p-6 shadow-sm"
```

#### Carte secondaire
```typescript
className="bg-gray-50 border border-gray-100 rounded-lg p-4"
```

### 7. Typographie

#### Titre principal
```typescript
className="text-black font-bold text-2xl"
```

#### Texte secondaire
```typescript
className="text-gray-600"
```

#### Texte désactivé
```typescript
className="text-gray-400"
```

## Règles de contraste WCAG

### Combinaisons validées (ratio > 4.5:1)
- ✅ `text-white` sur `bg-black` (21:1)
- ✅ `text-black` sur `bg-white` (21:1)
- ✅ `text-red-600` sur `bg-white` (6.4:1)
- ✅ `text-white` sur `bg-red-600` (5.7:1)
- ✅ `text-gray-600` sur `bg-white` (7.2:1)
- ✅ `text-white` sur `bg-gray-900` (18.3:1)

### Combinaisons à éviter
- ❌ `text-gray-400` sur `bg-white` (3.1:1)
- ❌ `text-red-300` sur `bg-white` (2.8:1)
- ❌ `text-white` sur `hover:bg-gray-50` (contraste insuffisant)
- ❌ `text-black` sur `hover:bg-white` (pas de différenciation visuelle)

## États interactifs

### Hover
```typescript
// ✅ Bon contraste avec texte noir sur fond blanc
className="hover:bg-gray-100 transition-colors"

// ✅ Parfait pour texte blanc (theme.text.primary)
className="hover:bg-gray-800 transition-colors"

// ❌ Éviter - contraste insuffisant avec texte blanc
className="hover:bg-gray-50 transition-colors"
className="hover:bg-gray-100 transition-colors" // avec theme.text.primary
```

**Règle importante** : 
- **Texte blanc** (`theme.text.primary`) : assombrir le fond avec `hover:bg-gray-800`
- **Texte noir** : éclaircir légèrement avec `hover:bg-gray-100`
- **Principe** : toujours aller vers plus de contraste, pas moins

### Focus
```typescript
className="focus:outline-none focus:ring-2 focus:ring-red-500 focus:border-red-500"
```

### Active
```typescript
className="active:bg-red-700"
```

### Disabled
```typescript
className="disabled:bg-gray-200 disabled:text-gray-400 disabled:cursor-not-allowed"
```

## Migration des composants existants

### Remplacements prioritaires

1. **Messages d'état** :
   - `bg-blue-100 text-blue-800` → `bg-red-50 text-red-800`
   - `bg-yellow-100 text-yellow-800` → `bg-red-100 text-red-900`
   - `bg-green-100 text-green-800` → `bg-gray-100 text-gray-800`

2. **Boutons** :
   - `bg-blue-600 hover:bg-blue-700` → `bg-red-600 hover:bg-red-700`
   - `bg-green-500 hover:bg-green-600` → `bg-red-600 hover:bg-red-700`

3. **Indicateurs** :
   - Point d'animation bleu → rouge
   - Bordures colorées → bordures grises ou rouges selon le contexte

## Cas spéciaux

### Dartboard
Les couleurs du dartboard conservent leur palette traditionnelle pour la lisibilité et la familiarité :
- Rouge : `#ff0037`
- Vert : `#009245`
- Noir : `#1b1b1b`
- Blanc : `#f2f2f2`

### Animations et effets
- Utilisez les variables CSS du thème pour les animations
- Conservez les effets de pulsation et glow en rouge
- Adaptez les dégradés à la palette rouge/gris

## Validation

Avant de valider une modification :
1. ✅ Vérifier le contraste WCAG AA
2. ✅ Tester les états hover/focus/active
3. ✅ Valider sur fond clair ET fond sombre
4. ✅ Confirmer la cohérence avec le système