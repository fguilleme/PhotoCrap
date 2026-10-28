# Roadmap PhotoCrap

État du projet au **28 octobre 2026**, version courante **v0.14** (version interne **0.14.0**). Cette feuille de route distingue les fonctions livrées dans v0.12.0, v0.13.1 et v0.14 des travaux restants. Les algorithmes sont propres à PhotoCrap ; une interface et des noms proches de Photoshop ne garantissent pas les mêmes résultats de rendu.

La [release v0.14](https://github.com/fguilleme/PhotoCrap/releases/tag/v0.14) est publiée. Son [workflow de tag](https://github.com/fguilleme/PhotoCrap/actions/runs/40735732946) a réussi les builds, tests, déploiements, paquets et publication sur macOS arm64, Linux x86_64 et Windows x64. Les quatre paquets et leurs quatre fichiers SHA-256 sont disponibles, en version **0.14.0**. Voir les [notes de version](CHANGELOG.md).

## Repères actuels vérifiés

| Élément | État dans v0.14 | Source |
|---|---|---|
| Barre d’outils | 71 outils recensés : **54 disponibles, 17 NYI** | Catalogue de `qml/Main.qml` |
| Raccourcis et gestes | **298 entrées** : 123 commandes personnalisables, 54 gestes contextuels, 121 NYI | `src/ShortcutManager.cpp`, `src/ShortcutCatalog.h` |
| Projets | Format `.pcr` **v8**, lecture des versions 1 à 7 ; sélections et historique temporaires | `src/DocumentIO.cpp`, [guide Sélections](docs/SELECTIONS.md) |
| Précision | Pixels RVB 8/16 bits, masques et couvertures de sélection 8 bits | [Précision et limites](README.md#raw-et-précision-des-couleurs) |

Les sections par version ci-dessous constituent l’historique des fonctions livrées : leurs anciens nombres de raccourcis et formats de projet ne décrivent pas la version courante. Les sections numérotées regroupent les travaux restants et les validations à poursuivre. **NYI** signifie recensé mais non implémenté ; les gestes contextuels disponibles ne sont pas des commandes globales personnalisables.

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

## Ajouts de v0.13.1

Ces ajouts sont inclus dans le tag v0.13.1. La publication des paquets est automatique après validation des trois plateformes : [état du workflow](https://github.com/fguilleme/PhotoCrap/actions/workflows/build.yml). Le catalogue de v0.13.1 contient 282 entrées de raccourcis et gestes, dont 113 NYI après l’implémentation de la Gomme magique et de la Plume.

- [x] Plume en mode Tracé : Bézier, poignées, ajout/suppression/conversion de points, contraintes, reprise, fermeture, annulation et sauvegarde des tracés en projet v8. Conversion en sélection, remplissage et contour raster avec sélection et profondeur 8/16 bits conservées.
- [x] Conversion Plume → sélection sans points ni poignées superposés ; tracé conservé et réactivable, avec curseur Commande/Ctrl corrigé et test natif macOS.
- [x] Fermeture de l’application avec panneaux flottants/masqués et protection des documents non sauvegardés.
- [x] Gomme magique : effacement par couleur avec tolérance, contiguïté, lissage, opacité et échantillonnage visible de tous les calques. Calcul asynchrone annulable, sélection respectée, pixels 8/16 bits et une étape d’annulation par clic ; outil réservé aux calques de pixels.
- [x] Menus contextuels distincts pour calques et vignettes de masque : propriétés et verrouillage côté calque, affichage/activation/inversion/suppression côté masque. Les actions portant sur un seul calque sont désactivées pour une sélection multiple ; les fonctions manquantes sont marquées NYI. Tests de clic droit ciblant un autre masque, d’annulation et de verrouillage.

## Ajouts de v0.14 : sélections et dialogues

- [x] Menu Sélection structuré selon la référence Adobe, au-delà des entrées visibles dans la capture. [Inventaire des comportements, options et écarts](docs/SELECTIONS.md).
- [x] Frange (1–200 px), Lisser, Dilater et Contracter (1–100 px), calcul asynchrone régional annulable, rejet des résultats obsolètes et historique des sélections sans modifier les pixels sauvegardés.
- [x] Contour de sélection masquable depuis Affichage, réaffiché à la création suivante ; Calques similaires par nature du calque ; raccourcis configurables. Catalogue de v0.14 : **298 entrées, dont 121 NYI**.
- [x] Comptabilisation des régions et contours partagés dans le budget mémoire de l’historique.
- [x] Couverture progressive 0–255, Contour progressif (rayon 1–1 000 px, Maj+F6), conservation dans l’historique et Resélectionner ; prise en compte par les opérations de pixels et les masques.
- [x] Plage de couleurs : pipettes remplacer/ajouter/retirer sur l’aperçu et le canevas, tolérance, groupes localisés, zones contiguës, inversion et aperçus image/masque synchronisés avec zoom natif et déplacement ; familles de couleurs et de tons.
- [x] Dialogues de fichiers Qt Quick lisibles en thème sombre : palette complète, repli adapté à Qt 6.8 et test `file_dialog_fallback` pour ouvrir, enregistrer, exporter et choisir un dossier. Les dialogues natifs restent utilisés lorsqu’ils sont disponibles.

## 1. Fiabilité et validation à terminer

- [x] Curseurs natifs macOS corrigés à la première activation des pipettes et de la règle, après réinitialisation par AppKit et après ouverture d’Informations ; gestes Commande de la Plume couverts par les tests natifs. Ces contrôles passent dans le workflow v0.14 sur Qt 6.8.3.
- [ ] Étendre ces contrôles de curseurs aux autres outils, transitions de focus, écrans multiples et fenêtres flottantes, au-delà des scénarios automatisés actuels.
- [ ] Étendre les tests d’espaces de travail : déplacements entre colonnes, sorties de groupes, dépôt avant le premier panneau, écrans multiples, changement de résolution et restauration de fenêtres hors écran.
- [ ] Étendre les essais au clavier, l’accessibilité, les curseurs, la haute densité et les tablettes sur les trois plateformes.
- [x] Version CMake et noms des paquets alignés sur 0.14.0 (tag v0.14) ; l’AppImage reprend automatiquement la version configurée.
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

## 3. Sélections et masques à compléter

Plage de couleurs, Contour progressif, Frange, Lisser, Dilater, Contracter, Resélectionner et l’affichage des contours sont livrés. Les étapes restantes sont :

- [ ] Anticrénelage fractionnaire des outils, Mode Masque et chargement depuis un calque/masque/couche.
- [ ] Tons chair/visages, hors gamut CMJN, réglages tonals détaillés, aperçus et préréglages Adobe complémentaires ; Étendre/Généraliser selon la tolérance de la Baguette magique.
- [ ] Zone de mise au point, Sujet, Ciel et espace Sélectionner et masquer, avec validation des détails fins et calculs asynchrones.
- [ ] Transformation du contour indépendante des pixels ; mémorisation/récupération dans des couches alpha persistantes.
- [ ] Aucun calque actif, recherche et isolation par filtrage du panneau Calques.
- [ ] Paramètres de contour progressif et d’anticrénelage lors de la conversion Plume → sélection ; le contour progressif peut déjà être appliqué après conversion.

Les paramètres et écarts avec Adobe sont détaillés dans [docs/SELECTIONS.md](docs/SELECTIONS.md). Les sélections de pixels, la sélection de calques, les masques de calque et les couches alpha sont des opérations distinctes.

## 4. Outils encore NYI

Les **17 outils NYI** du catalogue sont tous recensés ci-dessous et désactivés dans l’interface. Ils restent des fonctions futures, sans calendrier de livraison.

| Famille | Outils restant à implémenter |
|---|---|
| Déplacement et sélection | Plan de travail, pinceau de sélection, lasso magnétique, sélection d’objet |
| Recadrage et mise en page | Cadre |
| Mesure et annotation | Annotation, comptage |
| Gomme et réglage local | Gomme d’arrière-plan, pinceau de réglage |
| Tracés | Plume libre, plume de courbure, sélection de tracé et sélection directe |
| Texte et formes | Texte vertical, masque de texte horizontal, masque de texte vertical, forme personnalisée |

Améliorations des outils déjà disponibles :

- [ ] Modes Forme et masque vectoriel de la Plume, opérations booléennes et panneau Tracés complet. Le mode Tracé, les outils d’ajout/suppression/conversion de point et leur conversion en sélection sont déjà disponibles.
- [ ] Conservation non destructive des pixels hors canevas, rotation/redressement du cadrage, résolution physique et presets personnalisés ; propriétés web/HTML et export des tranches automatiques.
- [ ] Compléter le déplacement multicalques, l’auto-sélection sous la souris, les transformations et le sélecteur contextuel de pointes/motifs.
- [ ] Tolérance et options supplémentaires du remplissage contigu ; modes plus avancés de réparation et de pièce.
- [ ] Transformation au dépôt pour le déplacement basé sur le contenu ; meilleurs raccords sur les fonds complexes.
- [ ] Pression/inclinaison du stylet, pointes texturées, dynamiques de brosse et bibliothèque de motifs/pointes.
- [ ] Rectangle de navigation interactif dans le Navigateur.

## 5. Édition non destructive et couleur

- [ ] Compléter les commandes NYI des calques : fusion des calques sélectionnés/visibles, calque des visibles fusionnés, aplatissement, placement direct tout en haut/en bas, verrouillage des pixels transparents, étiquettes de couleur, pixellisation et options de fusion avancées.
- [ ] Compléter les commandes NYI des masques : appliquer le masque, associer/dissocier, charger comme sélection et Sélectionner et masquer.
- [ ] Masques vectoriels, effets de calque et objets dynamiques.
- [ ] Transformation libre, perspective, formes vectorielles et texte éditable.
- [ ] Fidélité colorimétrique des réglages et jeux d’images de référence ; les traitements actuels sont des implémentations sRGB simplifiées.
- [ ] Lectures supplémentaires dans Informations : espaces colorimétriques, opacité, avant/après et unités de mesure configurables.
- [ ] Gestion ICC complète, épreuvage écran/impression, RVB 32 bits, CMJN, tons directs et HDR.
- [x] Ouverture/import RAW asynchrone via LibRaw ; stockage, réglages photo, projets et PNG en 16 bits par canal.
- [ ] Porter les outils de réparation, Doigt, Pinceau mélangeur, Remplacement de couleur et formes d’historique en 16 bits.
- [ ] Développement RAW réglable, corrections optiques et réduction du bruit ; validation sur des fichiers de boîtiers réels.

## 6. Commandes d’édition et d’affichage encore NYI

Ces manques sont déjà indiqués dans le catalogue des raccourcis, même lorsque leur panneau ou une commande voisine existe :

- [ ] Couper/copier les pixels du calque actif, nouveau calque par copie/couper de sélection, Coller sur place et Coller dans la sélection. Aujourd’hui Copier utilise la composition visible, Coller crée un calque et Dupliquer copie le calque entier.
- [ ] Taille de la zone de travail indépendante du redimensionnement de l’image ; rétablissement du fichier enregistré.
- [ ] Réglages directs Niveaux, Courbes, Teinte/Saturation, Balance des couleurs et Noir et blanc, ainsi que Tonalité/Contraste/Couleur automatiques et Désaturer. Les panneaux de calques de réglage existants ne rendent pas ces commandes directes disponibles.
- [ ] Répéter le dernier filtre et atténuer la dernière opération ; Fluidité et filtre de développement RAW.
- [ ] Dialogue Remplir complet (motif, historique, contenu) ; le sélecteur de couleur et les remplissages premier/arrière-plan sont déjà disponibles.
- [ ] Grille de travail, repères, magnétisme, Extras, modes d’écran et masquage de la barre supérieure des options. Les règles, Tab/Maj+Tab et l’affichage du contour de sélection existent déjà.
- [ ] Gestes contextuels complémentaires : source du tampon décalable/rotative/redimensionnable, rapporteur de la règle, réglage Noir et blanc sur l’image, raccourcis détaillés des points de Courbes et édition du texte. L’inventaire par geste est dans [docs/SHORTCUTS.md](docs/SHORTCUTS.md).

## 7. Interopérabilité et extensibilité

- [ ] Import/export PSD/PSB sur un sous-ensemble explicitement testé : structure, pixels, masques et modes de fusion.
- [ ] Métadonnées EXIF/XMP et Informations du fichier ; impression, export web avancé et traitement par lots.
- [ ] API de plugins versionnée, scripting et actions.
- [ ] Intégration facultative de modèles locaux après validation des licences, des ressources nécessaires et de la qualité réelle des masques/rendus.
- [ ] Comparaisons de rendu et mesures de performance avant toute revendication de compatibilité avec une version précise de Photoshop.

Les fonctions déjà réalisées restent détaillées dans [README.md](README.md) et [README.en.md](README.en.md). Les sections historiques et les cases cochées décrivent les fonctions livrées ; les contrôles CI attestent les scénarios testés, pas une validation exhaustive de tous les usages. Les cases non cochées sont des travaux restants.
