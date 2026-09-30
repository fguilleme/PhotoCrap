# Validation du 30 septembre 2026

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
