# Notes de version

Les fonctions disponibles et leurs limites sont détaillées dans le [README](README.md),
le [guide Sélections](docs/SELECTIONS.md) et la [roadmap](ROADMAP.md).
Un [résumé en anglais](README.en.md#new-in-v015) est également disponible.

## v0.15 — 29 octobre 2026

Tag Git : `v0.15`. Version de l’application et des paquets : `0.15.0`.

### Calques et mémoire

- Réduire les copies des sources et du composite pendant les rendus en mode Normal,
  ainsi que la copie pleine résolution au retour du worker.
- Écarter les calques invisibles, les groupes masqués et les calques d’opacité nulle
  de la préparation des proxies, en conservant la précision du rendu complet.
- Réutiliser les bandes hautes encore présentes dans le LRU sans reconstruire leur
  bande basse évincée. Le cache RAM de fréquences reste limité à 128 Mio.
- Vérifier le rendu de 64 calques remplis en 8/16 bits, avec masques, opacités,
  retouche locale et Annuler/Rétablir. Clarifier la portée du test des 301 calques,
  qui valide le stockage compact et le rechargement.

### Cache de travail disque

- Stocker les gros rendus de fréquences dans des blocs natifs 8/16 bits sans
  recompression, avec SHA-256, publication atomique et lectures mappées en lecture seule.
- Réutiliser les blocs entre workers avant de recalculer ; conserver les blocs encore
  utilisés après leur éviction de l’index et nettoyer les sessions abandonnées.
- Ajouter Réglages → Cache de travail : dossier configurable, budget de 0 à 64 Gio
  (4 Gio par défaut), réserve de 512 Mio d’espace libre, statistiques et vidage.
  Zéro désactive le cache. Un stockage indisponible laisse le rendu fonctionner.
- Garder les petits aperçus et les demandes de vignettes de l’interface en RAM.
  Les bandes préparées pour la retouche deviennent des pixels éditables indépendants
  des fichiers temporaires.

### Couvertures de traits par tuiles

- Allouer uniquement les tuiles de 256 × 256 touchées par le pinceau, le crayon ou
  la gomme, sur les pixels et les masques, avec débit cumulatif et précision 8/16 bits.
- Réduire la couverture d’un petit trait sur un masque de document 24 Mpx/16 bits
  à 512 Kio, au lieu d’environ 183 Mio pour le plan complet.
- Introduire le partage de versions de tuiles et leur stockage disque avec références
  indépendantes de l’index du cache, pour préparer la migration des plans de calques.
- Étendre les tests aux frontières des tuiles, à la concurrence, aux erreurs disque,
  à l’annulation du geste et à Annuler/Rétablir.

### Compatibilité et limites

- Format `.pcr` v8 inchangé, lecture des versions 1 à 7 ; fonctionnalités de v0.14 conservées.
- Les plans éditables des calques et l’historique restent des images complètes en RAM.
  Le stockage par tuiles de ces plans n’est pas encore disponible.
- Limites inchangées : 32 Mpx, 512 calques et 2 Gio de plans de calques.
  Le cache de travail temporaire ne tient pas lieu d’autosauvegarde.

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
