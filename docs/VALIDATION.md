# Validation

## Préparation de v0.16 — 2 novembre 2026

- Les 43 tests hors écran passent localement sur Apple Silicon avec Qt 6.8.3 et
  Qt 6.12.0. La validation des paquets GitHub sur les trois plateformes reste requise.
- Le test de survol des presets force le placement de la ListView avant de déplacer
  la souris : sous Qt 6.8, l’ouverture précède l’activation des lignes pour les événements.
  Les tests de survol, annulation, clic, clavier et historique restent actifs.
- Sous Windows, CTest fournit le dossier système des polices à Qt offscreen lorsque
  `QT_QPA_FONTDIR` n’est pas déjà défini. Le test Texte exige une vraie base de polices.
- Les suites GPU paramétrées disposent de 90 secondes au total sur les runners ;
  leurs assertions et délais individuels restent inchangés.
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
