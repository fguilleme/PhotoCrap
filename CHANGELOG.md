# Notes de version

Les fonctions disponibles et leurs limites sont détaillées dans le [README](README.md),
le [guide Sélections](docs/SELECTIONS.md) et la [roadmap](ROADMAP.md).
Un [résumé en anglais](README.en.md#new-in-v014) est également disponible.

## v0.14 — 28 octobre 2026

Tag Git : `v0.14`. Version de l’application et des paquets : `0.14.0`.

### Sélections

- Plage de couleurs avec prélèvement sur le canevas ou l’aperçu sans fermer le dialogue,
  ajout/retrait d’échantillons, tolérance, portée localisée et inversion.
- Option Zones contiguës pour restreindre la sélection aux régions reliées aux échantillons ;
  aperçus image/masque synchronisés avec zoom à résolution native et déplacement.
- Contour progressif (Maj+F6), rayon de 1 à 1 000 px, avec couverture partielle conservée
  par la peinture, les remplissages, la copie, les déplacements et les masques.
- Frange, Lisser, Dilater et Contracter : calcul régional asynchrone et annulable,
  avec rejet des résultats devenus obsolètes.
- Annulation, rétablissement et resélection sans rendre un projet sauvegardé « modifié »
  lorsque seuls les contours changent. Les couvertures sont conservées dans l’historique.
- Affichage du contour de sélection indépendant de la sélection active ; sélection des
  calques similaires par nature et commandes supplémentaires configurables.
- Catalogue de 298 raccourcis et gestes, dont 121 NYI désactivés et documentés.

### Dialogues de fichiers

- Correction du contraste des dialogues Qt Quick de repli sous Linux : en-têtes,
  chemin, liste de fichiers, ligne sélectionnée et boutons restent lisibles en thème sombre.
- Repli adapté à Qt 6.8 ; conservation des dialogues natifs lorsqu’ils sont disponibles.
- Test `file_dialog_fallback` couvrant ouverture, enregistrement, export, choix de dossier,
  contraste et ouverture effective d’une image, dans la matrice CI des trois plateformes.

### Compatibilité et limites

- Format `.pcr` version 8 conservé, avec lecture des versions 1 à 7. Les sélections et
  leur historique restent temporaires ; elles ne sont pas enregistrées dans les projets.
- Les pixels de travail restent en 8 ou 16 bits par canal ; la couverture d’une sélection
  progressive utilise un masque distinct à 256 niveaux.
- Sujet, Ciel, Sélectionner et masquer, Mode Masque, transformation de sélection et couches
  alpha nommées restent NYI. Les algorithmes ne garantissent pas un rendu identique à Adobe.
- DMG macOS arm64, AppImage Linux x86_64, installateur et ZIP Windows x64, avec SHA-256 ;
  publication automatique après réussite de tous les contrôles de la matrice GitHub Actions.

## v0.13.1

- Plume Bézier et édition de points, sauvegarde des tracés dans les projets v8,
  conversion en sélection sans poignées superposées, remplissage et contour raster.
- Gomme magique asynchrone, annulable et compatible avec les pixels 8/16 bits.
- Menus contextuels distincts pour les calques et leurs masques.
- Fermeture de l’application avec des panneaux flottants ou masqués.
- Correction des curseurs natifs macOS ; v0.13.1 remplace le tag de préparation
  v0.13.0, qui n’avait pas publié de paquets.

Les fonctions antérieures sont recensées dans la section
[Disponible dans v0.12.0](ROADMAP.md#disponible-dans-v0120) de la roadmap.
