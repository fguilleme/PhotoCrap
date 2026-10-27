# Roadmap PhotoCrap

État du projet au **27 octobre 2026**, dernière release publiée **v0.12.0**. Cette feuille de route distingue les fonctions incluses dans cette release, les ajouts de la branche de développement et les travaux restants. Les algorithmes sont propres à PhotoCrap ; une interface et des noms proches de Photoshop ne garantissent pas les mêmes résultats de rendu.

La [release v0.12.0](https://github.com/fguilleme/PhotoCrap/releases/tag/v0.12.0) est publiée pour macOS arm64, Linux x86_64 et Windows x64. Son [workflow de tag](https://github.com/fguilleme/PhotoCrap/actions/runs/38401049787) a réussi les builds, tests, déploiements, paquets et publication. Le [workflow de branche vérifié au commit `9303970`](https://github.com/fguilleme/PhotoCrap/actions/runs/38522922351) est également vert. Les précédents échecs intermittents du curseur natif macOS restent à surveiller.

Les paquets de v0.12.0 portent encore le numéro **0.11.0** dans leur nom et leur version interne, la version CMake n’ayant pas été augmentée lors du tag.

## Disponible dans v0.12.0

- [x] Documents raster RVB sRGB 8 ou 16 bits par canal, ouverture/import RAW asynchrone via LibRaw et import d’images. Projets `.pcr` version 7 compatibles en lecture avec les versions 1–6 ; export aplati, avec précision 16 bits conservée en PNG.
- [x] Calques de pixels, Couleur unie, réglages non destructifs, groupes, écrêtage et 27 modes de fusion avec aperçu au survol.
- [x] Masques raster : création, peinture, inversion, activation/désactivation, affichage isolé par Alt-clic et copie vers un autre calque par glisser. Alt-clic sur l’œil isole un calque puis restaure les visibilités.
- [x] Panneaux Histogramme, Navigateur, Couleur, Calques, Couches, Historique et Informations : redimensionnement, repli, déplacement, regroupement en onglets, colonnes, flottants et organisation sauvegardée. Visibilité contrôlée depuis Affichage → Panneaux.
- [x] Panneau Informations automatique pour les pipettes et la règle : lectures RVB, coordonnées, quatre échantillons, mesures et dimensions. Placement flottant ou ancré conservé.
- [x] Barre d’outils configurable : ordre des groupes et des outils, visibilité, variantes par clic droit, icône du dernier outil choisi et entrées futures désactivées avec mention NYI.
- [x] Réglages spécialisés : Lumière, Couleur, Niveaux, Courbes et pipettes, Balance des couleurs, Noir et blanc avec presets/Auto/Teinte, Filtre photo, Mélangeur de couches, Style colorimétrique (LUT), Transfert de dégradé, Correction sélective, Isohélie et Seuil.
- [x] Prévisualisation des réglages sur proxies, caches d’images et de masques, remplacement des demandes intermédiaires par la dernière, puis rendu complet en arrière-plan qui actualise le cache écran. Séparation de fréquences dynamique, puis préparation asynchrone de calques de pixels retouchables en 8/16 bits, avec annulation, masques conservés et retour au rayon depuis les sources enregistrées dans le projet.
- [x] Sélections rectangulaire, elliptique, rangée/colonne unique, lasso, lasso polygonal, sélection rapide et baguette magique ; combinaisons de sélection et contours animés.
- [x] Peinture : pinceau, crayon, gomme, remplacement de couleur, pinceau mélangeur, tampon de duplication et tampon de motif. Taille, dureté, opacité, débit, espacement, forme de pointe et lissage.
- [x] Retouche locale : flou, netteté, doigt, densité −/+, éponge, œil rouge, correcteur, correcteur localisé, pièce, déplacement basé sur le contenu et suppression. Calculs limités aux zones touchées et à leur voisinage lorsque le traitement le permet.
- [x] Forme d’historique et forme d’historique artistique avec source retenue, contrôles spécialisés et annulation par geste.
- [x] Pipette ponctuelle/moyenne, anneau comparatif, échantillonneur déplaçable et règle. Rectangle, ellipse, triangle, polygone, étoile et ligne raster avec options de remplissage/contour ; texte horizontal rasterisé.
- [x] Zoom centré sous le pointeur, déplacement temporaire par Espace, rotation de l’affichage et zoom conservé par Annuler/Rétablir.
- [x] Dialogues annulables pour le pinceau et les propriétés des calques, réinitialisation, boutons communs avec focus visible et comportement Retour/Espace. Retour valide d’abord l’édition d’un champ numérique.
- [x] Interface française/anglaise, raccourcis personnalisables, fichiers récents, historique à budget mémoire et sauvegarde atomique.
- [x] Builds et paquets autonomes sur trois plateformes, checksums SHA-256 et publication GitHub depuis les tags `v*`. Contrôle de déploiement et de signature ad hoc du DMG ; tests moteur, QML et natifs macOS.
- [x] Recadrage avec cadre indépendant, ratios, dimensions de sortie, poignées et grille ; recadrage en perspective asynchrone des calques, masques et sources.
- [x] Tranche et Sélection de tranche : création, déplacement, redimensionnement, division, sauvegarde et export PNG des tranches utilisateur ; repères automatiques facultatifs.
- [x] Catalogue de 275 raccourcis et gestes référencés à Adobe, avec recherche et filtres disponibles/NYI/contextuels dans Réglages. Les fonctions manquantes sont recensées dans [docs/SHORTCUTS.md](docs/SHORTCUTS.md).
- [x] Interactions communes inspirées des références Adobe : Maj/Option pour les variantes, raccourcis temporaires, contraintes de tracé, combinaisons de sélections, Maj-clic de peinture, chiffres pour opacité/débit, réglage de brosse à la souris et curseur précis. [Guide détaillé](docs/TOOL_INTERACTIONS.md).
- [x] Jusqu’à 512 calques, budget de 2 Gio de données de calques, stockage compact des calques vides et masques uniformes, historique comptant les pixels supplémentaires, cache de fréquences borné à 128 Mio et liste de calques virtualisée.

## Ajouts de la branche de développement (après v0.12.0)

Cette section décrit la branche de développement ; ces ajouts ne sont pas inclus dans les paquets du tag v0.12.0. Le catalogue actuel contient 282 entrées de raccourcis et gestes, dont 113 NYI après l’implémentation de la Gomme magique et de la Plume.

- [x] Plume en mode Tracé : Bézier, poignées, ajout/suppression/conversion de points, contraintes, reprise, fermeture, annulation et sauvegarde des tracés en projet v8. Conversion en sélection, remplissage et contour raster avec sélection et profondeur 8/16 bits conservées.
- [ ] Modes Forme et masque vectoriel, opérations booléennes et panneau Tracés complet. Plume libre/de courbure et sélections de tracés restent NYI.
- [x] Gomme magique : effacement par couleur avec tolérance, contiguïté, lissage, opacité et échantillonnage visible de tous les calques. Calcul asynchrone annulable, sélection respectée, pixels 8/16 bits et une étape d’annulation par clic ; outil réservé aux calques de pixels.
- [x] Menus contextuels distincts pour calques et vignettes de masque : propriétés et verrouillage côté calque, affichage/activation/inversion/suppression côté masque. Les actions portant sur un seul calque sont désactivées pour une sélection multiple ; les fonctions manquantes sont marquées NYI. Tests de clic droit ciblant un autre masque, d’annulation et de verrouillage.

## 1. Fiabilité et validation à terminer

- [ ] Confirmer la stabilité du premier affichage du curseur natif macOS : des tests ont relevé une flèche Cocoa malgré une pipette correcte dans l’état Qt. Les derniers workflows passent ; vérifier aussi les transitions de focus, de fenêtres flottantes et de survol sur Qt 6.8.3 avant de considérer ce défaut définitivement résolu.
- [ ] Étendre les tests d’espaces de travail : déplacements entre colonnes, sorties de groupes, dépôt avant le premier panneau, écrans multiples, changement de résolution et restauration de fenêtres hors écran.
- [ ] Étendre les essais au clavier, l’accessibilité, les curseurs, la haute densité et les tablettes sur les trois plateformes.
- [ ] Aligner la version CMake et les noms des paquets sur le prochain tag de release.
- [ ] Signature Developer ID et notarisation macOS ; signature Authenticode Windows. Les signatures de distribution ne sont pas encore configurées.

## 2. Grands documents et performance

Les proxies et le rendu asynchrone des réglages sont déjà disponibles. Il reste à couvrir les chemins encore synchrones et à réduire le coût mémoire des grandes images.

- [x] Jusqu’à 512 calques, stockage compact des calques vides et masques uniformes dans les projets v7 et suivants, allocation différée lors de la peinture, historique comptant les pixels supplémentaires et cache LRU de fréquences limité à 128 Mio. Tests de charge de 301 calques sur une image 24 Mpx en 16 bits et liste de calques virtualisée.
- [ ] Composition et stockage par tuiles, invalidation fine des caches et traitements régionaux plus systématiques.
- [ ] Généraliser le calcul asynchrone avec annulation des résultats périmés aux opérations complètes de calques, masques, transformations et filtres directs.
- [ ] Mesures de latence, mémoire et précision des aperçus par outil/filtre, notamment lorsque le rayon d’un flou est ramené à la résolution du proxy.
- [ ] Historique par différences, cache disque, récupération après fermeture anormale et autosauvegarde.
- [ ] Documents multiples et espaces de travail nommés/importables.
- [ ] Accélération GPU lorsque le gain et la fidélité du résultat sont mesurés ; réduction du bruit avancée.

## 3. Outils encore NYI

Les entrées existent dans le catalogue mais sont désactivées. Elles restent des fonctions futures, sans calendrier de livraison.

| Famille | Outils restant à implémenter |
|---|---|
| Déplacement et sélection | Plan de travail, pinceau de sélection, lasso magnétique, sélection d’objet |
| Recadrage et mise en page | Cadre |
| Mesure et annotation | Annotation, comptage |
| Gomme et réglage local | Gomme d’arrière-plan, pinceau de réglage |
| Tracés | Plume libre, plume de courbure, sélection de tracé et sélection directe |
| Texte et formes | Texte vertical, masque de texte horizontal, masque de texte vertical, forme personnalisée |

Améliorations des outils déjà disponibles :

- [ ] Conservation non destructive des pixels hors canevas, rotation/redressement du cadrage, résolution physique et presets personnalisés ; propriétés web/HTML et export des tranches automatiques.
- [ ] Compléter le déplacement multicalques, l’auto-sélection sous la souris, les transformations et le sélecteur contextuel de pointes/motifs.
- [ ] Contour progressif, affinage des sélections et traitement fiable des détails fins.
- [ ] Tolérance et options supplémentaires du remplissage contigu ; modes plus avancés de réparation et de pièce.
- [ ] Transformation au dépôt pour le déplacement basé sur le contenu ; meilleurs raccords sur les fonds complexes.
- [ ] Pression/inclinaison du stylet, pointes texturées, dynamiques de brosse et bibliothèque de motifs/pointes.
- [ ] Rectangle de navigation interactif dans le Navigateur.

## 4. Édition non destructive et couleur

- [ ] Compléter les commandes NYI des menus de calques : fusion des calques sélectionnés/visibles, aplatissement, étiquettes de couleur et options de fusion avancées.
- [ ] Compléter les commandes NYI des masques : appliquer le masque, associer/dissocier, charger comme sélection et Sélectionner et masquer.
- [ ] Masques vectoriels, effets de calque et objets dynamiques.
- [ ] Transformation libre, perspective, formes vectorielles et texte éditable.
- [ ] Fidélité colorimétrique des réglages et jeux d’images de référence ; les traitements actuels sont des implémentations sRGB simplifiées.
- [ ] Lectures supplémentaires dans Informations : espaces colorimétriques, opacité, avant/après et unités de mesure configurables.
- [ ] Gestion ICC complète, épreuvage écran/impression, RVB 32 bits, CMJN, tons directs et HDR.
- [x] Ouverture/import RAW asynchrone via LibRaw ; stockage, réglages photo, projets et PNG en 16 bits par canal.
- [ ] Porter les outils de réparation, Doigt, Pinceau mélangeur, Remplacement de couleur et formes d’historique en 16 bits.
- [ ] Développement RAW réglable, corrections optiques et réduction du bruit ; validation sur des fichiers de boîtiers réels.

## 5. Interopérabilité et extensibilité

- [ ] Import/export PSD/PSB sur un sous-ensemble explicitement testé : structure, pixels, masques et modes de fusion.
- [ ] Métadonnées EXIF/XMP, export avancé et traitement par lots.
- [ ] API de plugins versionnée, scripting et actions.
- [ ] Intégration facultative de modèles locaux après validation des licences, des ressources nécessaires et de la qualité réelle des masques/rendus.
- [ ] Comparaisons de rendu et mesures de performance avant toute revendication de compatibilité avec une version précise de Photoshop.

Les fonctions déjà réalisées restent détaillées dans [README.md](README.md) et [README.en.md](README.en.md). Les cases cochées sous « Disponible dans v0.12.0 » appartiennent à la release ; celles de la section suivante appartiennent à la branche de développement. Les cases non cochées sont des travaux restants.
