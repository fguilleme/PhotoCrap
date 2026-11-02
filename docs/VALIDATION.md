# Validation

## Animation des contours hors de la vue — 2 novembre 2026

- Tests Metal sur Qt 6.8 et 6.12 : rectangles et ellipses en 8 et 16 bits,
  aucun travail de préparation de phase ni transfert de géométrie quand aucun
  segment n’est visible, puis reprise avec la phase exacte au retour du bord.
- Le repli logiciel conserve son fonctionnement ; l’horloge de phase continue
  de tourner afin de conserver la synchronisation avec les autres guides.

## Surimpressions hors de la vue — 2 novembre 2026

- Les tests couvrent les régions fragmentées, les contacts aux bords, les UV de
  Pièce et les limites du maillage. Une comparaison locale avec les anciens
  chemins sur 300 régions, échelles et vues conserve les mêmes sommets.
- Sur une région synthétique de 4 000 rectangles hors écran, 1 000 préparations
  successives des deux surimpressions passent d’environ 25 ms à 0,01 ms sur ce
  Mac. Il s’agit de la préparation CPU, pas du temps de rendu GPU complet.

## Contours de sélection hors de la vue — 2 novembre 2026

- Les tests vérifient les traits sur la limite de découpage, les courbes dont
  seuls les points de contrôle entrent dans la vue et les contours complexes
  extérieurs. Une comparaison avec l’ancienne implémentation couvre 400
  contours et vues variés, sans changement de sommets.
- Mesure locale synthétique pour 200 préparations répétées d’un contour de
  12 000 segments entièrement hors de la vue : environ 42 ms → 0,02 ms. Le
  rectangle englobant est alors déjà en cache dans le chemin Qt ; ce résultat
  ne mesure pas le rendu GPU complet ni une sélection nouvellement construite.

## Préparation des contours de sélection — 2 novembre 2026

- Les raccords à angle droit, demi-tours, chemins fermés, zones découpées et
  limites du maillage sont vérifiés. Une comparaison locale avec l’ancienne
  implémentation donne les mêmes sommets sur 400 contours et vues variés.
- Sur un contour synthétique de 12 000 segments visibles, la préparation CPU
  mesurée localement passe d’environ 227 à 206 ms pour 200 calculs. Ce résultat
  ne mesure pas le temps de rendu GPU complet.

## Sélection des textures en cache — 2 novembre 2026

- Comparaison avec l’algorithme précédent sur 500 ensembles déterministes de
  80 tuiles, budgets variés, égalités et tailles irrégulières. Les tuiles
  visibles restent conservées même si elles dépassent le budget.
- Mesure locale synthétique sur 8 192 entrées et 200 appels : environ 75 → 1 ms
  avec budget nul et 101 → 48 ms quand toutes les tuiles tiennent. Un budget
  intermédiaire nécessitant le tri donne environ 66 → 69 ms. Ces mesures
  concernent la sélection CPU du cache, pas le temps de rendu GPU complet.

## Animation de sélection hors écran — 2 novembre 2026

- Le test d’interface vérifie l’arrêt de l’animation lorsque la fenêtre est
  masquée ou réduite, puis sa reprise dans les deux cas après réaffichage.
- La sélection elle-même reste inchangée ; cette étape évite seulement les
  mises à jour périodiques de la phase quand la fenêtre n’est pas visible.

## Accueil et miniatures des fichiers récents — 2 novembre 2026

- Builds et `qml_smoke` sur Qt 6.8.3 et Qt 6.12.0 ; le test couvre les
  miniatures PNG et JPEG, les chemins accentués et contenant `%`, l’aperçu RAW
  intégré, la reconstruction d’un projet `.pcr`, le remplacement quand un fichier
  est illisible et la poignée de redimensionnement.
- Un DNG synthétique avec aperçu JPEG teste aussi l’extraction LibRaw dans le
  moteur ; le DNG sans aperçu vérifie le retour vide. Le fichier réel
  `IMG_9908.CR2` a permis de vérifier l’orientation et le décodage réduit.
- Le chargement des vignettes est asynchrone. Les projets `.pcr` sont composités
  puis mis en cache ; les RAW dépourvus d’aperçu intégré affichent un emplacement
  de remplacement.

## Préparation GUI des phases de sélection — 2 novembre 2026

- Builds Qt 6.8.3 / 6.12.0 et tests `canvas_render`, `tone_render`, `qml_smoke` ;
  `tile_cache` passe aussi sur le kit local. Formatage et clang-tidy propres.
- Contrôles natifs Metal (Qt 6.8.3 et 6.12.0) et OpenGL (Qt 6.12.0) sur macOS
  Apple Silicon : animation visible sans nouvelle préparation complète des
  snapshots/tuiles, géométrie et textures conservées, phases fractionnaires,
  rectangles/ellipses et lassos en 8/16 bits.
- Une baisse du budget mémoire coalescée avec une phase doit réellement évincer
  les tuiles hors champ. Une modification de visibilité de calque coalescée avec
  une phase doit rafraîchir la photo, puis retrouver ses pixels après restauration.
  Ces contrôles vérifient que le chemin court ne masque pas les demandes complètes.
- La synchronisation du thread graphique reste inchangée. Aucun gain chiffré en
  millisecondes ou en fréquence d’affichage n’est déduit des seuls compteurs.
- Vulkan/Direct3D restent à contrôler sur leurs plateformes.

## Candidats de lasso sans bord visible — 2 novembre 2026

- Builds Qt 6.8.3 / 6.12.0 et contrôles `lasso_guide`, `qml_smoke` ;
  `selection_mesh` passe aussi sur le kit local. Clang-tidy et formatage propres.
- Comparaisons natives Metal et OpenGL sur macOS Apple Silicon, en 8/16 bits.
  À 8×, le contour entoure la vue avec tous ses segments hors champ :
  `layer.enabled` reste désactivé, aucun Canvas n’est chargé sur le chemin GPU
  et les changements de phase ne rappellent pas `updatePaintNode`.
- Les compteurs de géométrie, transferts et paramètres restent stables dans ce
  cas. Revenir sur un bord conserve le même composant et la dernière phase,
  réactive sa composition et retrouve les pixels de la référence indépendante.
- Le repli logiciel garde son rendu. Les contrôles matériels Vulkan/Direct3D
  restent à exécuter sur leurs plateformes.

## Pointillés des candidats combinés du lasso — 2 novembre 2026

- Compilation et tests `lasso_guide` / `qml_smoke` sur Qt 6.8.3 et Qt 6.12.0 ;
  `selection_mesh` passe aussi sur le kit local. Contrôle clang-tidy des deux
  nouveaux fichiers C++ sans avertissement.
- Comparaisons natives sur macOS Apple Silicon : Metal 1×/1,25×/2× et OpenGL, avec
  référence Canvas indépendante en 8/16 bits. Phases fractionnaires et négatives,
  extrémités plates, croisements, zoom 64×, défilement, rotation et faible opacité.
- Les compteurs attestent que la phase conserve le maillage et ses buffers,
  sans repeindre de Canvas. Entrées non finies/hors budget, angles aigus,
  raccords sous-pixel au zoom 0,005×, transition Alt, masquage et réaffichage
  conservent le repli et les pixels. Les surfaces aux dimensions fractionnaires
  sont comparées avec la même tolérance de filtrage que les surfaces tournées.
- Les angles presque superposés du guide uni ont un repli plus conservateur
  (cosinus supérieur à 0,99), pour préserver les pointes QPainter aussi à 1×.
  Sur écran fractionnaire, le guide uni garde le Canvas à l’origine et à la taille
  de la vue pour éviter un changement de filtrage ; les nouveaux pointillés restent GPU.
- Vulkan et Direct3D n’ont pas été exécutés localement.

## Guides de tranches après v0.16 — 2 novembre 2026

- Build local et contrôles `slice_cache`, `slice_overlay`, `qml_smoke` sur Qt 6.8.3
  et Qt 6.12.0. Comparaisons natives sur Metal 1×/2×, OpenGL et repli à échelle
  d’écran fractionnaire, sur macOS Apple Silicon.
- Référence raster indépendante sur documents 8/16 bits, tranches automatiques,
  petites étiquettes débordantes, zoom jusqu’à 64×, rotation, superposition de
  poignées et opacité de l’ensemble des guides.
- Identité du modèle et des objets conservée au petit défilement ; absence de
  repeint du grand Canvas et du cache de chiffres observé. Repli au-delà de 128
  descriptions visibles et destruction des surfaces au masquage/retour GPU.
- Les vérifications de rendu matériel Vulkan/Direct3D restent à exécuter sur les
  plateformes concernées ; ces résultats locaux ne les remplacent pas.

## Validation de v0.16 — 2 novembre 2026

- Les 43 tests hors écran passent localement sur Apple Silicon avec Qt 6.8.3 et
  Qt 6.12.0. Les builds, tests et paquets des trois plateformes sont validés dans le
  [workflow de release](https://github.com/fguilleme/PhotoCrap/actions/runs/41430300211).
- Le test de survol des presets force le placement de la ListView avant de déplacer
  la souris : sous Qt 6.8, l’ouverture précède l’activation des lignes pour les événements.
  Les tests de survol, annulation, clic, clavier et historique restent actifs.
- Sous Windows, CTest fournit le dossier système des polices à Qt offscreen lorsque
  `QT_QPA_FONTDIR` n’est pas déjà défini. Le test Texte exige une vraie base de polices.
- Les suites GPU paramétrées disposent de 90 secondes au total sur les runners ;
  leurs assertions et délais individuels restent inchangés.
- Un worker volontairement retenu reproduit la concurrence entre le contour
  progressif et le proxy auxiliaire : la dernière demande de masque doit rester
  prioritaire. Le test couvre 8/16 bits, le rendu auxiliaire et Annuler/Rétablir.
- Le test de survol de la plume attend le rendu initial puis celui du guide
  provisoire avant de vérifier l’absence de repeint du tracé et des poignées.
- La CI utilise `macos-15` après le retrait de `macos-14`. En cas d’échec, les
  artefacts `Test-diagnostics-*` conservent les rapports CTest et Texte. Les messages
  Qt Windows sont redirigés vers stderr dans le workflow.

## Validation du 30 septembre 2026

Environnement exécuté : macOS Apple Silicon, AppleClang 21, Qt 6.11.2, CMake 4.4.3, compilation Release.

| Vérification | Résultat |
|---|---|
| Configuration et compilation CMake dans `~/Dev/PhotoCrap` | Réussies |
| Tests du moteur, 11 scénarios Qt Test | Réussis |
| Démarrage QML et rendu hors écran | Réussis, aucune erreur QML |
| Dessin via événements souris, annulation, saisie et protection des modifications | Réussis |
| Installation CMake et déploiement des dépendances Qt | Réussis |
| Démarrage du bundle `dist/PhotoCrap.app` avec Cocoa | Réussi |
| Tests d’interaction et rendu dans le bundle Cocoa | Réussis |
| Chargement depuis Homebrew ou `/usr/local` pendant ce test du bundle | Aucun, vérifié avec `DYLD_PRINT_LIBRARIES=1` |
| Liens symboliques du bundle vers l’extérieur ou liens cassés | Aucun |
| Compilation Linux / Windows | Non exécutée localement ; workflow GitHub fourni |

Le test CTest complet contient deux cibles, `document` et `qml_smoke` : 2/2 réussies.

Le bundle macOS embarque le plugin Cocoa. Le test hors écran documenté dans le README utilise le **bundle de développement** et le Qt installé, qui fournit aussi le plugin offscreen. Pour vérifier le bundle installé, utiliser son interface native :

```sh
./dist/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test
```

Le bundle compilé ici est destiné à macOS arm64. Il n’est pas signé avec une identité Developer ID et n’est pas notarisé. Les binaires d’autres plateformes doivent être produits avec leurs kits Qt respectifs.
