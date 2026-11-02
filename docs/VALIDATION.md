# Validation

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
