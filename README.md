# PhotoCrap

**Français** | [English](README.en.md)

Éditeur d’images de bureau en **C++20, Qt 6 et QML**, conçu pour macOS, Linux et Windows. L’interface, le moteur de pixels et le format de projet sont développés indépendamment.

La version 0.1 est une première application fonctionnelle, **pas une reproduction complète de Photoshop**. Les fonctions disponibles et les limites sont détaillées ci-dessous ; les développements nécessaires à une parité plus large sont dans [ROADMAP.md](ROADMAP.md).

![Interface PhotoCrap](docs/screenshot.png)

## Fonctionnalités

- Documents RVB 8 bits sRGB, fond blanc ou transparent, dimensions personnalisées et préréglages.
- Import d’images, ouverture de projets, import centré comme calque et collage du presse-papiers.
- Pinceau rond et gomme à empreintes : taille, dureté, opacité, débit cumulatif, espacement, angle, arrondi et lissage. Le contour de pointe suit le pointeur et un aperçu de trait est affiché dans **Affichage → Paramètres de pinceau…**. Les raccourcis suivent Photoshop pour la taille (`[`/`]`), la dureté (`Maj+[`/`Maj+]`) et les couleurs avant/arrière-plan (`D`/`X`).
- Pipette, remplissage contigu, dégradé à deux couleurs, rectangle, ellipse, ligne et texte rasterisé.
- Navigation façon Photoshop : `Ctrl/⌘ + molette` zoome autour du pointeur, la molette défile verticalement, `Maj + molette` horizontalement et `Espace + glisser` déplace librement la toile.
- Sélection rectangulaire, déplacement d’un calque ou des pixels sélectionnés, effacement, copie fusionnée et recadrage.
- Calques : ajout, duplication, renommage, ordre, visibilité, verrouillage, opacité et fusion vers le bas. Les masques de fusion peuvent être créés en blanc ou en noir, peints, inversés, activés et désactivés.
- Menu contextuel des calques avec duplication, renommage, suppression, fusion, masques et visibilité. Les commandes qui ne s’appliquent pas au calque actif sont affichées désactivées.
- Les 27 modes de fusion RVB classés par familles : normal, obscurcissement, éclaircissement, contraste, comparaison et composantes TSL.
- Vingt-trois calques de réglage non destructifs : luminosité/contraste, niveaux, courbes, exposition, vibrance, teinte/saturation, balance des couleurs, noir et blanc, filtre photo, mélangeur de couches, recherche de couleurs, négatif, isohélie, seuil, transfert de dégradé, correction sélective, flou gaussien, flou directionnel, flou de boîte, netteté optimisée, passe-haut, mosaïque et ajout de bruit. Chaque réglage reçoit automatiquement un masque blanc ou un masque dérivé de la sélection active.
- Séparation de fréquences dynamique : un calque de basses fréquences flouté et un calque de hautes fréquences lié, placé en mode Lumière linéaire. Le rayon commun est recalculé pendant le déplacement du curseur ; la prévisualisation réduite et le cache évitent de recalculer inutilement le flou.
- Réglages du calque actif : luminosité, contraste, saturation, noir et blanc, négatif, sépia, flou et netteté. Les sélections limitent leur zone d’application.
- Dialogues de réglage avec prévisualisation automatique, curseurs pleine largeur adaptés au thème sombre et valeurs modifiables en temps réel.
- Redimensionnement, rotation à 90°, symétries, zoom, déplacement du canevas, règles et aperçu de navigation.
- Historique annuler/rétablir avec suivi de l’état enregistré. Protection à l’ouverture, à la création et à la fermeture d’un document modifié.
- Fenêtre **Réglages** avec sections **Raccourcis clavier** et **Interface**. Les raccourcis sont modifiables, validés et persistants ; l’interface française ou anglaise peut être appliquée à chaud et le choix est conservé.
- Enregistrement atomique des projets `.pcr` avec leurs calques. Export aplati PNG, JPEG, BMP et WebP si le module correspondant est installé. Le JPEG et le BMP compositent la transparence sur blanc.
- Une composition de démonstration éditable : menu **Fichier → Ouvrir la composition de démonstration**, ou option `--demo`.
- Une liste persistante des dix fichiers récemment ouverts ou enregistrés est disponible dans **Fichier → Ouvrir les fichiers récents** ; les fichiers disparus sont retirés automatiquement.

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

Une installation via `cmake --install build --prefix dist` utilise le déploiement CMake de Qt. Le workflow GitHub place ce résultat dans une **AppImage x86_64** avec un lanceur qui configure les bibliothèques, plugins et imports QML embarqués. L’AppImage s’exécute directement après `chmod +x`, sans installation locale de Qt.

### Windows

Installer Visual Studio 2022 (charge de travail C++), CMake et un kit **Qt 6.8.3 MSVC 2022 64 bits** avec Qt Declarative, Qt Quick Controls et Qt Image Formats. Dans PowerShell :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
.\dist\bin\PhotoCrap.exe
```

L’étape d’installation embarque les bibliothèques Qt, les plugins et les imports QML. Le workflow publie deux variantes autonomes : un installateur NSIS `.exe` avec raccourcis et désinstallation, et un ZIP portable à décompresser. Un kit MinGW doit être associé au compilateur MinGW correspondant, pas au générateur Visual Studio.

Pour chaque exécution réussie, les artefacts GitHub contiennent un DMG autonome pour macOS, une AppImage pour Linux, ainsi que l’installateur et le ZIP portable pour Windows. La CI refuse de les publier si Qt Core, le plugin de plateforme ou les modules QML Qt Quick sont absents. La signature Developer ID, la notarisation macOS et la signature Authenticode Windows nécessitent des certificats et ne sont pas encore configurées.

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

Raccourcis : B pinceau, E gomme, V déplacement, M sélection, C recadrage, G remplissage, Maj+G dégradé, I pipette, T texte, U rectangle, O ellipse, L ligne, H navigation. `[` / `]` modifient la taille du pinceau et `Maj+[` / `Maj+]` sa dureté. `D` rétablit les couleurs noir/blanc et `X` les échange. Les raccourcis standards de sauvegarde, ouverture, annulation et presse-papiers sont adaptés par Qt à la plateforme. Ctrl (Cmd sur Mac) + 0 adapte l’image et + 1 affiche 100 %. `Ctrl/⌘ + molette` zoome sous le pointeur, la molette défile verticalement, `Maj + molette` horizontalement et `Espace + glisser` déplace librement la toile.

Tous les raccourcis se consultent et se modifient dans **Édition → Réglages… → Raccourcis clavier**. Une combinaison vide désactive une commande, les doublons sont refusés et le bouton **Valeurs par défaut** restaure le jeu initial. Les personnalisations sont conservées entre les lancements. Les raccourcis d’outils et d’édition d’image sont suspendus pendant la saisie dans un champ texte ou l’affichage d’une boîte de dialogue.

L’outil **Pièce** se trouve dans le groupe de correction (`J`). En mode **Source**, entourez la zone à réparer, puis faites glisser l’intérieur de la sélection vers une texture saine. En mode **Destination**, entourez la texture saine et faites-la glisser sur la zone à réparer. L’aperçu montre la texture pendant le geste ; au relâchement, sa couleur s’adapte aux bords de la zone. **Diffusion** va de 1 (textures détaillées) à 7 (zones lisses). Une réparation crée une seule étape d’annulation. Cette première version couvre le mode Normal sur le calque de pixels actif, sans les options Contenu pris en compte, Transparent ni Motif de Photoshop.

Le **Déplacement basé sur le contenu** partage le groupe `J`. Entourez un objet, puis faites glisser la sélection. Le mode **Déplacement** reconstruit l’ancien emplacement depuis les textures voisines ; **Extension** garde aussi l’original. **Structure** (1–7) règle la recherche de motifs et le raccord de bord, **Couleur** (0–10) l’adaptation au fond ; à zéro, la couleur copiée reste intacte. **Tous les calques** prélève le composite visible et écrit uniquement sur le calque de pixels actif. Le calcul final et la recomposition se font en arrière-plan, avec une seule étape d’annulation. Cette version utilise une recherche locale de texture et un raccord de couleur : les fonds complexes peuvent demander une retouche complémentaire. La transformation au dépôt n’est pas encore implémentée.

## Architecture

| Fichier | Responsabilité |
|---|---|
| `src/Document.h`, `src/Document.cpp` | Calques, pixels, sélection, composition, outils, historique, fichiers |
| `src/CanvasItem.*` | Affichage Qt Quick du composite C++ et du damier de transparence |
| `src/main.cpp`, `src/ShortcutManager.*`, `src/LanguageManager.*`, `src/RecentFilesManager.*` | Démarrage, exposition à QML, miniatures, langues, fichiers récents et registre persistant des raccourcis |
| `qml/Main.qml` | Fenêtre, menus, dialogues, raccourcis, canevas et panneaux |
| `qml/IconButton.qml`, `qml/PanelTitle.qml` | Composants visuels communs |
| `tests/DocumentTest.cpp`, `tests/UiSmoke.h` | Tests Qt Test du moteur et des interactions QML |
| `.github/workflows/build.yml` | Compilation, tests et installation sur trois systèmes |

Les calques sont stockés du bas vers le haut en `QImage::Format_ARGB32_Premultiplied`. Le composite et les rendus de séparation de fréquences sont mis en cache. Pendant un trait, seules les régions touchées sont recomposées ; le flou des fréquences n’est donc pas recalculé lorsqu’un masque change. Les instantanés d’historique utilisent le partage implicite de QImage ; un geste continu devient une seule opération. La limite est de 40 opérations et environ 256 Mio d’images retenues, avec au moins une annulation conservée même si elle dépasse ce budget.

Le `.pcr` est un document JSON versionné. Sa version 3 conserve les pixels, masques, paires de séparation de fréquences, réglages et propriétés des calques ; elle reste compatible en lecture avec les versions 1 et 2. L’historique et la sélection ne sont pas enregistrés. La lecture valide tous les calques avant de remplacer le document ouvert. La taille maximale du fichier est de 256 Mio ; le document est limité à 32 mégapixels, 64 calques et 512 Mio de pixels non compressés. Ces limites ne représentent pas la consommation mémoire totale : composition, filtres, historique et sérialisation utilisent aussi de la mémoire.

### Suppression

Dans le groupe de correction (J), Suppression permet de peindre sur un objet ou de l’entourer avec un contour presque fermé ; un tracé entre deux bords inclut aussi le coin correspondant. Une surbrillance rose montre les zones marquées ; Alt ou « Retirer des marques » soustrait une zone. « Après chaque tracé » lance la reconstruction au relâchement ; désactivé, plusieurs tracés peuvent être regroupés puis appliqués avec « Appliquer » ou Retour. Échap/Annuler abandonne les marques et le calcul en cours. Une sélection limite les zones marquées ; chaque application produit une seule étape d’annulation.

Le calcul local s’effectue en arrière-plan, sur les zones marquées et leur voisinage ; une nouvelle marque conserve les précédentes et remplace le résultat en attente. « Tous les calques » prélève les calques visibles et écrit uniquement dans le calque de pixels actif, ce qui permet une retouche sur un calque vide. Les masques et les calques verrouillés ne sont pas modifiés. La reconstruction copie et adapte des textures voisines, avec propagation de bordure en secours ; elle ne reproduit pas l’IA générative ni la détection automatique des distractions de Photoshop. Les arrière-plans complexes peuvent demander des retouches.

Référence : [outil Suppression d’Adobe](https://helpx.adobe.com/photoshop/using/tool-techniques/remove-tool.html).

### Forme d’historique

La **Forme d’historique** (`Y`) restaure les pixels d’un état source aux mêmes coordonnées sur le même calque, sans revenir globalement en arrière. Choisissez la source en cliquant sur l’icône à gauche d’un état dans **Historique**, ou dans la liste Source de la barre de l’outil ; ce choix ne modifie ni les pixels ni l’historique. Peignez ensuite avec taille, dureté, opacité, débit et mode de fusion. Une sélection limite le geste et chaque tracé crée une seule étape d’annulation. La transparence de l’état source est également restaurée.

La source initiale est l’état d’ouverture/création du document. On peut choisir un état futur après Annuler, puis en restaurer certaines zones ; la source choisie reste disponible si cette branche disparaît ou si les anciens états sont purgés. Son stockage partagé est compté dans le budget de l’historique. Le calque est retrouvé par son identité même après renommage ou réorganisation ; un nouveau calque ou une copie ne peut être restauré depuis un état où il n’existait pas. La source doit avoir les mêmes dimensions que le document. Cette version agit sur les calques de pixels, pas sur les masques.

Référence : [Forme d’historique d’Adobe](https://helpx.adobe.com/photoshop/using/tool-techniques/history-brush-tool.html).

### Forme d’historique artistique

La **Forme d’historique artistique** partage le groupe `Y` et la source de la Forme d’historique. Pour un essai simple sur une photo, cliquez sur **État actuel**, laissez **Tolérance** à zéro, puis peignez : les couleurs de la source deviennent des touches orientées selon ses détails. Dix styles proposent des traits serrés ou relâchés, courts ou longs, des touches et des boucles. **Zone** règle la dispersion et le nombre de touches ; une tolérance élevée réserve la peinture aux pixels différents de la source. La brosse conserve ses réglages de taille, dureté, forme, opacité, débit et fusion. Le calcul et la recomposition portent uniquement sur les rectangles touchés, avec sélection, verrouillage, annulation et annulation du geste.

Les contrôles et le principe suivent [la référence Adobe](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/create-fill-with-patterns/paint-stylized-strokes-with-the-art-history-brush.html). Le moteur de touches est propre à PhotoCrap, sans prétendre reproduire pixel pour pixel l’algorithme de Photoshop. Comme la Forme d’historique, cet outil nécessite un calque de pixels présent dans la source et des dimensions compatibles ; il ne peint pas sur les masques.

### Pipette et échantillonnage de couleur

Les outils du groupe `I` ont des icônes vectorielles et des curseurs natifs : la pointe de la pipette est le pixel prélevé, l’échantillonneur a un repère supplémentaire et Option/Alt sur un repère affiche des ciseaux. Les pipettes de Courbes et de balance des blancs utilisent le même curseur, sans dessin retardé dans le canevas ; les pipettes noir, gris et blanc gardent leur témoin distinctif.

La barre de **Pipette** et d’**Échantillonnage de couleur** propose Échantillon ponctuel et les moyennes 3 × 3, 5 × 5, 11 × 11, 31 × 31, 51 × 51 et 101 × 101. Les sources actuellement disponibles sont Tous les calques et Calque actif (ou son masque si l’on y travaille). Cliquer ou glisser avec la Pipette choisit le premier plan ; Option/Alt choisit l’arrière-plan et Maj place un échantillon sans changer la couleur. L’anneau optionnel compare la nouvelle couleur et celle d’avant le geste.

Les quatre repères numérotés restent sur l’image, tandis que leurs valeurs RVB figurent dans le panneau Informations. On peut les déplacer, les retirer avec Option/Alt ou en les tirant hors de l’image, et les supprimer avec Tout effacer. Un cinquième clic ne remplace pas silencieusement le premier. Les valeurs suivent les rendus et les tailles d’échantillon, sans historique de pixels. Le prélèvement utilise la composition complète terminée pour la précision ; pendant un calcul en arrière-plan, il utilise le dernier aperçu terminé sans lancer de nouveau rendu complet. Il ignore les visualisations de couches et les surimpressions de sélection. La Règle a ses lectures X, Y, largeur, hauteur, angle et D1 dans sa barre dédiée et dans Informations ; Maj contraint à 45°.

Références Adobe : [Pipette et couleurs de premier/arrière-plan](https://helpx.adobe.com/ca/photoshop/desktop/adjust-color/choose-colors/set-foreground-and-background-colors.html), [échantillonnage et valeurs de pixels](https://helpx.adobe.com/photoshop/desktop/adjust-color/selective-color-adjustments/view-pixel-color-values-in-an-image.html), [outil Règle](https://helpx.adobe.com/photoshop/using/positioning-ruler-tool.html).

### Panneau Informations

Le panneau Informations affiche aussi les coordonnées et les valeurs RVB sous le pointeur, les dimensions de la sélection et du document. Il est masqué au départ et s’ouvre automatiquement avec la pipette, l’échantillonneur, la règle ou les pipettes des réglages. Par défaut, c’est une fenêtre flottante ; une fois ancré ou regroupé avec un autre panneau, il conserve cette organisation. Affichage → Panneaux → Informations permet de le masquer ou de le rouvrir ; fermer sa fenêtre le masque sans modifier son choix d’ancrage. Son placement et sa taille sont sauvegardés avec l’espace de travail.

Référence : [Panneau Informations Adobe](https://helpx.adobe.com/photoshop/desktop/get-started/settings-and-preferences/use-the-info-panel-and-document-status-bar.html).

Le curseur des outils est synchronisé immédiatement, puis après la stabilisation des fenêtres et du focus. Un test Cocoa contrôle la pointe réelle de la pipette, de l’échantillonneur et de la règle dès leur première activation, sans nouveau mouvement de souris, ainsi qu’après la réouverture d’Informations.

## Validation et limites

Les tests vérifient notamment peinture/annulation, opacité, visibilité, verrouillage, sélection, recadrage, déplacements, remplissage, filtres, transformations, fusion, sauvegarde/réouverture, export, sauvegarde en échec et rejet des fichiers corrompus. Un test démarre également le moteur QML, dessine par événements souris, vérifie la saisie de texte et la protection des modifications, puis produit un rendu hors écran sans erreurs QML. Les vérifications d’interaction sont compilées lorsque `BUILD_TESTING=ON`.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --screenshot /tmp/photocrap.png
```

Sous Linux, utiliser `./build/PhotoCrap`. Le workflow GitHub est préparé pour les trois systèmes ; sa présence ne constitue pas une validation de builds qui n’ont pas encore été exécutés.

Limites connues : un seul document à la fois ; composition raster sur CPU avec flou natif accéléré sur macOS ; opérations synchrones pouvant ralentir les très grandes images ; texte non éditable après rasterisation ; pas de PSD/PSB, RAW, CMJN, HDR, 16/32 bits, masques vectoriels, objets dynamiques, tracés Bézier, réparation générative, textures et dynamiques de brosse, pression/inclinaison du stylet ou IA. Les algorithmes de réglage sont des implémentations sRGB simplifiées et ne reproduisent pas exactement les résultats colorimétriques de Photoshop. Le remplissage compare les couleurs exactement, sans tolérance. Les filtres directs modifient les pixels ; l’historique permet de les annuler. Le navigateur est un aperçu, sans rectangle de navigation interactif. L’espace sRGB est normalisé à l’import, sans chaîne d’épreuvage écran/impression.

## Références techniques

- [Intégration CMake/QML de Qt](https://doc.qt.io/qt-6/qtqml-cmake-integration.html)
- [Déploiement des applications QML](https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html)
- [Composition avec QPainter](https://doc.qt.io/qt-6/qpainter.html)
- [Installation de Qt dans GitHub Actions](https://github.com/jurplel/install-qt-action)
