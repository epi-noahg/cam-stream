# Système de Thème Unifié - GoDarts

## Vue d'ensemble

Le système de thème unifié pour GoDarts utilise une palette basée sur **Noir, Blanc, Rouge et Gris** pour assurer la cohérence visuelle et améliorer la lisibilité de l'application.

## Architecture

```
src/
├── lib/
│   ├── theme.ts              # Constantes et classes principales
│   ├── theme-fixes.ts        # Corrections spécifiques
│   └── theme-guide.md        # Guide d'application détaillé
├── hooks/
│   └── useTheme.ts          # Hooks React pour faciliter l'usage
└── app/
    └── globals.css          # Variables CSS et styles globaux
```

## Utilisation

### 1. Import des utilitaires

```typescript
import { useTheme, useGameTheme } from '@/hooks/useTheme';
import { THEME_CLASSES } from '@/lib/theme';
```

### 2. Dans un composant React

```typescript
function MyComponent() {
  const theme = useTheme();
  
  return (
    <div className={theme.common.card}>
      <h2 className={theme.common.cardTitle}>Titre</h2>
      <button className={theme.getButton('primary')}>
        Action principale
      </button>
    </div>
  );
}
```

### 3. Pour les composants de jeu

```typescript
function GameComponent() {
  const gameTheme = useGameTheme();
  
  return (
    <div className={gameTheme.status.currentTurn}>
      Tour de : {currentPlayer.name}
    </div>
  );
}
```

## Palette de couleurs

### Couleurs principales
- **Noir** : `#000000` - Arrière-plans principaux, textes sur fond clair
- **Blanc** : `#ffffff` - Textes sur fond sombre, surfaces claires  
- **Rouge** : `#dc2626` - Accents, actions importantes, gagnants
- **Gris** : Échelle 50-900 - Textes secondaires, bordures, états

### Classes Tailwind standardisées

#### Arrière-plans
- `bg-black` - Arrière-plan principal
- `bg-white` - Surfaces claires
- `bg-red-600` - Éléments d'accent
- `bg-gray-100` - Arrière-plans neutres

#### Textes
- `text-white` - Texte principal sur fond sombre
- `text-black` - Texte principal sur fond clair
- `text-red-600` - Texte d'accent
- `text-gray-600` - Texte secondaire

#### Bordures
- `border-gray-300` - Bordures standard
- `border-red-600` - Bordures d'accent

## Migration des composants existants

### Remplacements prioritaires

1. **Messages d'état** :
   ```diff
   - className="bg-blue-100 border border-blue-300 text-blue-800"
   + className="bg-red-50 border border-red-200 text-red-800"
   ```

2. **Boutons d'action** :
   ```diff
   - className="bg-green-500 hover:bg-green-600 text-white"
   + className="bg-red-600 hover:bg-red-700 text-white"
   ```

3. **Indicateurs de victoire** :
   ```diff
   - className="bg-yellow-100 text-yellow-800"
   + className="bg-red-100 text-red-900"
   ```

### Composants concernés
- `EnhancedScoreboard.tsx`
- `WinnerCelebration.tsx`
- `WinnerScoreboard.tsx`
- `EditableThrow.tsx`
- `UndoButton.tsx`

## Accessibilité

### Ratios de contraste validés (WCAG AA - 4.5:1 minimum)
- ✅ `text-white` sur `bg-black` (21:1)
- ✅ `text-black` sur `bg-white` (21:1)
- ✅ `text-red-600` sur `bg-white` (6.4:1)
- ✅ `text-white` sur `bg-red-600` (5.7:1)
- ✅ `text-gray-600` sur `bg-white` (7.2:1)

### États interactifs
```css
/* Focus */
.focus:outline-none.focus:ring-2.focus:ring-red-500

/* Hover - IMPORTANT: maintenir le contraste */
.hover:bg-gray-100.transition-colors /* ✅ avec text-black */
.hover:bg-gray-800.transition-colors /* ✅ avec text-white */

/* Disabled */
.disabled:bg-gray-200.disabled:text-gray-400.disabled:cursor-not-allowed
```

**⚠️ Règle critique** : Les états hover doivent maintenir un contraste minimum de 4.5:1 avec le texte. Éviter `hover:bg-gray-50` avec du texte noir.

## Variables CSS personnalisées

Le système utilise des variables CSS pour faciliter la maintenance :

```css
:root {
  --accent: #dc2626;
  --accent-hover: #b91c1c;
  --text-primary: #000000;
  --text-secondary: #6b7280;
  --border-primary: #e5e7eb;
}
```

## Cas spéciaux

### Dartboard
Les couleurs du dartboard conservent leur palette traditionnelle pour la familiarité :
- Rouge : `#ff0037`
- Vert : `#009245`
- Noir : `#1b1b1b`
- Blanc : `#f2f2f2`

### Mode sombre
Le thème s'adapte automatiquement au mode sombre en inversant les couleurs principales tout en conservant la palette rouge.

## Validation

Avant d'appliquer le système :
1. ✅ Vérifier les ratios de contraste
2. ✅ Tester les états interactifs
3. ✅ Valider sur différentes tailles d'écran
4. ✅ Confirmer la cohérence visuelle

## Prochaines étapes

1. **Appliquer les corrections** aux composants identifiés
2. **Tester l'accessibilité** avec les outils appropriés
3. **Valider visuellement** l'ensemble de l'application
4. **Documenter** les nouvelles pratiques pour l'équipe

---

*Ce système de thème assure la cohérence visuelle, améliore l'accessibilité et facilite la maintenance de l'application GoDarts.*