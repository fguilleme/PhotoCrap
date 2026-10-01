# PhotoCrap

Éditeur d’images de bureau en **C++20, Qt 6 et QML**, conçu pour macOS, Linux et Windows. L’interface, le moteur de pixels et le format de projet sont indépendants d’Adobe.

La version 0.1 est une première application fonctionnelle, **pas une reproduction complète de Photoshop**. Les fonctions disponibles et les limites sont détaillées ci-dessous ; les développements nécessaires à une parité plus large sont dans [ROADMAP.md](ROADMAP.md).

![Interface PhotoCrap](docs/screenshot.png)

## Fonctionnalités

- Documents RVB 8 bits sRGB, fond blanc ou transparent, dimensions personnalisées et préréglages.
- Import d’images, ouverture de projets, import centré comme calque et collage du presse-papiers.
- Pinceau rond et gomme à empreintes : taille, dureté, opacité, débit cumulatif, espacement, angle, arrondi et lissage. Le contour de pointe suit le pointeur et un aperçu de trait est affiché dans **Affichage → Paramètres de pinceau…**. Les raccourcis suivent Photoshop pour la taille (`[`/`]`), la dureté (`Maj+[`/`Maj+]`) et les couleurs avant/arrière-plan (`D`/`X`).
- Pipette, remplissage contigu, dégradé à deux couleurs, rectangle, ellipse, ligne et texte rasterisé.
- Sélection rectangulaire, déplacement d’un calque ou des pixels sélectionnés, effacement, copie fusionnée et recadrage.
- Calques : ajout, duplication, renommage, ordre, visibilité, verrouillage, opacité et fusion vers le bas. Les masques de fusion peuvent être créés en blanc ou en noir, peints, inversés, activés et désactivés.
- Les 27 modes de fusion RVB décrits par Adobe, classés dans l’ordre de Photoshop : modes normaux, obscurcissement, éclaircissement, contraste, comparaison et composantes TSL.
- Vingt-trois calques de réglage non destructifs : les seize réglages colorimétriques, plus flou gaussien, flou directionnel, flou de boîte, netteté optimisée, passe-haut, mosaïque et ajout de bruit. Chaque réglage reçoit automatiquement un masque blanc ou un masque dérivé de la sélection active.
- Séparation de fréquences conforme à la structure décrite par Adobe : un calque de basses fréquences flouté et un calque de hautes fréquences lié, placé en mode Lumière linéaire. Le rayon commun est recalculé pendant le déplacement du curseur.
- Réglages du calque actif : luminosité, contraste, saturation, noir et blanc, négatif, sépia, flou et netteté. Les sélections limitent leur zone d’application.
- Redimensionnement, rotation à 90°, symétries, zoom, déplacement du canevas, règles et aperçu de navigation.
- Historique annuler/rétablir avec suivi de l’état enregistré. Protection à l’ouverture, à la création et à la fermeture d’un document modifié.
- Interface disponible en français et en anglais, sélectionnable à chaud dans **Réglages → Interface**. Le choix est conservé et les catalogues Qt permettent d’ajouter d’autres langues.
- Enregistrement atomique des projets `.pcr` avec leurs calques. Export aplati PNG, JPEG, BMP et WebP si le module correspondant est installé. Le JPEG et le BMP compositent la transparence sur blanc.
- Une composition de démonstration éditable : menu **Fichier → Ouvrir la composition de démonstration**, ou option `--demo`.

## Compilation

Prérequis : CMake ≥ 3.24, compilateur C++20 et **Qt ≥ 6.5** avec Core, Gui, Concurrent, Qml, Quick, QuickControls2, LinguistTools et Test (pour les tests). Les modules QML QtQuick.Dialogs et QtQml.Models sont également nécessaires. Qt Image Formats ajoute notamment WebP et TIFF. Le flou utilise Accelerate/vImage sur macOS et Qt Concurrent sur les autres plateformes ; chaque binaire doit être compilé avec le kit Qt de sa plateforme.

### macOS

Avec Xcode Command Line Tools et Qt installé via Homebrew :

```sh
brew install qt cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
open build/PhotoCrap.app
```

Pour la démonstration :

```sh
open build/PhotoCrap.app --args --demo
```

Le bundle de développement utilise le Qt installé. Pour réunir ses bibliothèques et modules QML dans un bundle distribuable :

```sh
cmake --install build --prefix dist
open dist/PhotoCrap.app
```

La signature Developer ID et la notarisation ne sont pas configurées.

### Linux

Installer un kit **Qt 6.8.3 Desktop gcc_64** avec l’installateur Qt, incluant Qt Declarative, Qt Quick Controls et Qt Image Formats. Sur Ubuntu, installer les outils et dépendances système :

```sh
sudo apt install build-essential cmake libgl1-mesa-dev libxkbcommon-dev libxcb-cursor0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/PhotoCrap
```

Adapter `CMAKE_PREFIX_PATH` au chemin réel du kit. Les paquets Qt peuvent également être utilisés sur une distribution fournissant Qt ≥ 6.5 et les imports QML correspondants. Attention : [Ubuntu 24.04 fournit Qt 6.4.2](https://packages.ubuntu.com/noble/qt6-base-dev), insuffisant pour ce projet ; utiliser le kit Qt indiqué ci-dessus sur cette distribution.

Une installation via `cmake --install build --prefix dist` utilise le déploiement CMake de Qt ; le résultat dépend du kit et des bibliothèques système de la distribution. Aucun AppImage universel n’est fourni.

### Windows

Installer Visual Studio 2022 (charge de travail C++), CMake et un kit **Qt 6.8.3 MSVC 2022 64 bits** avec Qt Declarative, Qt Quick Controls et Qt Image Formats. Dans PowerShell :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
.\dist\bin\PhotoCrap.exe
```

L’étape d’installation embarque les bibliothèques Qt, les plugins et les imports QML. Un kit MinGW doit être associé au compilateur MinGW correspondant, pas au générateur Visual Studio.

### Qt Creator et presets

Ouvrir `CMakeLists.txt` dans Qt Creator, sélectionner le kit Qt de la machine et lancer la cible `PhotoCrap`. Si Qt est déjà dans `CMAKE_PREFIX_PATH`, les presets sont également utilisables :

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
```

## Utilisation

1. Créer un document, ouvrir une image ou charger la démonstration.
2. Sélectionner le calque dans la liste à droite ; double-cliquer son nom pour le renommer ou faire un clic droit pour accéder aux commandes du calque et de son masque. Le bouton **◐** ajoute ou sélectionne son masque. Le bouton **ƒ** ouvre les paramètres d’un réglage ou d’une séparation de fréquences.
3. Choisir un outil à gauche et dessiner sur le canevas. Le texte se règle dans la barre sous le canevas ; chaque clic le rasterise dans le calque actif.
4. Pour un recadrage, tracer une zone puis cliquer **Recadrer**. Les réglages et les coups de pinceau respectent la sélection ; la désélectionner pour agir sur toute l’image.
5. Enregistrer en `.pcr` pour conserver les calques, puis exporter une image pour la partager.

Raccourcis : B pinceau, E gomme, V déplacement, M sélection, C recadrage, G remplissage, D dégradé, I pipette, T texte, U rectangle, O ellipse, L ligne, H navigation. `[` / `]` modifient la taille du pinceau. Les raccourcis standards de sauvegarde, ouverture, annulation et presse-papiers sont adaptés par Qt à la plateforme. Ctrl (Cmd sur Mac) + 0 adapte l’image, + 1 affiche 100 %. La molette zoome directement dans le canevas.

Tous les raccourcis se consultent et se modifient dans **Édition → Réglages… → Raccourcis clavier**. Une combinaison vide désactive une commande, les doublons sont refusés et le bouton **Valeurs par défaut** restaure le jeu initial. Les personnalisations sont conservées entre les lancements. Les raccourcis d’outils et d’édition d’image sont suspendus pendant la saisie dans un champ texte ou l’affichage d’une boîte de dialogue.

## Architecture

| Fichier | Responsabilité |
|---|---|
| `src/Document.h`, `src/Document.cpp` | Calques, pixels, sélection, composition, outils, historique, fichiers |
| `src/CanvasItem.*` | Affichage Qt Quick du composite C++ et du damier de transparence |
| `src/main.cpp`, `src/ShortcutManager.*` | Démarrage, exposition à QML, miniatures et registre persistant des raccourcis |
| `qml/Main.qml` | Fenêtre, menus, dialogues, raccourcis, canevas et panneaux |
| `qml/IconButton.qml`, `qml/PanelTitle.qml` | Composants visuels communs |
| `tests/DocumentTest.cpp`, `tests/UiSmoke.h` | Tests Qt Test du moteur et des interactions QML |
| `.github/workflows/build.yml` | Compilation, tests et installation sur trois systèmes |

Les calques sont stockés du bas vers le haut en `QImage::Format_ARGB32_Premultiplied`. Le composite et les rendus de séparation de fréquences sont mis en cache. Pendant un trait, seules les régions touchées sont recomposées ; le flou des fréquences n’est donc pas recalculé lorsqu’un masque change. Les instantanés d’historique utilisent le partage implicite de QImage ; un geste continu devient une seule opération. La limite est de 40 opérations et environ 256 Mio d’images retenues, avec au moins une annulation conservée même si elle dépasse ce budget.

Le `.pcr` est un document JSON versionné. Sa version 3 conserve les pixels, masques, paires de séparation de fréquences, réglages et propriétés des calques ; elle reste compatible en lecture avec les versions 1 et 2. L’historique et la sélection ne sont pas enregistrés. La lecture valide tous les calques avant de remplacer le document ouvert. La taille maximale du fichier est de 256 Mio ; le document est limité à 32 mégapixels, 64 calques et 512 Mio de pixels non compressés. Ces limites ne représentent pas la consommation mémoire totale : composition, filtres, historique et sérialisation utilisent aussi de la mémoire.

## Validation et limites

Les tests vérifient notamment peinture/annulation, opacité, visibilité, verrouillage, sélection, recadrage, déplacements, remplissage, filtres, transformations, fusion, sauvegarde/réouverture, export, sauvegarde en échec et rejet des fichiers corrompus. Un test démarre également le moteur QML, dessine par événements souris, vérifie la saisie de texte et la protection des modifications, puis produit un rendu hors écran sans erreurs QML. Les vérifications d’interaction sont compilées lorsque `BUILD_TESTING=ON`.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --screenshot /tmp/photocrap.png
```

Sous Linux, utiliser `./build/PhotoCrap`. Le workflow GitHub est préparé pour les trois systèmes ; sa présence ne constitue pas une validation de builds qui n’ont pas encore été exécutés.

Limites connues : un seul document à la fois ; composition raster sur CPU avec flou natif accéléré sur macOS ; opérations synchrones pouvant ralentir les très grandes images ; texte non éditable après rasterisation ; pas de PSD/PSB, RAW, CMJN, HDR, 16/32 bits, masques vectoriels, objets dynamiques, tracés Bézier, outils de réparation, textures et dynamiques de brosse, pression/inclinaison du stylet ou IA. Les algorithmes de réglage sont des implémentations sRGB simplifiées et ne reproduisent pas exactement les résultats colorimétriques de Photoshop. Le remplissage compare les couleurs exactement, sans tolérance. Les filtres directs modifient les pixels ; l’historique permet de les annuler. Le navigateur est un aperçu, sans rectangle de navigation interactif. L’espace sRGB est normalisé à l’import, sans chaîne d’épreuvage écran/impression.

## Références

- [Adobe — Ajout et modification des masques de fusion](https://helpx.adobe.com/photoshop/desktop/create-masks/layer-masks/add-layer-masks.html)
- [Adobe — Présentation des calques de réglage et de remplissage](https://helpx.adobe.com/photoshop/desktop/create-manage-layers/color-adjustment-fill-layers/adjustment-and-fill-layers-overview.html)
- [Adobe — Description des modes de fusion](https://helpx.adobe.com/photoshop/desktop/repair-retouch/adjust-light-tone/blending-mode-descriptions.html)
- [Adobe — Retouche par séparation de fréquences](https://www.adobe.com/products/photoshop/frequency-separation.html)
- [Adobe — Outil Pinceau](https://helpx.adobe.com/photoshop/using/tool-techniques/brush-tool.html)
- [Adobe — Panneau Paramètres de pinceau](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/brushes-presets/display-brush-panel-brush-options.html)
- [Intégration CMake/QML de Qt](https://doc.qt.io/qt-6/qtqml-cmake-integration.html)
- [Déploiement des applications QML](https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html)
- [Composition avec QPainter](https://doc.qt.io/qt-6/qpainter.html)
- [Installation de Qt dans GitHub Actions](https://github.com/jurplel/install-qt-action)
