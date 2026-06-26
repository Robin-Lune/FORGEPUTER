# HOME

## Objectif

Fournir le premier ecran de demarrage de Forgeputer.

## Perimetre

- Initialiser l'affichage du Cardputer ADV.
- Afficher l'identite Forgeputer.
- Verifier la lecture clavier.
- Envoyer les entrees clavier vers le moniteur serie.

## Dependances

- M5Cardputer
- M5Unified
- M5GFX

## Architecture

Implementation temporaire dans `src/main.cpp`.

Le module sera extrait plus tard vers `src/apps/home/` quand l'interface commune des applications sera creee.

## API

Aucune API publique pour l'instant.

## Etats

- `Ready` : l'ecran est initialise.
- `Input received` : une ligne clavier a ete validee.

## Flux utilisateur

1. Le firmware demarre.
2. L'ecran affiche `Forgeputer`.
3. L'utilisateur saisit du texte au clavier.
4. La touche entree envoie la ligne dans le moniteur serie.

## Limites

- Pas encore d'AppManager.
- Pas encore de navigation entre applications.
- Pas encore de theme UI partage.

## Roadmap

- Extraire HOME dans son propre module applicatif.
- Brancher HOME sur l'interface commune `init()`, `update()`, `draw()`, `onKey()`, `close()`.
- Ajouter une navigation vers Settings.

## TODO

- Valider le rendu sur le Cardputer ADV reel.
- Valider la compatibilite du `.bin` avec M5Launcher.
