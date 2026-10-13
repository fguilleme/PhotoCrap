# PhotoCrap

**Français** | [English](README.en.md)

Éditeur d’images de bureau en **C++20, Qt 6 et QML**, conçu pour macOS, Linux et Windows. L’interface, le moteur de pixels et le format de projet sont développés indépendamment.

La version **0.11.0** est une application raster fonctionnelle, **pas une reproduction complète de Photoshop**. Les fonctions disponibles et les limites sont détaillées ci-dessous ; les développements nécessaires à une parité plus large sont dans [ROADMAP.md](ROADMAP.md).

## Télécharger

La [release 0.11.0](https://github.com/fguilleme/PhotoCrap/releases/tag/v0.11.0) fournit un **DMG macOS arm64**, une **AppImage Linux x86_64**, un **installateur Windows x64** et un **ZIP Windows portable**. Chaque paquet est accompagné d’un fichier `.sha256`. Les paquets embarquent Qt ; une installation de Qt est nécessaire seulement pour compiler le projet.

État vérifié le **12 octobre 2026** : le [workflow du tag](https://github.com/fguilleme/PhotoCrap/actions/runs/38168128564) a réussi les builds, tests, déploiements et la publication sur les trois plateformes. Le DMG a passé le contrôle de signature ad hoc ; les sommes SHA-256 des quatre paquets téléchargés ont été vérifiées. Un [workflow de branche](https://github.com/fguilleme/PhotoCrap/actions/runs/38168129164) échoue toutefois au test du curseur natif macOS, même après relance. Ce défaut reste ouvert ; la réussite de la publication ne signifie pas que tous les contrôles sont verts.

![Composition de démonstration PhotoCrap](docs/screenshot.png)

## Fonctionnalités

- Documents RVB 8 ou 16 bits par canal sRGB, fond blanc ou transparent, dimensions personnalisées et préréglages.
- Import d’images, ouverture de projets, import centré comme calque et collage du presse-papiers.
- Pinceau rond et gomme à empreintes : taille, dureté, opacité, débit cumulatif, espacement, angle, arrondi et lissage. Le contour de pointe suit le pointeur et un aperçu de trait est affiché dans **Affichage → Pinceau…**. Les raccourcis suivent Photoshop pour la taille (`[`/`]`), la dureté (`Maj+[`/`Maj+]`) et les couleurs avant/arrière-plan (`D`/`X`).
- Barre d’outils organisée par groupes : clic droit pour choisir un outil, icône du dernier outil choisi et infobulles. Ordre des groupes, ordre des outils et visibilité personnalisables dans **Réglages → Barre d’outils** ; les outils non implémentés restent visibles, désactivés et marqués **NYI**.
- Gomme magique dans le groupe **E** : clic droit pour la choisir, puis clic sur une couleur pour l’effacer du calque actif. Tolérance, lissage, pixels contigus, échantillonnage de tous les calques et opacité dans la barre supérieure. Calcul en arrière-plan, **Échap** pour annuler, sélection respectée et précision 8/16 bits conservée. Elle agit sur les pixels, pas sur les masques.
- Peinture et retouche : pinceau, crayon, remplacement de couleur, pinceau mélangeur, gomme, tampon de duplication, tampon de motif, flou, netteté, doigt, densité −/+, éponge, correction des yeux rouges, correcteur, correcteur localisé, pièce, déplacement basé sur le contenu, suppression et formes d’historique.
- Pipette avec prélèvement ponctuel ou moyen, quatre échantillons de couleur, règle, remplissage contigu et dégradé à deux couleurs. Formes raster : rectangle, ellipse, triangle, polygone, étoile et ligne, avec paramètres de remplissage et de contour. Texte horizontal rasterisé.
- Navigation façon Photoshop : `Ctrl/⌘ + molette` zoome autour du pointeur, la molette défile verticalement, `Maj + molette` horizontalement et `Espace + glisser` déplace librement la toile.
- Sélections rectangulaire, elliptique, rangée/colonne unique, lasso, lasso polygonal, sélection rapide et baguette magique. Contours animés, modes remplacer/ajouter/soustraire/intersection, déplacement des pixels sélectionnés, effacement, copie fusionnée et recadrage.
- Calques de pixels, remplissage Couleur unie, réglages, groupes et écrêtage : ajout, duplication, renommage, ordre, visibilité, verrouillage, opacité et fusion vers le bas. Masques raster blancs/noirs, peinture, inversion, activation/désactivation et copie d’un masque vers un autre calque par glisser. Alt-clic sur l’œil isole un calque ; Alt-clic sur une vignette de masque affiche le masque seul.
- Menus contextuels distincts pour les calques et les vignettes de masque : propriétés, verrouillage, duplication, regroupement et fusion vers le bas côté calque ; affichage, activation, inversion et suppression côté masque. Les commandes indisponibles sont désactivées, les fonctions manquantes marquées **NYI**.
- Les 27 modes de fusion RVB classés par familles, avec aperçu au survol du menu : le clic valide, quitter le menu restaure l’état précédent.
- Calques de réglage non destructifs : **Lumière** (exposition, contraste, hautes lumières, ombres, blancs, noirs), **Couleur** (température, teinte, vibrance, saturation et pipette de balance des blancs), Niveaux, Courbes, Balance des couleurs, Noir et blanc, Filtre photo, Mélangeur de couches, Style colorimétrique (LUT), Négatif, Isohélie, Seuil, Transfert de dégradé et Correction sélective. Flou gaussien, flou directionnel, flou de boîte, netteté optimisée, passe-haut, mosaïque et bruit sont aussi disponibles comme calques de filtre. Les anciens paramètres de projets restent lisibles ; chaque réglage reçoit un masque blanc ou dérivé de la sélection.
- Éditeurs spécialisés : Courbes avec histogramme, points de contrôle et quatre pipettes ; Niveaux avec repères d’entrée/sortie ; Noir et blanc avec six familles de couleurs, presets, Auto et Teinte ; Transfert de dégradé avec roue à deux pointeurs ; Filtre photo et Mélangeur de couches avec préréglages. Barre commune d’écrêtage, visibilité, réinitialisation et suppression.
- Séparation de fréquences dynamique : un calque de basses fréquences flouté et un calque de hautes fréquences lié, placé en mode Lumière linéaire. Le rayon commun est recalculé pendant le déplacement du curseur ; la prévisualisation réduite et le cache évitent de recalculer inutilement le flou. Le bouton **Préparer la retouche** calcule ensuite les deux calques en arrière-plan et les convertit en pixels éditables, sans toucher à l’original : pinceau/flou sur Basses fréquences, tampon avec échantillonnage du calque actif sur Hautes fréquences. Masques, opacités, modes de fusion et profondeur 8/16 bits sont conservés. Les sources sont conservées dans le budget du document. **Revenir au réglage du rayon** restaure la paire dynamique, après confirmation de l’abandon des retouches ; cette opération est elle-même annulable. Le bouton **ƒ** reste disponible sur les deux calques pour retrouver le panneau et les conseils de retouche, y compris après réouverture du projet.
- Réglages du calque actif : luminosité, contraste, saturation, noir et blanc, négatif, sépia, flou et netteté. Les sélections limitent leur zone d’application.
- Aperçus de réglage calculés en arrière-plan sur des proxies réduits, demandes intermédiaires remplacées par la plus récente et rendu complet asynchrone qui actualise le cache écran. Annuler restaure une session de propriétés ; Fermer conserve les changements. Dans un champ numérique, le premier Retour valide et sort de l’édition ; le suivant active le bouton par défaut.
- Redimensionnement, rotation à 90°, symétries, zoom, rotation de l’affichage, déplacement du canevas, règles et aperçu de navigation.
- Panneaux **Histogramme, Navigateur, Couleur, Calques, Couches, Historique et Informations** : déplacement, regroupement en onglets, colonnes, repli, redimensionnement, fenêtres flottantes et organisation persistante. **Affichage → Panneaux** contrôle leur visibilité. Couches affiche RVB ou un canal en niveaux de gris sans modifier le document.
- Historique annuler/rétablir avec suivi de l’état enregistré. Protection à l’ouverture, à la création et à la fermeture d’un document modifié.
- Fenêtre **Réglages** avec sections **Raccourcis clavier**, **Interface** et **Barre d’outils**. Les raccourcis sont modifiables, validés et persistants ; l’interface française ou anglaise peut être appliquée à chaud et le choix est conservé.
- Enregistrement atomique des projets `.pcr` avec leurs calques. Export aplati PNG, JPEG, BMP et WebP si le module correspondant est installé. Le JPEG et le BMP compositent la transparence sur blanc.
- Une composition de démonstration éditable : menu **Fichier → Ouvrir la composition de démonstration**, ou option `--demo`.
- Une liste persistante des dix fichiers récemment ouverts ou enregistrés est disponible dans **Fichier → Ouvrir les fichiers récents** ; les fichiers disparus sont retirés automatiquement.

## RAW et précision des couleurs

Les fichiers RAW (DNG, CR2/CR3, NEF, ARW, RAF, ORF, RW2 et autres extensions reconnues) sont développés par LibRaw 0.22.2 en arrière-plan, avec balance des blancs du boîtier, orientation et sortie sRGB 16 bits. L’ouverture et l’import comme calque utilisent les données du capteur ; un chargement peut être annulé. La compatibilité réelle dépend du boîtier et de son encodage. Le test automatisé développe un DNG Bayer synthétique, sans vignette JPEG. Les DNG compressés en JPEG avec pertes ou JPEG XL ne sont pas pris en charge par cette configuration.

La profondeur se choisit à la création ou dans **Image → Profondeur des couleurs**. Les réglages photo, la composition, les masques, les filtres spatiaux, le pinceau, la gomme, le tampon, les formes et le recadrage conservent le format 16 bits. Le projet `.pcr` et l’export PNG conservent cette précision ; JPEG, BMP et WebP sont des sorties 8 bits. Passer de 8 à 16 bits ne recrée pas les détails perdus.

Doigt, Pinceau mélangeur, Correcteur, Correcteur localisé, Suppression, Pièce, Déplacement basé sur le contenu, Œil rouge, Remplacement de couleur et les deux formes d’historique restent réservés aux pixels 8 bits ; leur utilisation sur les pixels 16 bits est bloquée explicitement, sans conversion silencieuse. Les limites sont de 32 mégapixels et 2 Gio de données de calques, avec 8 octets par pixel couleur 16 bits et 1 octet par pixel de masque (lignes alignées sur 4 octets).

CMake télécharge les sources LibRaw et zlib épinglées et vérifie leurs SHA-256. Ces bibliothèques sont liées statiquement. Les paquets incluent les sources et licences inchangées dans `third-party` (LibRaw distribué sous CDDL 1.0).

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

### Outils et raccourcis

Un bouton représente un groupe d’outils. Son **clic droit** affiche les variantes ; son icône et sa touche rappellent le dernier outil choisi. Les touches actuellement branchées par défaut sont :

| Touche | Groupe / outils disponibles |
|---|---|
| V | Déplacement |
| M | Rectangle, ellipse, rangée unique, colonne unique de sélection |
| L | Lasso, lasso polygonal |
| W | Sélection rapide, baguette magique |
| C | Recadrage, perspective, tranches |
| I | Pipette, échantillonnage de couleur, règle |
| J | Correcteur localisé, suppression, correcteur, pièce, déplacement basé sur le contenu, œil rouge |
| B | Pinceau, crayon, remplacement de couleur, pinceau mélangeur |
| S | Tampon de duplication, tampon de motif |
| Y | Forme d’historique, forme d’historique artistique (branche de développement) |
| E | Gomme |
| G | Dégradé, remplissage contigu |
| O | Densité −, densité +, éponge |
| T | Texte horizontal rasterisé |
| U | Rectangle, ellipse, triangle, polygone, étoile, ligne |
| H / R / Z | Main / rotation de l’affichage / zoom |

Sur la branche de développement, **Y** active les formes d’historique. **Maj + touche** et **Option/Alt-clic** parcourent les variantes ; maintenir une lettre utilise temporairement son outil. Les préférences permettent de modifier ces comportements. Flou, Netteté et Doigt partagent un groupe sans lettre par défaut. Les entrées **NYI** restent désactivées. Voir le [guide souris et clavier](docs/TOOL_INTERACTIONS.md), avec les références Adobe et les limites actuelles.

`[` / `]` règlent la taille du pinceau, `Maj+[` / `Maj+]` sa dureté, `D` rétablit noir/blanc et `X` échange les couleurs. `Ctrl/⌘ + 0` adapte l’image ; `Ctrl/⌘ + 1` affiche 100 %. `Ctrl/⌘ + molette` zoome sous le pointeur, la molette défile verticalement, `Maj + molette` horizontalement et `Espace + glisser` déplace la toile. Annuler/Rétablir conserve le zoom de navigation.

Le catalogue contient **275 entrées**, avec recherche par action, touche ou contexte et filtres **Disponibles**, **NYI** et **Gestes contextuels**. Les fonctions manquantes restent des indications désactivées ; elles ne capturent aucune touche. Les gestes à la souris sont consultables sans devenir des raccourcis globaux. L’[inventaire détaillé](docs/SHORTCUTS.md) sert aussi de liste des fonctionnalités à venir.

Les raccourcis et la barre d’outils se personnalisent dans **Réglages** : **menu PhotoCrap → Préférences…** ou **⌘,** sur macOS, **Édition → Réglages…** sur les autres plateformes. Les raccourcis sont suspendus pendant une saisie ou une boîte de dialogue modale ; une combinaison vide désactive une commande, les doublons sont refusés et les personnalisations sont conservées entre les lancements.


L’outil **Pièce** se trouve dans le groupe de correction (`J`). En mode **Source**, entourez la zone à réparer, puis faites glisser l’intérieur de la sélection vers une texture saine. En mode **Destination**, entourez la texture saine et faites-la glisser sur la zone à réparer. L’aperçu montre la texture pendant le geste ; au relâchement, sa couleur s’adapte aux bords de la zone. **Diffusion** va de 1 (textures détaillées) à 7 (zones lisses). Une réparation crée une seule étape d’annulation. Cette première version couvre le mode Normal sur le calque de pixels actif, sans les options Contenu pris en compte, Transparent ni Motif de Photoshop.

Le **Déplacement basé sur le contenu** partage le groupe `J`. Entourez un objet, puis faites glisser la sélection. Le mode **Déplacement** reconstruit l’ancien emplacement depuis les textures voisines ; **Extension** garde aussi l’original. **Structure** (1–7) règle la recherche de motifs et le raccord de bord, **Couleur** (0–10) l’adaptation au fond ; à zéro, la couleur copiée reste intacte. **Tous les calques** prélève le composite visible et écrit uniquement sur le calque de pixels actif. Le calcul final et la recomposition se font en arrière-plan, avec une seule étape d’annulation. Cette version utilise une recherche locale de texture et un raccord de couleur : les fonds complexes peuvent demander une retouche complémentaire. La transformation au dépôt n’est pas encore implémentée.

### Recadrage et tranches (branche de développement)

Le groupe **C** propose désormais Recadrage, Recadrage en perspective, Tranche et Sélection de tranche, accessibles par clic droit sur son icône. Ces ajouts sont postérieurs à la release 0.11.0.

- **Recadrage** : tracez un cadre, déplacez-le ou ajustez ses huit poignées ; choisissez un ratio, inversez son orientation ou saisissez les dimensions de sortie en pixels. La grille et l’assombrissement extérieur aident au cadrage. Entrée applique, Échap abandonne.
- **Perspective** : tracez un cadre puis ajustez ses quatre coins sur les bords du sujet. Entrée redresse le quadrilatère et recadre tous les calques, masques et sources en arrière-plan. Annuler restaure l’image d’origine.
- **Tranches** : tracez des rectangles avec Tranche ; Sélection de tranche permet de les déplacer, les redimensionner, les supprimer ou les diviser en rangées/colonnes. Les tranches utilisateur sont sauvegardées dans le projet et exportables en PNG ; les fichiers existants ne sont pas écrasés. Les tranches automatiques sont des repères visuels, pas des zones modifiables ni exportées.

Le recadrage supprime les pixels hors cadre, récupérables par Annuler. La conservation non destructive hors canevas, la rotation/redressement par ligne, les presets de résolution et l’export HTML des tranches restent à implémenter. Un changement de dimensions efface les repères de tranches ; Annuler les restaure.

## Architecture

| Fichier | Responsabilité |
|---|---|
| `src/Document.h`, `src/Document.cpp` | Modèle du document, état commun, historique et histogrammes |
| `src/DocumentLayers.cpp`, `src/DocumentEditing.cpp`, `src/DocumentIO.cpp` | Calques/masques, édition/transformation et lecture/écriture des projets |
| `src/DocumentCompositor.cpp`, `src/DocumentPreview.cpp` | Composition, caches de proxies, rendus complets et ordonnanceur asynchrone |
| `src/DocumentAdjustments.cpp`, `src/DocumentFilters.cpp`, `src/filters/*` | Gestion des réglages et modules de calcul spécialisés |
| `src/tools/*` | Contrôleur de gestes, paramètres et implémentations d’outils séparées par famille |
| `src/CanvasItem.*` | Affichage Qt Quick, damier, contours de sélection et curseurs natifs |
| `src/main.cpp`, `src/ShortcutManager.*`, `src/LanguageManager.*`, `src/RecentFilesManager.*`, `src/WorkspaceSettings.*` | Démarrage, exposition QML, langues, fichiers récents, raccourcis et espace de travail persistant |
| `qml/Main.qml`, `qml/*Editor.qml`, `qml/*Options.qml` | Fenêtre, menus, dialogues, éditeurs de réglage et barres d’options spécialisées |
| `qml/DockablePanel.qml`, `qml/InformationPanel.qml` | Conteneur commun des panneaux et affichage des mesures |
| `tests/DocumentTest.cpp`, `tests/UiSmoke.h`, `tests/Mac*Test.*` | Tests du moteur, des interactions QML et des menus/curseurs natifs macOS |
| `.github/workflows/build.yml`, `cmake/*`, `packaging/*` | Builds, tests, déploiement, paquets, contrôles de signature et publication des releases |

Les calques sont stockés du bas vers le haut en `QImage::Format_ARGB32_Premultiplied` (8 bits par canal) ou `QImage::Format_RGBA64_Premultiplied` (16 bits par canal). Les masques restent en 8 bits ; les proxies écran peuvent être réduits en 8 bits sans changer les pixels de travail. Le composite et les rendus de séparation de fréquences sont mis en cache. Pendant un trait, seules les régions touchées sont recomposées ; le flou des fréquences n’est donc pas recalculé lorsqu’un masque change. Les instantanés d’historique utilisent le partage implicite de QImage ; un geste continu devient une seule opération. La limite est de 40 opérations et environ 256 Mio d’images supplémentaires retenues par rapport au document courant, avec au moins une annulation conservée même si elle dépasse ce budget.

Le `.pcr` est un document JSON versionné. Sa version 7 conserve la profondeur des couleurs et les pixels, masques, paires de séparation de fréquences, réglages et propriétés des calques ; elle conserve aussi les groupes et l’écrêtage et reste compatible en lecture avec les versions 1 à 6. Les calques vides et les masques uniformes sont stockés sous forme compacte dans cette version et ne sont développés qu’à la première modification locale. L’historique, la sélection et les repères d’échantillonnage ne sont pas enregistrés dans le projet ; l’organisation des panneaux est conservée séparément dans les réglages de l’application. La lecture valide tous les calques avant de remplacer le document ouvert. La taille maximale du fichier est de 256 Mio ; le document est limité à 32 mégapixels, 512 calques et 2 Gio de données non compressées de calques. Le budget compte séparément les pixels couleur, les sources de séparation de fréquences et les masques 8 bits ; un réglage ne réserve que son masque et un dossier ne réserve aucune image. Les images non uniformes partagées sont réservées séparément pour chaque calque afin de permettre leur modification et leur rechargement. Les calques vides et masques uniformes ne réservent pas de plan complet ; peindre dessus vérifie le budget avant de développer leurs pixels. Le cache de fréquences utilise une éviction LRU bornée à 128 Mio, et ses miniatures sont calculées à leur taille d’affichage. Ces limites ne représentent pas la consommation mémoire totale : composition, filtres, historique et sérialisation utilisent aussi de la mémoire.

### Suppression

Dans le groupe de correction (J), Suppression permet de peindre sur un objet ou de l’entourer avec un contour presque fermé ; un tracé entre deux bords inclut aussi le coin correspondant. Une surbrillance rose montre les zones marquées ; Alt ou « Retirer des marques » soustrait une zone. « Après chaque tracé » lance la reconstruction au relâchement ; désactivé, plusieurs tracés peuvent être regroupés puis appliqués avec « Appliquer » ou Retour. Échap/Annuler abandonne les marques et le calcul en cours. Une sélection limite les zones marquées ; chaque application produit une seule étape d’annulation.

Le calcul local s’effectue en arrière-plan, sur les zones marquées et leur voisinage ; une nouvelle marque conserve les précédentes et remplace le résultat en attente. « Tous les calques » prélève les calques visibles et écrit uniquement dans le calque de pixels actif, ce qui permet une retouche sur un calque vide. Les masques et les calques verrouillés ne sont pas modifiés. La reconstruction copie et adapte des textures voisines, avec propagation de bordure en secours ; elle ne reproduit pas l’IA générative ni la détection automatique des distractions de Photoshop. Les arrière-plans complexes peuvent demander des retouches.

Référence : [outil Suppression d’Adobe](https://helpx.adobe.com/photoshop/using/tool-techniques/remove-tool.html).

### Forme d’historique

La **Forme d’historique** (groupe affiché `Y`) restaure les pixels d’un état source aux mêmes coordonnées sur le même calque, sans revenir globalement en arrière. Choisissez la source en cliquant sur l’icône à gauche d’un état dans **Historique**, ou dans la liste Source de la barre de l’outil ; ce choix ne modifie ni les pixels ni l’historique. Peignez ensuite avec taille, dureté, opacité, débit et mode de fusion. Une sélection limite le geste et chaque tracé crée une seule étape d’annulation. La transparence de l’état source est également restaurée.

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

Sous Linux, utiliser `./build/PhotoCrap`. Les tests natifs macOS utilisent le backend Cocoa, séparément du smoke test hors écran. Le statut de publication et l’échec de curseur restant sont détaillés dans [Télécharger](#télécharger) et dans la [roadmap](ROADMAP.md).

Limites connues : un seul document à la fois ; composition raster sur CPU avec flou natif accéléré sur macOS ; certains chemins d’édition restent synchrones et peuvent ralentir les très grandes images ; texte non éditable après rasterisation ; pas de PSD/PSB, CMJN, HDR, 32 bits, masques vectoriels, objets dynamiques, tracés Bézier, réparation générative, textures et dynamiques de brosse, pression/inclinaison du stylet ou IA. Les algorithmes de réglage sont des implémentations sRGB simplifiées et ne reproduisent pas exactement les résultats colorimétriques de Photoshop. Le remplissage compare les couleurs exactement, sans tolérance. Les filtres directs modifient les pixels ; l’historique permet de les annuler. Le navigateur est un aperçu, sans rectangle de navigation interactif. L’espace sRGB est normalisé à l’import, sans chaîne d’épreuvage écran/impression.

## Références techniques

- [Intégration CMake/QML de Qt](https://doc.qt.io/qt-6/qtqml-cmake-integration.html)
- [Déploiement des applications QML](https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html)
- [Composition avec QPainter](https://doc.qt.io/qt-6/qpainter.html)
- [Installation de Qt dans GitHub Actions](https://github.com/jurplel/install-qt-action)
