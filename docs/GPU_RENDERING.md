# Canevas et accélération graphique

Étapes après **v0.15**, en développement : présentation du composite dans le
canevas, puis aperçus Courbes, Niveaux, Lumière, Couleur, Balance des couleurs,
Noir et blanc, Transfert de dégradé, Filtre photo, Mélangeur de couches, Isohélie,
Seuil, Style colorimétrique, Correction sélective et anciens réglages
Luminosité/Contraste, Exposition, Vibrance, Teinte/Saturation et Négatif sur GPU. Les pixels de travail
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

La composition des calques, les masques, les traits et les rendus complets des
filtres restent calculés sur CPU. Les aperçus pris en charge sont décrits ci-dessous.
Les surimpressions générales
utilisent encore QPainter dans une petite surface puis une texture ; leurs traits ne sont pas encore dessinés par un shader.
Une modification du composite remplace toutes ses textures visibles : les transferts
régionaux et la composition GPU restent à développer.

La résidence graphique des blocs est limitée à la vue, mais aucun budget global de
VRAM configurable n’est encore implémenté. L’image source et le niveau réduit sont
encore en RAM ; ce travail ne migre pas les plans de calques et l’historique sur disque
et ne relève pas la limite de 2 Gio.

## Aperçus de réglages sur GPU

Le chemin de filtre GPU est limité : le calque actif doit être le dernier calque,
un réglage Courbes, Niveaux, Lumière, Couleur, Balance des couleurs, Noir et blanc,
Transfert de dégradé, Filtre photo, Mélangeur de couches, Isohélie, Seuil,
Style colorimétrique, Correction sélective, Luminosité/Contraste, Exposition,
Vibrance, Teinte/Saturation ou Négatif visible,
avec un mode de fusion GPU pris en charge, sans dossier ni écrêtage. Les masques absents, désactivés,
uniformes et peints sont pris en charge. Son opacité peut varier de 0 à 100 %.
La vue doit être RVB et le composite réduit sous le réglage entièrement opaque.
Les autres situations utilisent automatiquement l’aperçu CPU existant, y compris
le backend logiciel.

- Un worker compose une fois la base réduite sous le réglage. Elle est conservée
  tant que la pile sous-jacente et la résolution du proxy restent valides.
- Pour les réglages à table, chaque changement de paramètres fabrique une table
  de **256 × 1 pixels** à partir des fonctions CPU de référence. Un shader du graphe de scène applique
  cette table à la base ; la photo n’est pas retransférée à chaque déplacement.
  Les calibrations des pipettes Courbes passent par la même table.
- Pour Courbes, Niveaux, Balance des couleurs, Isohélie, Luminosité/Contraste
  et Exposition, la table corrige chaque canal RVB.
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
- Noir et blanc calcule les six familles de teintes sur GPU, avec les valeurs
  et défauts du moteur CPU, puis indexe une palette de gris ou de colorisation.
  Cette palette est produite par `applyBlackWhite` : la couleur de teinte garde
  les conversions HSL de QColor, sans réimplémentation approximative dans le shader.
- Transfert de dégradé indexe la palette CPU à partir de la luminance entière arrondie
  comme dans `applyGradientMap`. Modifier les deux couleurs ne retransfère pas la photo.
- Filtre photo et Mélangeur de couches utilisent une matrice RVB avec une constante
  par canal. Le Filtre photo inclut la densité et la correction de luminosité dans
  cette matrice ; sa résolution de couleur et son ancien paramètre de teinte sont
  partagés avec le CPU. Le Mélangeur conserve ses coefficients, constantes et
  valeurs monochromes. Seuls les coefficients changent pendant le geste, sans
  retransfert de la photo ni de la table d’identité liée au matériau.
- Isohélie utilise la table de `applyPosterize`, pour conserver les mêmes arrondis
  des paliers de 2 à 32 niveaux. Seuil calcule la luminance avec les coefficients
  entiers 2126/7152/722 et l’arrondi du chemin CPU 8 bits, puis indexe sa table
  noir/blanc. Le calcul utilise des entiers de précision suffisante sur les backends
  vérifiés ; le rendu final conserve le chemin natif 8/16 bits du CPU.
- Style colorimétrique reprend les six formules internes sur GPU, avec intensité
  et préservation de luminosité. L’ordre des opérations est conservé, y compris
  la dépendance du bleu au rouge déjà transformé dans Pellicule passée.
- Correction sélective calcule les pondérations des neuf familles, leurs 36 réglages
  et l’adaptation relative/absolue dans le shader. Les noms et l’ordre des familles
  sont partagés avec le CPU. Les anciens projets au réglage global, sans les neuf
  familles, gardent le rendu CPU asynchrone pour préserver leur comportement.
- Les anciens réglages Exposition et Vibrance gardent leurs règles propres,
  distinctes des panneaux Lumière et Couleur. Exposition utilise une table issue
  du calcul CPU (EV, décalage, gamma). Vibrance calcule directement son facteur
  dans le shader, y compris ses valeurs négatives. Leurs calculs CPU sont isolés
  dans `ExposureFilter` et `VibranceFilter`, avec conservation des 8/16 bits et de l’alpha.
- Teinte/Saturation utilise une conversion HSL dans le shader, avec quantification
  compatible avec les composantes de QColor. Son calcul CPU dédié conserve les
  pixels natifs et l’alpha. Les gris utilisent une teinte neutre au lieu du marqueur
  indéfini de QColor : une rotation négative ne les transforme plus en noir.
- Négatif utilise une table inversée fixe, partagée entre les prévisualisations,
  et un calcul CPU par canaux mémoire sans conversion QColor. Ce réglage n’a pas
  de curseur de couleur ; son ajout et le rendu final restent asynchrones sur CPU,
  et le chemin GPU est disponible pendant une session de propriétés éligible.
- Le shader compose le réglage avec la base opaque selon l’opacité du calque et
  la couverture du masque uniforme. Il applique la densité du masque et la
  prémultiplication écran en 8 bits avant l’opacité, comme le chemin CPU. Un contour
  progressif ne change pas un masque uniforme. Ces deux constantes occupent les
  emplacements libres avant les neuf vecteurs : le bloc reste de 224 octets.
  Les masques uniformes ne nécessitent aucune texture supplémentaire. Les bases
  transparentes conservent le chemin CPU.
- Pour un masque peint, un worker prépare une image Grayscale8 à la résolution
  de la base écran, avec le même redimensionnement et flou CPU que l’aperçu existant.
  Le shader reçoit les blocs visibles de ce masque avec les mêmes coordonnées et
  pixels de bordure que la photo. La texture utilise le format d’affichage RGB
  standard pour rester portable ; le masque de travail et son proxy restent en 8 bits.
  Les blocs de masque quittant la vue sont libérés et rechargés au retour.
- Changer la densité du masque peint modifie uniquement une constante du shader.
  Le contour progressif prépare un nouveau masque en arrière-plan, en réutilisant
  la base écran. Les demandes successives remplacent celles en attente ; un résultat
  ancien ne supprime pas une demande de contour plus récente. Annulation, nouvelle
  pile et changement de résolution invalident les résultats en vol. Pendant ce
  calcul, le dernier masque terminé reste affiché. La peinture elle-même reste CPU.
- Quatorze modes sont pris en charge : Normal, Obscurcir, Produit, Densité linéaire +,
  Éclaircir, Écran, Densité linéaire − (Ajout), Incrustation, Lumière crue, Lumière
  linéaire, Lumière ponctuelle, Différence, Exclusion et Soustraction. Le registre
  de ces modes est partagé entre l’éligibilité du moteur et les tests.
- Les quatre fusions Teinte, Saturation, Couleur et Luminosité utilisent une
  conversion HSL commune avec Teinte/Saturation. Elle conserve les quantifications
  QColor (centièmes de degré, saturation/luminosité 16 bits), une teinte neutre
  pour les gris et les canaux mélangés 16 bits avant l’arrondi écran final.
- Densité couleur +, Densité couleur − et Division utilisent les coordonnées
  entières 0–255 des canaux avant de former leurs rapports. Les cas source zéro
  ou 255 gardent les branches du CPU, sans epsilon arbitraire dans les dénominateurs.
- Lumière tamisée reprend le polynôme et la racine du moteur existant, y compris
  sa branche basse. Lumière vive choisit entre densité et éclaircissement selon
  le demi-ton, avec les canaux entiers pour ses dénominateurs.
- Couleur plus foncée et Couleur plus claire comparent les sommes RVB entières,
  puis choisissent une couleur complète. En cas d’égalité, elles gardent la base,
  même si la couleur du réglage a une teinte différente.
- Ces onze fusions demandent des octets d’entrée exacts. Elles sont éligibles
  avec Courbes, Niveaux, Luminosité/Contraste, Exposition, Balance des couleurs,
  Isohélie, Lumière, Transfert de dégradé, Seuil et Négatif, sans atténuation du
  masque : masque absent, désactivé, uniforme blanc ou densité à zéro. L’opacité
  du calque peut rester réduite. Les autres réglages et les masques visibles restent
  CPU : une différence d’un canal peut changer fortement une teinte près du gris
  ou un rapport près d’un dénominateur nul. Ce repli ne change pas le comportement de l’outil.
- Les fusions RVB reprennent les formules CPU. Le masque est appliqué avant la
  conversion des pixels prémultipliés en couleurs droites, puis le mélange utilise
  l’alpha du masque et l’opacité. Normal conserve les arrondis de son chemin QPainter
  distinct. Le mode de fusion est codé dans les bits supérieurs de l’entier du
  shader ; le bloc reste de 224 octets et aucun sampler n’est ajouté.
- Le survol des modes de fusion d’un réglage éligible ne change que cette constante.
  Les calques de pixels, Fondu et Mélange maximal restent CPU. Les onze modes
  conditionnels utilisent les règles plus strictes décrites ci-dessus. Cela porte
  à 25 les modes éligibles pour certains aperçus de réglage.
  Il s’agit de l’aperçu du réglage actif, pas d’une composition GPU de toute la pile.
- Les tables identiques sont conservées. Changer uniquement la densité ou le
  contour progressif d’un masque uniforme ne retransfère ni la photo ni la table.
  Le mélange des six familles Noir et blanc réutilise aussi sa palette quand la
  colorisation reste identique.
- Les paramètres du shader sont regroupés dans neuf `vec4` réutilisés entre les
  algorithmes. Le bloc complet occupe 224 octets selon la réflexion du shader,
  plutôt que de réserver un bloc différent pour chaque filtre. Changer les styles
  ou les corrections sélectives ne retransfère ni la photo ni la texture d’identité.
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

## Aperçu des menus de styles

Les menus Noir et blanc, Filtre photo et Mélangeur de couches prévisualisent
le style survolé, y compris la ligne déjà sélectionnée. Les flèches du clavier
font aussi varier l’aperçu dans le menu ouvert. Ils utilisent le même rendu de
calque que les curseurs : GPU dans les cas éligibles, sinon proxy CPU asynchrone.

Le survol ne crée pas d’historique. Échap ou un clic extérieur restaure les valeurs
présentes à l’ouverture, y compris une couleur personnalisée. Un clic ou Entrée
valide une seule étape d’annulation. Annuler dans les propriétés restaure encore
l’ensemble de la session, même après la validation d’un style. Le contrôle Aperçu
reste respecté lorsqu’il est désactivé.

Le composant commun `PresetComboBox.qml` distingue surbrillance et activation,
avec un délégué de survol pour la ligne courante. Voir les signaux
[highlighted et activated de ComboBox](https://doc.qt.io/qt-6.8/qml-qtquick-controls-combobox.html#signals).

## Vérification

Le test `canvas_render` couvre les couleurs et la transparence d’images 8/16 bits,
les frontières entre blocs, les sélections animées/cachées, la réutilisation au zoom
et au déplacement, la vue des couches, le masque isolé et le détachement du document.
Il vérifie aussi l’éviction/relecture des blocs hors écran, l’absence de référence
persistante au composite complet et la taille de la surface des sélections à fort zoom.

```sh
ctest --test-dir build -R '^(gpu_tone_preview|gpu_conditional_blend_preview|tone_render|preset_preview|canvas_render|qml_smoke)$' --output-on-failure
```

Ce test CTest utilise le backend logiciel pour rester exécutable dans la CI des trois
plateformes. `gpu_tone_preview` vérifie aussi la réutilisation des bases, l’annulation,
la résolution du proxy, les replis et le rendu final 16 bits. `tone_render` compare
les dix-huit réglages à la référence CPU. La liste des filtres est partagée avec
le moteur : un nouvel aperçu éligible est automatiquement parcouru et le test
affiche chaque filtre ainsi que leur nombre ; sur GPU il vérifie également que modifier les
paramètres ne recrée pas les textures photo. `preset_preview` exerce survol, navigation clavier, validation, Échap, clic extérieur,
Aperçu désactivé et annulation de la session des propriétés pour les trois menus.
Ces tests comptent les créations de textures par le canevas, pas les octets ou
les temps mesurés dans le pilote GPU. Il ne suffit donc pas à valider un backend natif.
Ce scénario a également passé sur **Metal, Apple M2 Pro**, avec le rendu Qt Quick
sur un thread distinct et un écran de facteur 2, ainsi que la comparaison des
dix-huit aperçus et des calibrations de pipettes. Les six contrôles
Lumière sont comparés aux deux extrêmes, sur des gris, des couleurs saturées, du noir
et du blanc. Les extrêmes de Température, Teinte, Vibrance, Saturation et des trois
canaux de Balance des couleurs sont aussi comparés à la référence. L’historique,
l’annulation et le rendu final sont vérifiés en 8 et 16 bits pour Lumière, Couleur,
Balance des couleurs, Noir et blanc, Transfert de dégradé, Filtre photo et Mélangeur
de couches, Isohélie, Seuil, Style colorimétrique et Correction sélective. Les six familles Noir et
blanc sont comparées à −200 et +300, avec et sans teinte, ainsi que plusieurs palettes
chaudes, froides et achromatiques. Les motifs incluent un cycle de teintes saturées
et atténuées pour contrôler les transitions entre familles.
Les essais Filtre photo couvrent densités 0/25/100 %, couleurs chaudes/froides,
noir/blanc et repli sur l’ancienne teinte, avec et sans préservation de luminosité.
Les coefficients et constantes du Mélangeur sont comparés à −200/+200 en RVB
et en monochrome. Ces modifications ne retransfèrent pas les textures photo.
Les 31 réglages d’Isohélie et les limites de Seuil (dont 0, 1, 127, 128, 129, 254
et 255) sont comparés pixel par pixel sur des rampes de gris et de primaires.
Ces contrôles couvrent les ruptures des fonctions, sans transfert des textures photo.
Les six styles sont comparés à intensité 0/35/100 %, avec et sans préservation de
luminosité. Chaque composante de chacune des neuf familles de Correction sélective
est vérifiée à −100/+100 en relatif et en absolu ; un projet enregistré avec les
anciens paramètres globaux vérifie le repli CPU. Les anciens aperçus sont aussi
retestés après le regroupement des paramètres du shader. Les combinaisons
extrêmes de Luminosité/Contraste et Vibrance/Saturation sont comparées sur des rampes,
ainsi que 27 combinaisons EV/décalage/gamma d’Exposition. Des valeurs connues
vérifient séparément les règles CPU, le facteur négatif de Vibrance et les nuances
fines en 16 bits. Teinte/Saturation est comparé sur 135 combinaisons de rotation
(incluant les demi-degrés et le passage à ±180°), saturation et luminosité, sur
rampes et couleurs saturées/atténuées. Des valeurs connues et la formule QColor
indépendante contrôlent aussi les primaires, les gris, l’alpha et les pixels 16 bits.
Les comparaisons denses incluent désormais les lignes de couleurs mélangées.
Les dix-huit shaders sont aussi comparés à la composition QPainter avec les masques
uniformes 0/128/255, les densités 0/37/100 % et une opacité de 37 %. Niveaux couvre
également les opacités 0/1/50/99/100 %. Les contrôles vérifient la réutilisation des
textures quand seule la densité change et le remplacement par le rendu final CPU.
Les tests moteur couvrent ces états en 8 et 16 bits, l’historique, l’annulation et
la désactivation du masque. Les masques peints sont exercés sur les dix-huit
réglages avec densités 0/37/100 % et contours progressifs 2/8 pixels. La densité ne
retransfère aucune texture ; le contour ne remplace que celle du masque. Un cas de
1 280 pixels de large vérifie la frontière à 1 024 pixels, l’éviction/relecture et
le niveau de détail réduit. Les tests moteur vérifient aussi les contours demandés
rapidement, l’annulation pendant le calcul et le redimensionnement, en 8/16 bits.
Les quatorze modes de fusion sont comparés sur les dix-huit réglages avec masque
peint, contour progressif et opacité réduite ; Niveaux est aussi vérifié à 100 %.
Les compteurs doivent rester stables pendant le changement de mode. Les tests
moteur couvrent chaque mode en 8/16 bits avec et sans masque peint, l’historique,
l’annulation, le repli pour tous les modes restants et le retour au GPU après Fondu.
Les quatre fusions HSL sont comparées à QColor sur chacun des dix réglages
éligibles, avec des rampes grises/primaires et des couleurs saturées ou atténuées.
Le changement de fusion ne doit retransférer ni la photo ni la table. Les tests
moteur couvrent les dix réglages en 8/16 bits, l’historique, l’annulation, le repli
avec masque gris, le retour au GPU à densité zéro ou masque désactivé, ainsi que
le repli de chacun des huit autres réglages pour ces onze modes. Ils sont regroupés
dans `gpu_conditional_blend_preview`, séparé de la suite générale pour conserver des délais
de test courts. Les images uniformes du test moteur restent petites ; le scénario
natif conserve ses motifs couleur complets. Les trois modes de rapport et les deux
modes de contraste sont comparés sur les dix réglages éligibles et sur chaque valeur
de base 0–255, face à des sorties Courbes constantes
0/1/2/51/63/64/127/128/191/192/204/253/254/255, à 100 % et 37 % d’opacité.
Les seuils autour de 1/4 et 1/2 sont ainsi couverts. Des valeurs connues conservent
la branche basse de Lumière tamisée (51 vers 204 donne 120 à pleine opacité).
Les deux sélections de couleur sont aussi vérifiées avec des décalages opposés
rouge/vert, pour exercer les sommes égales et celles modifiées par l’écrêtage. Le changement de mode ne retransfère aucune photo. Le test moteur
utilise `QTest::qWaitFor` pour traiter les événements sans les pauses de 50 ms de
chaque attente générale ; les délais maximaux restent identiques.
Le test des dix-huit réglages passe aussi en **OpenGL 2.1 sur ce Mac**, avec
`QSG_RENDER_LOOP=basic`. Le mode OpenGL multithread s’arrête dans Qt/AppKit lors de
la création du contexte, avant le chargement du shader ; ce mode n’est pas validé.
Le fonctionnement normal macOS utilise Metal. Les backends GPU Linux/Windows
restent à vérifier sur leurs plateformes. Le même scénario peut s’exécuter sur un GPU,
sans manipulation de la souris :

```sh
# macOS, build avec BUILD_TESTING=ON
QT_QPA_PLATFORM=cocoa QSG_RHI_BACKEND=metal QSG_INFO=1 \
  build/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test --canvas-render-test
# Comparaison des shaders des dix-huit réglages sur le même backend
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
