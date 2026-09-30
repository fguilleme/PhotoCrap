# Vers un éditeur comparable à Photoshop

La version 0.1 livre les fondations raster. Cette feuille de route distingue les fonctions existantes des travaux encore nécessaires ; aucune des fonctions futures ci-dessous n’est annoncée comme disponible.

## 1. Fiabilité et grands documents

- Composition par tuiles, mises à jour limitées aux régions modifiées et moteur de filtres asynchrone.
- Historique par différences, cache disque, sauvegarde de récupération et reprise après fermeture anormale.
- Documents multiples, zoom centré sous le pointeur, panneaux redimensionnables et espaces de travail persistants.
- Vérification sur machines Windows et Linux, paquets natifs, tests d’interaction et d’accessibilité étendus.

## 2. Édition non destructive

- Masques raster et vectoriels avec édition indépendante des pixels.
- Calques de réglage, groupes, effets de calque et masques d’écrêtage.
- Objets dynamiques, transformation libre, perspective et texte éditable.
- Sélections polygonales, lasso, baguette, contour progressif et amélioration des contours.

## 3. Peinture et photographie

- Moteur de brosses avec pression, inclinaison, texture, espacement, dureté et stabilisation.
- Tampon de duplication, correcteur, pièces, fluidité et outils de retouche locale.
- Courbes, niveaux, balance des couleurs, histogramme et filtres accélérés.
- Développement RAW via une bibliothèque dédiée, optique et réduction du bruit.

## 4. Interopérabilité et couleur

- Import/export PSD/PSB sur un sous-ensemble explicitement testé : structure, pixels, masques et modes de fusion.
- Gestion ICC complète, RVB 16/32 bits, CMJN, tons directs, HDR et épreuvage.
- Formats interchangeables, métadonnées EXIF/XMP, export avancé et automatisation par lots.

## 5. Extensibilité

- API de plugins versionnée, scripting, actions et intégration de modèles locaux facultatifs.
- Jeux de fichiers de référence, comparaisons de rendu et mesures de performance avant toute revendication de compatibilité.

La parité avec Photoshop exige de définir une version cible et une matrice de compatibilité. Elle ne peut pas être déduite de la seule présence d’une interface ressemblante.
