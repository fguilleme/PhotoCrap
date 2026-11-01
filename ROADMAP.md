# Roadmap PhotoCrap

État du projet au **1er novembre 2026**, version courante **v0.15** (version interne **0.15.0**). Cette feuille de route distingue les fonctions livrées dans v0.12.0, v0.13.1, v0.14 et v0.15 des travaux restants. Les algorithmes sont propres à PhotoCrap ; une interface et des noms proches de Photoshop ne garantissent pas les mêmes résultats de rendu.

La [release v0.15](https://github.com/fguilleme/PhotoCrap/releases/tag/v0.15) fournit les paquets macOS arm64, Linux x86_64 et Windows x64 en version **0.15.0**, avec SHA-256. La publication est automatique après réussite des builds, tests et contrôles des paquets : [état du workflow](https://github.com/fguilleme/PhotoCrap/actions/workflows/build.yml). Le [workflow de branche au commit `bcbbc18`](https://github.com/fguilleme/PhotoCrap/actions/runs/40849316101) a déjà validé ces contrôles sur les trois plateformes. Voir les [notes de version](CHANGELOG.md).

## Repères actuels vérifiés

| Élément | État dans v0.15 | Source |
|---|---|---|
| Barre d’outils | 71 outils recensés : **54 disponibles, 17 NYI** | Catalogue de `qml/Main.qml` |
| Raccourcis et gestes | **298 entrées** : 123 commandes personnalisables, 56 gestes contextuels, 119 NYI | `src/ShortcutManager.cpp`, `src/ShortcutCatalog.h` |
| Projets | Format `.pcr` **v9** avec texte éditable, lecture des versions 1 à 8 ; sélections et historique temporaires | `src/DocumentIO.cpp`, [guide Sélections](docs/SELECTIONS.md) |
| Précision | Pixels RVB 8/16 bits, masques et couvertures de sélection 8 bits | [Précision et limites](README.md#raw-et-précision-des-couleurs) |

Les sections « Disponible » et « Ajouts » ci-dessous constituent l’historique des fonctions livrées : leurs anciens nombres de raccourcis et formats de projet ne décrivent pas la version courante. Les sections numérotées regroupent les travaux restants et les validations à poursuivre. **NYI** signifie recensé mais non implémenté ; les gestes contextuels disponibles ne sont pas des commandes globales personnalisables.

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
- [x] Pipette ponctuelle/moyenne, anneau comparatif, échantillonneur déplaçable et règle. Rectangle, ellipse, triangle, polygone, étoile et ligne raster avec options de remplissage/contour ; texte horizontal éditable en calque dédié (ponctuel/paragraphe, formats mixtes, polices, paragraphes et sauvegarde v9).
- [x] Création automatique de calques par Texte et les six formes raster ; choix Nouveau calque/Pixels sur calque actif dans leur barre. Pinceau/Crayon créant un calque sur cible non peignable selon une préférence persistante, masques conservés, insertion dans les groupes, annulation atomique du dessin et du calque, abandon du calque provisoire et limites mémoire/calques respectées. Les formes vectorielles restent à faire ; règles et écarts Adobe documentés dans [docs/TOOL_LAYERS.md](docs/TOOL_LAYERS.md).
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

## Ajouts de v0.15 : calques, mémoire et cache

- [x] Éviter les copies pleine résolution des sources et du composite à chaque calque en mode Normal, ainsi que la copie finale du résultat du worker.
- [x] Écarter les plans invisibles de la préparation des proxies, sans casser les chaînes d’écrêtage ni la précision du rendu complet.
- [x] Réutiliser une bande haute encore dans le LRU sans recalculer sa bande basse évincée ; conserver le plafond de 128 Mio.
- [x] Ajouter une pile de 64 calques remplis en 8/16 bits, comparée à une référence de rendu, avec retouche locale et Annuler/Rétablir ; tester l’éviction des fréquences et les calques/groupes masqués.
- [x] Première étape du cache disque : blocs natifs 8/16 bits vérifiés par SHA-256, mappings en lecture seule, budgets, éviction, nettoyage et protection des sessions actives. Branché sur les bandes de fréquences d’au moins 16 Mio, dans les workers ; configuration et vidage depuis Réglages → Cache de travail.
- [x] Moteur de plans parcimonieux par tuiles de 256 × 256, versions partagées et stockage disque avec références indépendantes de l’index du cache. Premier usage : couverture du pinceau, du crayon et de la gomme, avec tests 8/16 bits, masques, débit et annulation. Les plans de calques restent des QImage complètes.

## 1. Fiabilité et validation à terminer

- [x] Curseurs natifs macOS corrigés à la première activation des pipettes et de la règle, après réinitialisation par AppKit et après ouverture d’Informations ; gestes Commande de la Plume couverts par les tests natifs. Ces contrôles passent dans le workflow v0.14 sur Qt 6.8.3.
- [ ] Étendre ces contrôles de curseurs aux autres outils, transitions de focus, écrans multiples et fenêtres flottantes, au-delà des scénarios automatisés actuels.
- [ ] Étendre les tests d’espaces de travail : déplacements entre colonnes, sorties de groupes, dépôt avant le premier panneau, écrans multiples, changement de résolution et restauration de fenêtres hors écran.
- [ ] Étendre les essais au clavier, l’accessibilité, les curseurs, la haute densité et les tablettes sur les trois plateformes.
- [x] Version CMake et noms des paquets alignés sur 0.15.0 (tag v0.15) ; l’AppImage reprend automatiquement la version configurée.
- [ ] Signature Developer ID et notarisation macOS ; signature Authenticode Windows. Les signatures de distribution ne sont pas encore configurées.

## 2. Grands documents et performance

Les proxies et le rendu asynchrone des réglages sont déjà disponibles. Il reste à couvrir les chemins encore synchrones et à réduire le coût mémoire des grandes images.

- [x] Jusqu’à 512 calques, stockage compact des calques vides et masques uniformes dans les projets v7 et suivants, allocation différée lors de la peinture, historique comptant les pixels supplémentaires et cache LRU de fréquences limité à 128 Mio. Test de stockage/rechargement de 301 calques sur une image 24 Mpx en 16 bits : les calques ajoutés sont vides et les réglages masqués ; ce n’est pas un test de 301 photos remplies. Liste de calques virtualisée.
- [ ] Composition et stockage par tuiles, invalidation fine des caches et traitements régionaux plus systématiques.
- [ ] Généraliser le calcul asynchrone avec annulation des résultats périmés aux opérations complètes de calques, masques, transformations et filtres directs.
- [ ] Mesures de latence, mémoire et précision des aperçus par outil/filtre, notamment lorsque le rayon d’un flou est ramené à la résolution du proxy.
- [ ] Stockage de travail par tuiles avec cache disque configurable pour les pixels, masques et versions d’historique ; éviction réelle de RAM, calculs asynchrones et contrôle d’espace libre. Une première étape existe pour les rendus de fréquences ; les plans éditables et l’historique ne sont pas encore déchargés sur disque. [Audit, implémentation et suite](docs/LAYER_MEMORY.md).
- [ ] Historique par différences, récupération après fermeture anormale et autosauvegarde, distincts du cache temporaire de travail.
- [ ] Documents multiples et espaces de travail nommés/importables.
- [x] Première étape d’affichage après v0.15 : canevas en textures du graphe de scène Qt Quick, blocs visibles réutilisables, niveau de détail d’affichage conservé et surimpressions séparées de la photo ; repli logiciel et test `canvas_render`. [Fonctionnement et limites](docs/GPU_RENDERING.md).
- [x] Premiers aperçus GPU Courbes/Niveaux : base écran opaque réutilisée et table RVB issue du moteur CPU, actualisation auxiliaire du Navigateur/histogrammes, repli CPU pour les cas non pris en charge et rendu final natif 8/16 bits. Tests de référence CPU, d’annulation et de textures sur Metal (M2 Pro) ; autres backends matériels à vérifier.
- [x] Aperçu GPU du panneau Lumière : Exposition, Contraste, Hautes lumières, Ombres, Blancs et Noirs via une table de luminance CPU et une reconstruction couleur dans le shader. Réutilisation de la base écran, historique et rendu final 8/16 bits ; contrôles extrêmes et couleurs saturées comparés à la référence sur Metal.
- [x] Aperçus GPU Couleur et Balance des couleurs : paramètres Température/Teinte/Vibrance/Saturation calculés directement dans le shader, table RVB partagée avec le calcul CPU de Balance ; base réutilisée, repli logiciel et rendu final 8/16 bits. Comparaison des couleurs et valeurs extrêmes sur Metal et OpenGL en boucle principale.
- [x] Aperçus GPU Noir et blanc et Transfert de dégradé : six familles calculées dans le shader, palettes de gris/colorisation et de dégradé issues du moteur CPU. Réutilisation des textures photo, annulation et rendu final natif 8/16 bits ; couleurs saturées/atténuées, extrêmes des familles et teintes comparés sur Metal et OpenGL en boucle principale.
- [x] Aperçus GPU Filtre photo et Mélangeur de couches via des matrices couleur : densité et préservation de luminosité, coefficients et constantes RVB/monochromes. Paramètres transmis sans recréer les textures ; repli CPU et rendu final natif 8/16 bits. Comparaisons des valeurs extrêmes sur Metal et OpenGL en boucle principale.
- [x] Aperçu automatique des menus de styles Noir et blanc, Filtre photo et Mélangeur de couches au survol et au clavier ; validation unique, restauration sur Échap/fermeture et respect du contrôle Aperçu. Composant commun et test d’interface `preset_preview`.
- [x] Aperçus GPU Isohélie et Seuil : table de paliers issue du CPU et luminance entière pour le seuil noir/blanc ; base écran réutilisée, annulation et rendu final natif 8/16 bits. Comparaison dense des rampes, des 31 réglages d’Isohélie et des limites de Seuil sur Metal et OpenGL en boucle principale.
- [x] Aperçus GPU Style colorimétrique et Correction sélective : six styles, intensité/luminosité, neuf familles et modes relatif/absolu. Paramètres de shader regroupés dans un bloc de 224 octets, textures réutilisées et rendu final natif 8/16 bits. Comparaisons Metal/OpenGL en boucle principale et repli CPU testé pour les anciens projets à correction globale.
- [x] Aperçus GPU des anciens réglages Luminosité/Contraste, Exposition et Vibrance : tables CPU ou calcul direct conservant leurs règles propres, dont le facteur négatif de Vibrance. Calculs CPU Exposition/Vibrance séparés, rendu final 8/16 bits et alpha conservés. Registre commun au moteur et aux tests de rendu pour couvrir les seize réglages.
- [x] Aperçus GPU Teinte/Saturation (conversion HSL) et table fixe de Négatif ; calculs CPU séparés et Négatif sans conversions QColor par pixel. Correction des gris devenant noirs avec une teinte négative. Couverture des dix-huit réglages, de 135 combinaisons HSL et de la précision/alpha 16 bits ; ajout de Négatif et rendu final toujours sur CPU asynchrone.
- [x] Composition des aperçus GPU en mode Normal avec opacité réduite et masques uniformes blanc/gris/noir, densité et contour progressif. Paramètres sans texture supplémentaire, tables identiques réutilisées, historique/annulation et rendu final natif 8/16 bits préservés. Comparaison aux compositions QPainter sur les dix-huit réglages ; masques peints et autres modes restent sur CPU.
- [x] Aperçus GPU des réglages avec masque peint : proxy de masque préparé en arrière-plan et blocs visibles alignés sur la photo. Densité sans retransfert, contour progressif ne remplaçant que le masque, conservation de la base écran, rejet des demandes périmées et rendu final natif CPU. Tests des dix-huit réglages, des frontières de blocs, du zoom, de l’annulation et du redimensionnement ; peinture toujours sur CPU.
- [x] Quatorze modes de fusion pour les aperçus GPU des réglages, dont Produit, Écran, Incrustation, Lumières crue/linéaire/ponctuelle, Différence et Exclusion. Masque prémultiplié puis mélange selon la référence CPU ; survol sans retransfert de textures ni agrandissement du bloc de paramètres. Tests des dix-huit réglages, historique/annulation en 8/16 bits et repli des autres modes. Composition des calques de pixels toujours sur CPU.
- [x] Fusions GPU Teinte, Saturation, Couleur et Luminosité pour dix réglages à entrée exacte, sans atténuation du masque. Conversion HSL partagée et quantifiée comme QColor, opacité du calque conservée et repli CPU explicite pour les autres combinaisons. Comparaisons de pixels, historique/annulation et rendu final 8/16 bits ; changement de fusion sans retransfert de photo/table.
- [x] Aperçus GPU Densité couleur +/− et Division pour les dix réglages à entrée exacte et couverture de masque complète. Rapports calculés depuis les canaux entiers, branches zéro/255 conservées et aucun retransfert de photo au changement de mode. Tests des limites, opacités 37/100 %, historique/annulation en 8/16 bits et replis CPU partagés avec les fusions HSL.
- [x] Aperçus GPU Couleur plus foncée/claire et Lumière tamisée/vive pour les dix réglages à entrée exacte et couverture de masque complète. Comparaisons de sommes RVB entières, égalités conservant la base et formules de contraste inchangées. Tests des quarts/demi-tons, sources proches de zéro, valeurs connues, historique et rendu final 8/16 bits. 25 modes éligibles selon les cas à cette étape.
- [x] Aperçus GPU Fondu et Mélange maximal sans interpolation : motif de Fondu stable dans les coordonnées du proxy, seuil d’opacité entier et origine des tuiles ; décisions de Mélange maximal aux frontières partagées avec le moteur CPU dans une seconde ligne de table. Couverture de masque complète requise et dix réglages à entrée exacte pour Mélange maximal. Les 27 modes sont désormais éligibles selon les conditions ; rendu final natif toujours CPU. Tests des 256 frontières complémentaires, opacités limites, changement de zoom, tuiles et historique 8/16 bits.
- [x] Transferts régionaux des textures au niveau de détail entier : le composite identifie sa zone de sortie avec ses clés avant/après, et seuls les blocs touchés et leurs raccords sont remplacés. Prise en compte des halos de filtres et masques adoucis ; repli complet pour les coordonnées absolues ou les versions intermédiaires manquantes. Tests de pixels et de transferts 8/16 bits, frontières, hors champ, déplacement, annulation et niveaux réduits.
- [x] Transferts régionaux aux niveaux d’affichage réduits : mise à jour partielle du cache sur les grilles de réduction entières, sans conserver le composite natif. Dimensions irrégulières redimensionnées intégralement par Qt pour conserver leur phase d’échantillonnage, mais seules les tuiles touchées sont retransférées. Comparaisons exactes à Qt avec alpha et 8/16 bits, tests de raccords/hors champ aux niveaux 1/2 et mesure séparée des pixels rééchantillonnés.
- [x] Réutilisation des tuiles de masque inchangées pendant les aperçus GPU : comparaison des pixels visibles et de leurs bordures sans recopier les blocs identiques, clés de texture indépendantes par tuile et suppression correcte des textures lors de la désactivation du masque. Densité sans transfert ; contour progressif ne retransférant que les blocs modifiés. Tests de références couleur en 8/16 bits, raccords, niveaux réduits et formats Grayscale8/RVB/RGBA64. Le calcul du proxy et de son flou reste intégral sur CPU en arrière-plan.
- [x] Cache graphique de navigation LRU réglable dans Préférences → Cache de travail (64 Mio par défaut, 0–1 024 Mio) : conservation des tuiles photo/masque récemment affichées, éviction des moins récentes et retrait des nœuds hors champ du dessin. Budget estimé des textures du canevas ; zone visible prioritaire même si elle le dépasse. Invalidation des retouches hors champ et suivi différé des versions de masque, sans conserver le composite natif. Tests de budget, persistance, liaison QML, navigation 8/16 bits et masques modifiés hors champ.
- [x] Contours animés des sélections générales sur GPU : géométrie de trait limitée à la vue, pointillés anticrénelés animés par une constante de shader, sans recréer d’image ni de géométrie à chaque phase. Phase conservée au déplacement, transformations du graphe de scène et découpage de la vue. Repli QPainter sur backend logiciel ou au-delà d’un million de sommets ; rectangles/ellipses/lassos QML conservés. Tests de trous, découpage, grandes coordonnées, phases fractionnaires, rotation et absence de remplissage.
- [x] Marquage de l’outil Suppression en géométrie GPU : rectangles disjoints de la région limités à la vue, mélange translucide sans texture intermédiaire, trous et annulation préservés. Découpage local commun aux contours de sélection, y compris sous rotation. Repli QPainter logiciel ou au-delà d’un million de sommets. Tests d’aire, trous, coordonnées, transparence, animation simultanée et protection des pixels 16 bits ; la reconstruction Suppression reste CPU et réservée aux pixels 8 bits.
- [x] Aperçu de Pièce en texture GPU bornée, avec géométrie texturée suivant la région et ses trous. Aucun second rendu de la taille de la vue ; Source réutilise la géométrie et Destination conserve les textures donneuses identiques. Transparence, déplacement, découpage, rotation et annulation comparés à la référence raster. Repli logiciel, complexité et allocation ; la préparation du donneur et la réparation finale restent CPU.
- [x] Réparation de Pièce asynchrone au relâchement : adaptation locale, comparaison des pixels et copie du calque sur worker ; aperçu conservé jusqu’au composite réparé, une seule annulation, abandon par Échap/Annuler et rejet des résultats obsolètes. Tests Source/Destination, contour progressif, transparence, gestes rapprochés, changement de calque/document et absence de modification.
- [x] Assemblage final du Déplacement basé sur le contenu sur worker : copies locales, raccords Source/Destination, transparence, comparaison et copie du calque complet hors du thread GUI ; couverture douce limitée aux régions touchées sans parcourir leur espace vide. Aperçu conservé jusqu’au composite, annulation des calculs obsolètes et protection de la sauvegarde/fermeture. Tests des modes Déplacement/Extension, chevauchement, contour progressif, tous les calques et rendu Metal/OpenGL.
- [x] Finition de Suppression sur worker : développement différé des calques compacts, copies et assemblage des pixels, comparaison et couverture douce limitée aux marques. Installation du résultat sans copie complète sur le thread GUI, annulation sans résultat tardif et protection de la fermeture/sauvegarde pendant le calcul. Tests des calques vides, échantillonnage global, transparence, trous, contour progressif, absence de modification et rendu logiciel/Metal/OpenGL.
- [x] Contour progressif de la Gomme magique fusionné avec l’effacement sur worker en 8/16 bits : interpolation native uniquement sur les pixels correspondants, aucun second détachement complet sur le GUI et rectangle modifié calculé après atténuation. Annulation lors d’un changement de réglage, garde commune des quatre retouches asynchrones pour Annuler, fermeture et sauvegarde. Comparaisons au mélange séparé, lissage, contiguïté, opacité, tous les calques et transparence à l’écran Metal/OpenGL.
- [x] Contours de rectangle et d’ellipse dans le chemin commun GPU/logiciel du canevas : courbes elliptiques directes, trait cosmétique comme les sélections générales, géométrie mise en cache par forme/rectangle/zoom/vue et phase animée sans texture. Suppression du Canvas QML d’ellipse et du repeint QML pour le rectangle ; lignes/colonnes uniques et lassos conservés. Tests du changement rectangle→ellipse aux mêmes dimensions, courbure, absence de remplissage, animation fractionnaire, déplacement, rotation et masquage sur Metal/OpenGL.
- [x] Contours des lassos libre et polygonal terminés dans le chemin GPU/logiciel commun : points natifs, fermeture et concavités conservées, géométrie réutilisée pour la phase et invalidée au déplacement du contour. Guides en cours conservés en QML et limités à la vue ; disparition du tracé candidat QML après validation, y compris pour une sélection combinée. Tests des transitions, annulation, retouches Pièce/Déplacement, phases fractionnaires, clipping, rotation, 8/16 bits et pixels inchangés.
- [x] Contours de rangée et colonne uniques dans le chemin GPU/logiciel commun : bande d’un pixel natif suivant le zoom, géométrie limitée à la vue, phase sans retransfert et repli raster sous forme de bande étroite. Suppression du dernier Canvas QML de sélection rectangulaire. Tests 8/16 bits aux zooms 0,5×/1×/4×, clipping, rotation, pixels préservés et maillage borné pour 16 384 pixels à 8×.
- [x] Candidat du lasso libre sur le contour GPU commun dès qu’il forme une zone : points natifs, animation sans texture et réutilisation de la dernière géométrie au relâchement. Même chemin lors du détourage initial de Pièce et Déplacement basé sur le contenu ; repli logiciel borné. Guide ouvert initial, poignées polygonales et candidat combiné conservés en QML. Tests du lasso en 8/16 bits et des deux retouches en 8 bits : geste, mouvement, masquage, annulation, historique et absence de doublon QML.
- [x] Surimpressions de mesure bornées et séparées du panneau principal : anneau de pipette de 84 × 84 pixels logiques, déplacé par le graphe de scène sans repeindre à couleur constante ; règle limitée à l’emprise visible de son segment et de ses poignées. Tests de rendu sur une géométrie 16K à 16× : couleurs, absence de repeint au déplacement, coordonnées après défilement/rotation, règle traversant le document et disparition au masquage.
- [x] Contour du pinceau dans un composant borné à la vue : dessin local réutilisé au déplacement sans découpage, repeint limité lors du découpage, conservation de l’angle/aplatissement et des marges hors image visibles. Signes +/− de Sélection rapide positionnés dans les limites et gardés droits. Tests logiciel/Metal/OpenGL des pixels, absence de repeint, ellipse, clipping, rotation, repositionnement et empreinte de 32 000 pixels affichés ; liaison réelle du curseur à la souris dans le smoke test QML.
- [x] Surimpression Plume séparée en courbe, segment provisoire et poignées, dans leur ordre de dessin : surfaces limitées aux enveloppes visibles, snapshot des points conservé entre notifications et survol sans repeindre les parties fixes. Modifications/annulation réactualisant les caches, fermeture et désactivation supprimant le guide. Comparaisons à l’ancien dessin en 8/16 bits, zoom/défilement, compteurs de repeint et gestes réels du smoke test QML.
- [x] Guides de lasso extraits et bornés à leur enveloppe visible : snapshot natif conservé entre changements, vidé lorsque le contour passe au canevas partagé, et aucune animation de phase pour les guides unis. Dessin en une passe conservant l’anticrénelage ; candidats combinés et transition Alt gardant leurs pointillés. Comparaisons à l’ancien rendu en 8/16 bits, zoom/défilement, masquage, validation, annulation et vérification des repeints/du minuteur dans l’interface.
- [x] Cache natif des descriptions de tranches par dimensions, rectangles manuels et affichage automatique : buffer partagé réutilisé pendant le survol, la sélection et les gestes provisoires, invalidation naturelle après édition/Annuler/Rétablir/ouverture. Snapshots QML partagés dans les contrôles et le canevas ; rendu limité aux tranches visibles avec marge pour étiquettes/poignées, sans repeint sur les réglages sans rapport. Tests de partition fragmentée plafonnée à 1 024 descriptions, buffers, projets et rendu 8/16 bits comparé à l’ancien canevas.
- [x] Voile des recadrages classique et perspective sur GPU : rectangle de quatre sommets, ouverture par quatre plans et mise à jour des paramètres sans texture ni rechargement de géométrie au déplacement. Coordonnées locales indépendantes du regroupement Qt ; cadre/grille/poignées raster limités à leur emprise visible, snapshot des points réutilisé au survol et repli logiciel complet. Comparaisons à l’ancien rendu en 8/16 bits sur logiciel, Metal et OpenGL : zoom, déplacement, rotation, cadre hors champ, ouverture dégénérée et annulation.
- [x] Cadre, grille et poignées de recadrage classique/perspective dans le graphe de scène GPU via Qt Quick Shapes CurveRenderer et rectangles conservés, sans Canvas de guides. Repli raster logiciel/anciens kits Qt ; tests des pixels, zoom, déplacement, rotation, annulation et absence de repeint raster, avec différences de lissage explicitement bornées.
- [x] Trait et repères circulaires de la règle sur GPU, sans Canvas ni texture intermédiaire ; composants communs de vue/CurveRenderer avec le recadrage. Repli logiciel/Qt ancien, lectures Informations inchangées et tests des points, zooms, défilement, rotation, mesure 16K, masquage et absence de repeints raster sur GPU.
- [x] Anneau de pipette procédural sur GPU : deux couleurs et liserés sans Canvas ni texture source, mises à jour par paramètres, transparence et opacité héritée préservées. Repli logiciel borné et repli sur erreur de shader ; comparaison à l’ancien rendu pour les couleurs, alpha, déplacement, redimensionnement, rotation et masquage sur logiciel/Metal/OpenGL, sans repeint raster sur GPU.
- [x] Contour de pinceau procédural sur GPU, sans Canvas ni texture même pendant un déplacement coupé par la vue. Angle, aplatissement, traits contrastés, signes +/− et opacité conservés ; repli logiciel borné. Comparaisons à l’ancien dessin sur logiciel/Metal/OpenGL : formes, rotation, clipping, 32 000 pixels, diamètre d’un pixel, aplatissement minimal, coordonnées fractionnaires et réglages modifiés pendant le masquage.
- [x] Tracé principal de la Plume conservé sur GPU via CurveRenderer : deux traits sans Canvas ni texture, cache au survol et préparation des cubiques en quadratiques avec erreur bornée à 0,01 pixel logique. Emprise visible découpée, repli logiciel/Qt ancien, poignées et segment provisoire inchangés. Comparaisons 8/16 bits sur logiciel/Metal/OpenGL : édition, Annuler, zoom 64×, défilement fractionnaire, rotation, fermeture, ancre seule, angles, croisements et réactivation.
- [x] Cadre provisoire des tranches sur GPU, séparé du fond numéroté : création, déplacement et redimensionnement sans repeindre les tranches fixes. Révision du cache natif filtrant les notifications de brouillon dans le canevas et les options ; buffer et snapshots réutilisés. Repli logiciel/Qt ancien et comparaisons 8/16 bits sur logiciel/Metal/OpenGL, avec annulation, validation, zoom, défilement, rotation et compteurs de repeint.
- [ ] Étendre les filtres et la composition sur GPU aux autres réglages, masques et mélanges ; autres surimpressions par shaders et budget global de VRAM. Mesurer les gains et la fidélité sur chaque backend matériel. Réduction du bruit avancée.

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

- [x] Texte éditable ponctuel/paragraphe, double-clic sur la vignette T, saisie/curseur/sélection sur l’image, formats mixtes, familles/styles, couleur, interligne, approche, crénage métrique, échelle horizontale, ligne de base, capitales et indices/exposants ; alignements, retraits et espacements de paragraphes. Annulation par session, projets v9, transformations conservées et pixellisation explicite. [Inventaire Adobe et limites](docs/TEXT_TOOL.md).
- [ ] Texte sur tracé, déformations, texte vertical/masques, options OpenType et polices variables, styles réutilisables, crénage optique, lissages Adobe, typographie CJK détaillée, césure, correction/recherche et Adobe Fonts.
- [ ] Revoir la conformité de la Plume avec Photoshop : interactions de création et d’édition des tracés, souris et clavier. Des écarts sont signalés ; leur correction est différée après les optimisations graphiques.
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
- [ ] Transformation libre, perspective interactive des calques et formes vectorielles.
- [ ] Dégradé non destructif avec calque de remplissage et poignées ; l’outil actuel reste un dégradé raster classique.
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
- [ ] Gestes contextuels complémentaires : source du tampon décalable/rotative/redimensionnable, rapporteur de la règle, réglage Noir et blanc sur l’image, raccourcis détaillés des points de Courbes et raccourcis typographiques avancés. L’inventaire par geste est dans [docs/SHORTCUTS.md](docs/SHORTCUTS.md).

## 7. Interopérabilité et extensibilité

- [ ] Import/export PSD/PSB sur un sous-ensemble explicitement testé : structure, pixels, masques et modes de fusion.
- [ ] Métadonnées EXIF/XMP et Informations du fichier ; impression, export web avancé et traitement par lots.
- [ ] API de plugins versionnée, scripting et actions.
- [ ] Intégration facultative de modèles locaux après validation des licences, des ressources nécessaires et de la qualité réelle des masques/rendus.
- [ ] Comparaisons de rendu et mesures de performance avant toute revendication de compatibilité avec une version précise de Photoshop.

Les fonctions déjà réalisées restent détaillées dans [README.md](README.md) et [README.en.md](README.en.md). Les sections historiques et les cases cochées décrivent les fonctions livrées ; les contrôles CI attestent les scénarios testés, pas une validation exhaustive de tous les usages. Les cases non cochées sont des travaux restants.
