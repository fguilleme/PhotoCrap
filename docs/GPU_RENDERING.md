# Canevas et accélération graphique

Étapes après **v0.15**, en développement : présentation du composite dans le
canevas, puis aperçus Courbes, Niveaux, Lumière, Couleur et Balance des couleurs
sur GPU. Les pixels de travail
et le rendu final restent calculés sur CPU.

## Ce qui change

`CanvasItem` utilise maintenant `QQuickItem` et le graphe de scène Qt Quick,
à la place d’une surface `QQuickPaintedItem` contenant toute la photo.
Les nœuds et textures utilisent les API publiques Qt, sans dépendance à `GuiPrivate`.
Qt choisit son backend : Metal, OpenGL, Vulkan ou Direct3D selon la plateforme,
la configuration et la disponibilité. Ce changement ne force pas Vulkan sous Linux.

- Le composite terminé est consulté temporairement sur le thread GUI, qui prépare
  des copies indépendantes des blocs visibles. Le canevas ne garde pas de référence
  au composite complet qui forcerait une copie pleine image à la prochaine retouche.
  Les accès au document et la préparation des surimpressions restent sur le thread GUI ;
  le thread du graphe de scène ne consulte que ces instantanés.
- Un niveau de détail d’affichage est réduit sur CPU et conservé. Il suit la résolution
  physique de l’écran par paliers de puissance de deux, pour éviter de transférer tous
  les pixels d’origine à faible zoom. Ce n’est pas encore une pyramide calculée sur GPU.
- Le niveau est découpé en blocs de **1 024 × 1 024**, avec un pixel voisin de bordure
  pour l’interpolation. Seuls les blocs intersectant la partie visible sont chargés ;
  ceux qui sortent de la vue sont libérés. Il n’y a pas de texture unique de la taille
  d’un grand document.
- Les textures sont réutilisées tant que l’image, le niveau et les blocs visibles
  restent identiques. Changer le zoom dans le même niveau ne fait que modifier la
  géométrie des nœuds ; déplacer la vue réutilise les blocs encore présents.
- Le damier de transparence utilise une texture répétée de 24 × 24 sur les backends
  RHI. Le rendu logiciel garde un fond limité à la zone visible.
- Les contours des sélections générales, le marquage de suppression et l’aperçu de
  Pièce sont séparés de la photo. Leur surface est limitée à la portion visible,
  y compris après rotation de la vue. Le tracé d’une sélection générale est mis en
  cache ; animer ses fourmis ne recrée pas les textures de la photo.
- Les ressources graphiques sont créées et détruites sur le thread de rendu, avec
  reconstruction après suppression du nœud ou changement de fenêtre.

Les curseurs natifs et les événements clavier/souris restent sur les mêmes objets
QML. Les rectangles, ellipses et lassos conservent leurs surimpressions QML existantes.

## Précision et limites

L’affichage utilise des textures **8 bits par canal**, comme la surface d’affichage
précédente. Les pixels de travail, projets et exports conservent leurs **8 ou 16 bits**.
Ce n’est ni un affichage HDR, ni une nouvelle gestion ICC.

La composition des calques, les masques, les traits et les filtres hors aperçu
Courbes, Niveaux, Lumière, Couleur et Balance des couleurs restent calculés sur CPU. Les surimpressions générales
utilisent encore QPainter dans une petite surface puis une texture ; leurs traits ne sont pas encore dessinés par un shader.
Une modification du composite remplace toutes ses textures visibles : les transferts
régionaux et la composition GPU restent à développer.

La résidence graphique des blocs est limitée à la vue, mais aucun budget global de
VRAM configurable n’est encore implémenté. L’image source et le niveau réduit sont
encore en RAM ; ce travail ne migre pas les plans de calques et l’historique sur disque
et ne relève pas la limite de 2 Gio.

## Aperçus de réglages sur GPU

Le chemin de filtre GPU est limité : le calque actif doit être le dernier calque,
un réglage Courbes, Niveaux, Lumière, Couleur ou Balance des couleurs visible,
en mode Normal à 100 %, sans dossier ni écrêtage, avec un masque absent, désactivé ou uniformément
blanc. La vue doit être RVB et le composite réduit sous le réglage entièrement opaque.
Les autres situations utilisent automatiquement l’aperçu CPU existant, y compris
le backend logiciel.

- Un worker compose une fois la base réduite sous le réglage. Elle est conservée
  tant que la pile sous-jacente et la résolution du proxy restent valides.
- Pour les réglages à table, chaque changement de paramètres fabrique une table
  de **256 × 1 pixels** à partir des fonctions CPU de référence. Un shader du graphe de scène applique
  cette table à la base ; la photo n’est pas retransférée à chaque déplacement.
  Les calibrations des pipettes Courbes passent par la même table.
- Pour Courbes, Niveaux et Balance des couleurs, la table corrige chaque canal RVB.
  Pour Lumière, elle transforme la luminance ; le shader reconstruit ensuite les trois canaux avec
  les pondérations et divisions entières du moteur CPU. Exposition, Contraste,
  Hautes lumières, Ombres, Blancs et Noirs suivent ainsi la même règle colorimétrique
  que leur aperçu CPU. Une correction RVB indépendante donnerait un autre résultat.
- Couleur utilise un calcul direct dans le shader : Température et Teinte, puis
  Vibrance et Saturation selon la chrominance et la luminance du pixel. Les quatre
  paramètres sont transmis comme constantes ; changer un curseur ne recrée ni la
  texture photo ni une table. La table d’identité liée au matériau est réutilisée.
- Le calcul CPU de Balance des couleurs est partagé avec la génération de sa table,
  sans changement de ses règles d’arrondi et de précision 8/16 bits.
- Le Navigateur et les histogrammes reçoivent un proxy CPU auxiliaire, planifié
  à intervalles d’au moins 100 ms pendant le geste. Ils peuvent suivre avec un
  retard lié au calcul ; ils ne déclenchent pas de lecture GPU ni de composition
  complète synchrone pour suivre l’aperçu.
- À la validation, le rendu complet CPU conserve les 8/16 bits du document et
  remplace l’aperçu GPU quand il est prêt. Annuler ou modifier la pile invalide
  les bases en vol ; les résultats périmés ne réactivent pas un ancien aperçu.

L’aperçu GPU reste une approximation écran en 8 bits. Un document 16 bits peut
l’utiliser si sa base est réduite au format écran ; une base encore à sa résolution
native en 16 bits conserve le chemin CPU. Cette étape n’accélère ni l’export ni
la composition pleine résolution, et ne généralise pas les shaders aux autres filtres.
Les shaders sont précompilés avec Qt Shader Tools, avec une variante compatible
avec le regroupement des géométries Qt Quick (`BATCHABLE`).

## Vérification

Le test `canvas_render` couvre les couleurs et la transparence d’images 8/16 bits,
les frontières entre blocs, les sélections animées/cachées, la réutilisation au zoom
et au déplacement, la vue des couches, le masque isolé et le détachement du document.
Il vérifie aussi l’éviction/relecture des blocs hors écran, l’absence de référence
persistante au composite complet et la taille de la surface des sélections à fort zoom.

```sh
ctest --test-dir build -R '^(gpu_tone_preview|tone_render|canvas_render|qml_smoke)$' --output-on-failure
```

Ce test CTest utilise le backend logiciel pour rester exécutable dans la CI des trois
plateformes. `gpu_tone_preview` vérifie aussi la réutilisation des bases, l’annulation,
la résolution du proxy, les replis et le rendu final 16 bits. `tone_render` compare
les cinq réglages à la référence CPU ; sur GPU il vérifie également que modifier les
paramètres ne recrée pas les textures photo. Ces tests comptent les créations de textures par le canevas, pas les octets ou
les temps mesurés dans le pilote GPU. Il ne suffit donc pas à valider un backend natif.
Ce scénario a également passé sur **Metal, Apple M2 Pro**, avec le rendu Qt Quick
sur un thread distinct et un écran de facteur 2, ainsi que la comparaison des
cinq aperçus et des calibrations de pipettes. Les six contrôles
Lumière sont comparés aux deux extrêmes, sur des gris, des couleurs saturées, du noir
et du blanc. Les extrêmes de Température, Teinte, Vibrance, Saturation et des trois
canaux de Balance des couleurs sont aussi comparés à la référence. L’historique,
l’annulation et le rendu final Lumière/Couleur/Balance sont vérifiés en 8 et 16 bits.
Le test des cinq réglages passe aussi en **OpenGL 2.1 sur ce Mac**, avec
`QSG_RENDER_LOOP=basic`. Le mode OpenGL multithread s’arrête dans Qt/AppKit lors de
la création du contexte, avant le chargement du shader ; ce mode n’est pas validé.
Le fonctionnement normal macOS utilise Metal. Les backends GPU Linux/Windows
restent à vérifier sur leurs plateformes. Le même scénario peut s’exécuter sur un GPU,
sans manipulation de la souris :

```sh
# macOS, build avec BUILD_TESTING=ON
QT_QPA_PLATFORM=cocoa QSG_RHI_BACKEND=metal QSG_INFO=1 \
  build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --canvas-render-test
# Comparaison des shaders des cinq réglages sur le même backend
QT_QPA_PLATFORM=cocoa QSG_RHI_BACKEND=metal QSG_RENDER_LOOP=threaded \
  build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --tone-render-test
# Vérification OpenGL sur ce Mac, avec le rendu sur le thread principal
QT_QPA_PLATFORM=cocoa QSG_RHI_BACKEND=opengl QSG_RENDER_LOOP=basic \
  build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --tone-render-test

# Linux, dans une session graphique avec Vulkan disponible
QSG_RHI_BACKEND=vulkan QSG_INFO=1 \
  build/PhotoCrap --smoke-test --canvas-render-test
```

Supprimer une éventuelle variable `QT_QUICK_BACKEND=software` avant ces essais natifs.
Les comparaisons de pixels tolèrent deux unités sur les canaux 8 bits pour les
arrondis/interpolations. Les autres backends matériels doivent aussi être vérifiés ;
aucun facteur d’accélération n’est annoncé sans mesure comparative.

## Références Qt

- [Graphe de scène et backends](https://doc.qt.io/qt-6.8/qtquick-visualcanvas-scenegraph-renderer.html)
- [QQuickPaintedItem et surface intermédiaire](https://doc.qt.io/qt-6.8/qquickpainteditem.html)
- [Création et durée de vie des textures](https://doc.qt.io/qt-6.8/qquickwindow.html#createTextureFromImage)
- [Nœud image indépendant du backend](https://doc.qt.io/qt-6.8/qsgimagenode.html)
- [Matériaux et shaders du graphe de scène](https://doc.qt.io/qt-6.8/qsgmaterialshader.html)
- [Précompilation des shaders](https://doc.qt.io/qt-6.8/qtshadertools-build.html)
