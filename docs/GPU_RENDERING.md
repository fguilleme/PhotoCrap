# Canevas et accélération graphique

## Niveaux de détail conservés sur GPU — 3 novembre 2026

Les piles GPU éligibles possèdent quatre niveaux d’affichage (1/2 à 1/16), calculés
par deux passes d’aire sur le composite natif de chaque tuile. La grille est celle
du document complet ; les deux coordonnées globales servent à définir les poids,
y compris pour les dimensions impaires. Une marge native fixe de 33 pixels,
ajoutée au halo spatial, fournit les voisins nécessaires au filtrage des niveaux.
Chaque niveau garde une passe horizontale RGBA32F et une sortie RGBA8 ou RGBA32F
selon la profondeur du document. Les réductions 8/16 bits conservent l’alpha
prémultiplié ; la sortie est quantifiée selon la profondeur native.

Les niveaux sont créés à la demande. Un zoom vers un niveau déjà calculé réutilise
sa texture, sans nouveau transfert, recomposition native ou réduction. Une version
native modifiée invalide ces niveaux ; seul le niveau demandé est recalculé.
Leur stockage et leurs intermédiaires sont comptés dans le budget partagé et dans
la réserve conservatrice du jeu de travail visible. Les tuiles hors écran restent
évincées par LRU. La destruction du graphe libère aussi les niveaux.

Le CPU conserve son chemin réduit pour les piles incompatibles, les formats
manquants, le zoom au-delà du niveau 1/16 et les jeux de travail trop grands. Les
entrées et accumulateurs natifs restent nécessaires : cette première pyramide ne
transforme pas une pile native dépassant le budget en pile GPU éligible. Export,
sauvegarde et référence pleine résolution restent natifs CPU.

Les tests contrôlent tous les pixels d’une image 1057 × 545 en 8/16 bits, avec
transparence, masque et opacité ; ils couvrent les raccords, les zooms aller-retour,
le repli sous petit budget et la recréation des ressources. Qt décrit la libération
anticipée comme dépendante du backend : si la demande est ignorée, le test détruit
puis recrée sa fenêtre dédiée ([QQuickWindow::releaseResources](https://doc.qt.io/qt-6/qquickwindow.html#releaseResources)).


## Filtres spatiaux dans la pile GPU — 3 novembre 2026

Flou de boîte, Flou gaussien, Flou directionnel, Netteté et Passe-haut utilisent
maintenant des passes séparées sur les tuiles visibles. Le halo de lecture est
la somme des voisinages de la pile, puis seul le centre de la tuile est affiché.
Les passes conservent l'alpha prémultiplié, les masques et la fusion du calque.

Le gaussien reprend les trois convolutions de boîte du moteur. En 16 bits, chaque
passe conserve sa troncature native. Pour le flou 8 bits accéléré sur Mac, la passe
horizontale conserve la somme entière dans une cible RGBA32F ; l'arrondi 2D n'est
appliqué qu'après la verticale, comme dans la référence vImage. Le flou directionnel
reprend les positions et le nombre d'échantillons du CPU. Netteté et Passe-haut
utilisent l'image originale et le gaussien sans écraser l'accumulateur de la pile.
Les deux cibles auxiliaires sont réutilisées par les filtres successifs et comptées
dans le budget avant allocation.

Les paramètres actualisent directement le snapshot GPU, indépendamment d'un
worker CPU occupé. Les aperçus CPU auxiliaires du Navigateur/histogramme sont
limités à un lancement toutes les 100 ms pendant le geste ; seule la dernière
demande en attente est conservée. La validation finale reste un calcul CPU natif
asynchrone. Annuler restaure immédiatement le snapshot précédent ; un résultat
CPU périmé ne remplace pas ce nouvel état.

Les modes et combinaisons non pris en charge, les masques adoucis, les formats
indisponibles et les jeux de travail trop grands conservent le repli CPU. Les
calques uniformément transparents ne créent pas de passes ; les ruptures de
chaînes d'écrêtage consécutives sont regroupées.

## Composition tuilée de la pile — 3 novembre 2026

`LayerStackNode` utilise QRhi et des passes hors écran par tuile de 512 px. Les
calques de pixels, le texte et Couleur unie sont composés avec leurs masques,
opacités et écrêtages, en respectant les groupes masqués. Les 21 modes pris en
charge sont comparés au CPU en 8 et 16 bits. Les modes 1, 18 et 23–26, les
réglages tonals autres que Couleur unie et les masques adoucis déclenchent un repli de
toute la pile vers le chemin existant. Le rendu final natif CPU reste la référence.

Les entrées et les cibles sont RGBA8 en 8 bits. En 16 bits, les entrées et les
intermédiaires sont RGBA32F, avec quantification à 65 535 entre les opérations ;
un demi-flottant ne suffit pas à représenter toutes les valeurs natives. La
couverture d'écrêtage suit le masque 8 bits existant. Le rendu d'affichage est
comparé avec une tolérance de deux unités 8 bits, sans modifier les pixels sauvegardés.

Une version régionale dépend des identifiants immuables des tuiles de stockage,
pas du QImage reconstruit. Une modification distante ou un déchargement disque
ne provoque donc aucun transfert de la région inchangée. Le résultat de la
composition reste sur GPU entre les images. Les snapshots de plans sont libérés
après leur préparation ; les tuiles hors écran sont évincées selon leur ancienneté.
OpenGL utilise des shaders GLSL 330 avec emplacements de sorties explicites :
les couleurs et la couverture ont deux cibles distinctes. Les contextes de bureau
antérieurs à 3.3 et OpenGL ES antérieur à 3.0 conservent le repli CPU sans tentative
de compilation incompatible. Avec les framebuffers à axe Y inversé, la zone
source est réfléchie dans la texture entière avant son inversion, afin de ne pas
décaler la marge de voisinage dans la partie visible.

Le jeu de travail visible est estimé avant allocation ; s'il dépasse le budget,
l'affichage utilise le chemin CPU. Le budget graphique global et les niveaux de
détail GPU restent les étapes suivantes du plan de migration.

Ce chemin est compilé lorsque Qt 6.6+ fournit `GuiPrivate`, et activé uniquement
sur un backend QRhi ayant les formats requis. Les ressources vivent sur le thread
du scene graph, sans accès à un Document mutable. Les cibles ping-pong gardent la
propriété de leurs textures ; les matériaux d'affichage ne font que les emprunter.
Voir les références Qt sur [QRhi](https://doc.qt.io/qt-6/qrhi.html),
[QSGRenderNode](https://doc.qt.io/qt-6/qtquick-scenegraph-customrendernode-example.html)
et la [propriété des textures](https://doc.qt.io/qt-6/qquickwindow.html#createTextureFromRhiTexture).

Accélérations livrées dans **v0.16**, puis étapes en développement : présentation du composite dans le
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
  ceux qui sortent de la vue peuvent rester dans le cache graphique borné.
  Il n’y a pas de texture unique de la taille d’un grand document.
- Les textures sont réutilisées tant que l’image, le niveau et les blocs visibles
  restent identiques. Changer le zoom dans le même niveau ne fait que modifier la
  géométrie des nœuds ; déplacer la vue réutilise les blocs encore présents.
- Une recomposition régionale ne remplace que les tuiles touchées, y compris aux
  niveaux de détail réduits. Le moteur transmet le rectangle réellement recomposé, incluant
  l’influence des filtres et du contour progressif des masques. Le canevas inclut
  aussi le pixel voisin de chaque tuile pour préserver les raccords interpolés.
  Une retouche hors champ ne retransfère aucune tuile résidente ; son résultat
  est chargé lorsque la vue se déplace.
- Le rectangle est lié aux clés exactes du composite avant/après. Si un autre
  consommateur a déjà calculé plusieurs versions entre deux images du canevas,
  les clés ne correspondent plus et l’affichage recharge toutes les tuiles.
  Il ne conserve pas une référence au composite pour effectuer ce contrôle.
  Les nœuds comparent les clés des copies de tuiles et gardent leurs textures
  lorsque leurs pixels n’ont pas été invalidés.
- Le damier de transparence utilise une texture répétée de 24 × 24 sur les backends
  RHI. Le rendu logiciel garde un fond limité à la zone visible.
- Les contours des sélections générales, le marquage de suppression et l’aperçu
  de Pièce sont séparés de la photo. Sur les backends RHI, Pièce utilise directement
  sa texture donneuse bornée, le marquage est une géométrie translucide et les
  contours généraux utilisent
  une géométrie de trait et un shader de pointillés : animer leurs fourmis ne
  recrée ni image, ni texture, ni géométrie.
- Les ressources graphiques sont créées et détruites sur le thread de rendu, avec
  reconstruction après suppression du nœud ou changement de fenêtre.

Les curseurs natifs et les événements clavier/souris restent sur les mêmes objets
QML. Les contours de sélection passent par le canevas commun ; seuls les guides
ouverts initiaux, polygonaux et de combinaison conservent une surimpression QML.

## Précision et limites

L’affichage utilise des textures **8 bits par canal**, comme la surface d’affichage
précédente. Les pixels de travail, projets et exports conservent leurs **8 ou 16 bits**.
Ce n’est ni un affichage HDR, ni une nouvelle gestion ICC.

La composition des calques, les masques, les traits et les rendus complets des
filtres restent calculés sur CPU. Les aperçus pris en charge sont décrits ci-dessous.
Les contours généraux, le marquage de Suppression et l’aperçu de Pièce utilisent
les géométries GPU décrites plus bas, avec repli QPainter pour le backend logiciel
ou les cas trop complexes. L’image donneuse de Pièce reste préparée par le moteur
CPU ; son affichage ne passe plus par une seconde surface de la taille de la vue.
Les transferts régionaux couvrent le composite CPU et ses niveaux réduits.
`render/DisplayLevel` met à jour le cache réduit par portions lorsque les dimensions
source sont divisibles par celles du niveau. Les portions sont alignées sur cette
grille et redimensionnées avec `QImage::scaled(..., SmoothTransformation)`, puis
recopiées sans conversion dans le cache. Un pixel de garde du niveau est ajouté
avant d’invalider les halos des tuiles.

Quand les dimensions forment une grille irrégulière, découper la source déplacerait
la phase et les poids de Qt. Dans ce cas, la réduction complète reste utilisée,
mais seules les tuiles intersectant la région projetée sont retransférées. Aucun
nouvel algorithme de réduction ni changement de précision des pixels de travail
n’est introduit. Le cache ne garde pas de référence au composite natif.

Un changement de dimensions, un rendu asynchrone complet,
une vue de couche/masque ou une annulation sans rectangle fiable conserve également
le rechargement complet. La composition GPU de la pile reste à développer.
Fondu, Bruit et Mosaïque demandent une recomposition CPU complète, car leur résultat
dépend des coordonnées absolues. Les masques adoucis élargissent le rectangle de
recomposition de leur rayon ; les halos des filtres s’additionnent.

## Cache graphique de navigation

Préférences → Cache de travail propose un **Cache graphique** de 0 à 1 024 Mio,
avec 64 Mio par défaut. La valeur est enregistrée dans `cache/textureMiB` et
appliquée immédiatement au canevas ; 0 retrouve l’éviction immédiate hors champ.

Les tuiles visibles sont prioritaires. L’espace restant conserve les tuiles hors
champ les plus récemment affichées, avec un coût estimé de 4 octets par pixel
pour la photo et autant pour son masque éventuel, bordures comprises. Les tuiles
les moins récentes sont libérées en premier. Si la zone visible dépasse le budget,
elle reste complète et aucune réserve hors champ n’est conservée. Ce budget est
une cible pour les textures photo/masque du canevas, pas une limite de la VRAM
totale du processus : tables, surimpressions, allocations du pilote et ressources
en cours de libération ne sont pas comptées.

Les nœuds hors champ sont détachés du dessin mais gardés en cache. Revenir sur
une tuile valide réutilise sa texture. Une retouche régionale invalide aussi ses
tuiles en cache ; les changements globaux, de résolution ou de document les
recréent. Chaque snapshot de masque retient la version source qu’il représente :
une modification hors champ est comparée et appliquée lorsque sa tuile revient.
Désactiver le masque libère également les textures de masque conservées hors champ.

Les copies CPU des tuiles suivent les mêmes décisions de conservation. Elles
restent indépendantes du composite et conservent sa précision (donc jusqu’à
8 octets par pixel pour une copie couleur 16 bits) ; leur RAM s’ajoute au budget
des textures 8 bits. L’image source et le niveau réduit sont encore en RAM ; ce
travail ne migre pas les plans de calques et l’historique sur disque et ne relève
pas la limite de 2 Gio.

## Vérification des transferts régionaux

Le test `canvas_render` contrôle en 8/16 bits qu’un trait local remplace une seule
texture sur trois, qu’un trait à la frontière en remplace deux, et qu’un trait hors
champ ne crée aucune texture avant le déplacement de la vue. Il vérifie aussi
l’annulation, le rechargement complet lorsque des versions intermédiaires manquent,
les changements de niveau et les couleurs effectivement lues dans la fenêtre.
Les niveaux réduits 1/2 sont testés avec trois tuiles visibles, en 8/16 bits et
avec dimensions régulières ou irrégulières. Des compteurs distincts contrôlent les
transferts de textures et la surface source rééchantillonnée : les petites retouches
sur une grille entière traitent moins de 1 % de l’image dans ce scénario. Les
retouches hors champ doivent aussi apparaître correctement lors du déplacement.
Le scénario de navigation teste aussi un retour sans transfert, l’éviction en
baissant le budget, la réactualisation d’une tuile retouchée hors champ et le
maintien de toute la zone visible avec un budget inférieur à sa taille.
La suite `tile_cache` contrôle l’ordre LRU, les coûts photo/masque différents,
les égalités et les budgets nuls ou dépassés. Les tests de réglages vérifient
la persistance, les bornes et la liaison bidirectionnelle entre préférence et canevas.
La suite `display_level` compare exactement le cache partiel à la réduction
complète Qt sur des motifs à haute fréquence, avec transparence, formats 8/16 bits,
bords, changements de format/niveau et dimensions irrégulières. Elle vérifie aussi
que les pixels hors région projetée restent inchangés.
Les tests moteur comparent le résultat régional à une reconstruction complète avec
un flou ou un masque adouci, et contrôlent les replis pour Fondu, Bruit et Mosaïque.

## Contours de sélection animés sur GPU

L’animation commune des pointillés est suspendue quand la fenêtre principale est
masquée ou réduite. Elle reprend dès que la fenêtre redevient visible, sans
modifier la sélection ; les aperçus et outils qui utilisent cette phase partagent
la même suspension.

Les sélections générales (`selectionShape >= 6`, dont les sélections combinées ou
issues d’un masque), les rectangles, les ellipses, les rangées/colonnes uniques et
les lassos terminés passent par `render/SelectionMesh` et `SelectionOutlineNode`.
Le lasso libre utilise aussi ce chemin pendant le geste, dès que son candidat
forme une zone (`selectionShape == 4` et `hasSelection`). Sa phase animée ne
repeint plus de texture QML, même avant le relâchement. Les guides ouverts initiaux,
polygonaux et de combinaison conservent leur affichage QML.

Les rectangles et ellipses créent directement un chemin de la taille de la sélection,
avec une vraie courbe elliptique. Le type et le rectangle font partie de la clé du
cache : passer de rectangle à ellipse aux mêmes dimensions remplace le tracé. Le
maillage projette les courbes selon l’échelle courante avant de les découper à la
vue ; il ne dessine ni remplissage ni contour de région pixellisé pour l’ellipse.
Leur trait adopte les mêmes dimensions cosmétiques que les sélections générales.
Sur le backend logiciel, ce chemin est peint dans la surface de surimpression
limitée à la vue, avec le même repli QPainter que les sélections générales. Sans
autre surimpression raster simultanée, cette surface est aussi limitée à l’emprise
du contour et sa marge : une petite sélection ne repeint pas toute la vue.

Les sélections de rangée/colonne uniques (`selectionShape` 2/3) dessinent le rectangle
exact de leur bande d’un pixel natif. Le zoom agrandit cette bande mais conserve un
trait cosmétique, comme les autres contours. Seuls ses segments visibles produisent
de la géométrie ; une rangée de 16 384 pixels à fort zoom ne crée plus de texture
QML de cette longueur. Sur le backend logiciel, la surface est réduite à la bande
visible et à sa marge. Les pixels sélectionnés et l’historique restent ceux du
moteur de sélection existant.

Pour le lasso libre, en cours ou terminé, et le polygonal terminé (`selectionShape`
4/5), le chemin est construit directement depuis une copie partagée des points
natifs `QPolygonF`,
sans conversion en `QVariantList` ni en contour de masque pixellisé. Le tracé est
fermé et conserve les concavités. Ses points font partie de la clé du cache : un
déplacement de sélection remplace la géométrie, sans transférer la photo.

`lassoPreviewActive` distingue un geste en cours d’une sélection terminée, y
compris quand Pièce ou Déplacement crée sa sélection temporaire : leur détourage
libre passe lui aussi sur le contour commun dès qu’il forme une zone. Le relâchement
réutilise le dernier maillage si les points restent identiques. Le Canvas QML
affiche seulement la ligne initiale sans zone, les guides polygonaux avec leur
segment provisoire et leurs sommets, et le candidat des sélections combinées
(la sélection combinée possède déjà son propre contour).
Sa surface est limitée à `imageCanvas.viewportRect` ; les points et la position du
pointeur sont translatés vers cette origine locale. Après validation ou annulation,
ce guide disparaît. Une sélection combinée ne garde donc plus le tracé candidat
QML par-dessus son contour final. La création du masque de sélection reste CPU.

Le chemin de région est conservé tant que le contour ne change pas. Le maillage
ne contient que les segments utiles à la vue, avec jointures en onglet bornées et
un découpage local. Ses positions sont relatives à l’origine de la vue ; la
distance le long du contour est calculée avant découpage puis ramenée à une
période courte. Déplacer la vue ne redémarre pas les pointillés. La projection des
jointures le long du segment conserve aussi cette phase quand un coin sort du champ.

Le shader applique un trait cosmétique de 1,5 pixel physique, avec une transition
anticrénelée d’un pixel aux bords et entre noir/blanc. Ce matériau indépendant possède un bloc de
72 octets utiles selon la réflexion du shader (matrice, opacité et phase) et n’utilise aucune texture. Une animation
fractionnaire ne met à jour que sa phase ; les opérations Pièce et Suppression
n’ont pas à être repeintes pour faire avancer les fourmis.

La préparation GUI possède aussi un chemin court pour cette seule phase.
`setSelectionPhase` planifie un polish, mais ne marque pas l’image comme modifiée.
Sans demande complète en attente, `updatePolish` conserve les snapshots, les
tuiles de masque, la résidence des tuiles et leur ordre LRU. Il demande le rendu
avec les données déjà préparées, sans reparcourir le cache ni appeler le réglage
d’échantillonnage du document. Le thread graphique continue ses vérifications
de ressources et sa mise à jour du matériau ; cette étape optimise la préparation
sur le thread de l’interface, pas l’ensemble de la synchronisation graphique.

Toute demande `queueFrame` reste prioritaire, même avec ses deux arguments à
`false` : c’est le cas d’un changement de budget du cache. Les notifications
d’image, de masque, de sélection, les changements de vue, de taille, de fenêtre
ou de facteur d’écran passent donc toujours par la préparation complète.
Une requête arrivée pendant cette préparation reste également planifiée.

Les compteurs de `canvas_render` vérifient l’absence de préparation complète
pendant les phases générales, rectangulaires/elliptiques et du lasso libre.
Des demandes coalescées de budget et de visibilité d’un calque vérifient aussi
l’éviction effective des tuiles hors champ et le rafraîchissement des pixels.

Le nœud hérite des transformations du canevas et utilise un clip du graphe de
scène, y compris sous rotation. Un changement de contour, d’échelle, de résolution
physique ou de vue reconstruit la géométrie. Les snapshots restent préparés sur
le thread GUI ; le thread de rendu ne consulte pas le document.

Le maillage est limité à **un million de sommets** (environ 15,3 Mio de données de sommets).
Au-delà, et sur le backend logiciel, le rendu QPainter précédent reste utilisé.
La première conversion d’une région très complexe reste sur CPU ; cette étape
n’accélère pas la création du masque de sélection ni son calcul de contour.

La suite `selection_mesh` contrôle les trous, les sous-chemins fermés, les échelles
non uniformes, les segments hors champ, la phase après découpage et les grandes
coordonnées. Le test natif `canvas_render` vérifie une animation visible, les phases
fractionnaires, la stabilité des compteurs de géométrie/textures, le déplacement,
la rotation et l’absence de remplissage de la sélection sur Metal et OpenGL. Il
contrôle aussi les courbes elliptiques, le changement de forme à dimensions égales,
le masquage et les pixels de travail 8/16 bits ; `selection_mesh` vérifie la
courbure à plusieurs échelles, y compris non uniformes. Les tests des lassos
vérifient aussi fermeture, concavités, transitions en cours/terminé, points déplacés,
conservation des pixels, phases fractionnaires, découpage et rotation. Le smoke
test QML contrôle la limite de la vue, le transfert du candidat libre vers le
canevas sans doublon et la conservation des guides polygonaux/combinés. Les tests
de rendu vérifient aussi les phases pendant le geste, la mise à jour des points,
la réutilisation du maillage au relâchement, le masquage et l’annulation dans les
trois outils de détourage libre, sans modification des pixels ni retransfert photo
(lasso en 8/16 bits, Pièce et Déplacement encore réservés aux documents 8 bits).
Les rangées et colonnes sont contrôlées en 8/16 bits aux zooms 0,5×, 1× et 4× :
visibilité, absence de remplissage hors bande, phases fractionnaires, découpage,
rotation et absence de modification des pixels. Le test de maillage utilise aussi
une bande de 16 384 pixels à 8× et vérifie une quantité de sommets bornée à la vue.

## Marquage de Suppression sur GPU

`render/RegionMesh` projette les rectangles disjoints de la région de marquage,
les découpe à la vue et génère des triangles en coordonnées locales. Les trous
issus de la soustraction restent vides ; les rectangles n’introduisent pas de
surfaces superposées qui doubleraient le mélange translucide.
`RegionOverlayNode` utilise le matériau couleur de Qt avec le même rose et le
même alpha que la référence QPainter. Aucune surface ni texture d’image n’est
nécessaire sur un backend RHI.

La géométrie est conservée jusqu’à un changement de région, d’échelle ou de vue.
Faire avancer les pointillés d’une sélection simultanée ne la reconstruit pas.
Annuler le marquage retire son nœud sans changer les pixels ni l’historique.
`ClippedOverlayNode`, commun à cette surface et aux contours de sélection, gère
la translation locale et la géométrie explicite du clip utilisée sous rotation.

La limite reste un million de sommets, soit environ 7,6 Mio pour le marquage.
Le backend logiciel et les régions dépassant cette limite conservent le chemin
QPainter. Cette optimisation concerne seulement l’affichage des marques : leur
région, la reconstruction asynchrone et l’historique restent gérés par le moteur
existant. L’outil Suppression reste réservé aux pixels 8 bits ; le refus des
pixels 16 bits ne provoque ni conversion ni modification.

La suite `region_mesh` vérifie l’aire des régions trouées, les échelles non
uniformes, le découpage, les grandes coordonnées et le repli de complexité.
`canvas_render` compare le mélange couleur, l’intérieur des trous et les zones
non marquées, puis contrôle l’animation simultanée, le clip sous rotation,
l’annulation et le maintien du refus en 16 bits sur Metal/OpenGL et en logiciel.

## Aperçu de Pièce en texture GPU

`render/PatchMesh` génère des triangles texturés suivant les rectangles disjoints
de la région cible. Les positions sont limitées à la vue et relatives à son origine ;
les coordonnées UV restent liées au rectangle cible entier, avant découpage.
Les trous restent donc vides et un déplacement de vue ne décale pas la texture.
`PatchPreviewNode` possède la texture donneuse et utilise le matériau texturé
standard de Qt, avec transparence prémultipliée et échantillonnage au plus proche,
comme l’affichage raster précédent.

Le moteur prépare toujours son image donneuse sur CPU, limitée à 1 000 pixels
sur chaque axe. Le canevas réutilise cette image immuable ; il ne la repeint plus
dans une seconde image couvrant la vue. Quand les pixels donneurs changent, la
texture bornée est retransférée entièrement. Des pixels identiques conservent
leur clé de texture, même si le moteur a produit une nouvelle instance d’image.
La géométrie ne change que si la région cible, son rectangle, l’échelle ou la vue
change (ou si le sous-rectangle de texture du backend change).

Ainsi, en mode Source, déplacer l’échantillon conserve généralement la géométrie ;
en mode Destination, déplacer un donneur inchangé conserve sa texture. L’animation
d’une sélection simultanée ne modifie aucune de ces ressources. Annuler libère
immédiatement l’aperçu ; terminer le geste le conserve jusqu’à l’arrivée du composite
réparé. Le même chemin d’affichage sert aux aperçus de déplacement
de région qui utilisent cette image ; aucun algorithme de réparation n’est modifié.

Le repli QPainter couvre le backend logiciel, le dépassement du million de sommets
(environ 15,3 Mio) et un échec d’allocation de texture. La préparation du donneur
reste synchrone sur CPU pendant le geste ; cette étape n’accélère que son affichage.
Les restrictions existantes des outils en 16 bits restent applicables.

La suite `patch_mesh` vérifie les UV après découpage, les trous, le rectangle cible,
les grandes coordonnées et le repli de complexité. `canvas_render` compare les
modes Source/Destination sur une image donneuse à motifs et semi-transparente,
contrôle la réutilisation des ressources, les trous, la rotation et l’annulation,
en logiciel ainsi que sur Metal et OpenGL.

## Réparation de Pièce en arrière-plan

Au relâchement, `PatchTool` envoie au worker des valeurs immuables : image source,
région cible, décalage, diffusion et couverture douce. L’adaptation de couleur par
`RegionHealing`, la recherche du rectangle de pixels modifiés, l’application du
contour progressif et la copie d’écriture du calque s’exécutent sur ce worker.
Le calcul de réparation reste limité à la région ; les pixels hors sélection et
les trous sont conservés. Le format actuel QImage demande encore une copie du
calque complet quand ses pixels changent ; elle a lieu hors du thread GUI, sans
réduire le coût mémoire de l’état d’annulation.

Le thread GUI installe ensuite l’image terminée et inscrit une seule étape dans
l’historique. Il demande un composite écran asynchrone, puis son raffinement ;
le donneur reste visible jusque-là, sans retour intermédiaire à la photo précédente.
Une réparation identique aux pixels source n’ajoute pas d’étape. Pendant le calcul,
le document est considéré modifié pour protéger la fermeture ; la sauvegarde et
l’export sont indisponibles afin de ne pas écrire un état antérieur à la réparation.

Échap ou Annuler pendant le calcul restaure la sélection d’origine et abandonne
le travail. Changer d’outil, de réglage Pièce, de sélection, de calque ou de document,
ou commencer un autre geste, invalide également le résultat en attente. Un jeton
atomique et une génération empêchent toute réponse tardive d’écraser le nouvel
état. La résolution de couleur et les lignes de réparation contrôlent l’annulation ;
les copies mémoire et la couverture douce sont vérifiées avant/après.

Les tests moteur comparent les pixels Source/Destination adoucis et transparents
à la référence, contrôlent les gestes successifs et les annulations, et vérifient
la réparation locale en 4K. Le test `canvas_render` vérifie aussi le remplacement
du donneur par l’image réparée, la conservation des trous et le retour par Annuler,
en logiciel et sur Metal/OpenGL.

## Assemblage du Déplacement basé sur le contenu

Le worker de `ContentAwareMoveTool` traite désormais toute la modification :
reconstruction de l’ancienne zone en mode Déplacement, adaptation du donneur,
assemblage des régions et copie d’écriture du calque. Le callback GUI ne peint
plus les pixels ; il installe l’image terminée, inscrit une étape d’historique
et lance le composite écran asynchrone. Extension garde les pixels d’origine.
Les calques échantillonnés par « Tous les calques » restent immuables.

Les patches locaux conservent le même ordre de composition que l’ancien chemin :
remplissage de la source, puis donneur par-dessus la destination, y compris quand
les régions se chevauchent. La couverture douce prend le maximum du masque source
et de sa version déplacée. Son application parcourt uniquement l’union des régions
touchées, en évitant l’espace vide entre elles. Les pixels hors région et les trous
restent inchangés. La copie du calque QImage complet reste nécessaire pour un état
modifié ; déplacer ce travail hors du GUI n’en réduit pas le coût mémoire.

L’aperçu donneur est conservé jusqu’au composite réparé. Échap, Annuler ou un
changement d’outil/réglage abandonne le calcul en cours et restaure la sélection.
Les modifications de sélection, de calque ou de document et les nouveaux gestes
invalident aussi les réponses tardives. Le document est signalé modifié pendant
le calcul ; sauvegarde et export attendent sa fin ou son annulation, comme Pièce.

La suite `content_move` compare les modes Déplacement/Extension à une référence
raster indépendante pour des sélections adoucies, transparentes et chevauchantes
ou séparées. Elle contrôle les changements de réglage, Annuler/Rétablir pendant
le calcul, les résultats tardifs et la sauvegarde après réparation. La suite moteur
conserve les contrôles 4K et d’échantillonnage des calques. `canvas_render` vérifie
le remplacement de l’aperçu, les trous et l’annulation en logiciel/Metal/OpenGL.

## Finition de Suppression en arrière-plan

`RemoveTool` capture l’image stockée du calque sans appeler `layerImage` : les
calques vides ou uniformes restent compacts sur le thread GUI. Le worker développe
leur plan de pixels si nécessaire, prélève les calques visibles si demandé, puis
reconstruit la texture. Il assemble un patch local avec la même composition Source
et les mêmes trous que le chemin précédent, compare les pixels et prépare la copie
du calque terminé. Le contour progressif s’applique seulement aux rectangles
disjoints des marques, en évitant les pixels hors région.

Le callback GUI installe directement l’image calculée ; il ne fait ni peinture,
ni développement de calque, ni copie complète. Il retire les marques, ajoute une
seule étape d’annulation et demande le composite écran puis son raffinement en
arrière-plan. Une zone identique ne crée pas d’historique et ses plans temporaires sont libérés
sur le worker ; si la reconstruction
échoue, les marques restent disponibles pour correction ou annulation.

Échap, le bouton Annuler, Annuler/Rétablir ou un changement d’outil abandonne le
calcul. Une nouvelle marque conserve les marques précédentes et relance le travail
sur leur union. Changer « Tous les calques » arrête le travail tout en gardant les
marques pour une nouvelle application. Les générations et le jeton d’annulation
rejettent les résultats périmés. Pendant le calcul, le document est considéré
modifié et la sauvegarde/l’export sont suspendus, comme pour Pièce et Déplacement.

La suite `removal` compare les pixels à une référence raster pour les calques
pleins ou vides, les couvertures adoucies, l’alpha et les trous. Elle vérifie les
annulations tardives, la sauvegarde, les calques uniformes et l’échec sans voisins.
Les tests moteur existants couvrent aussi les tracés 4K et les nouveaux marquages
pendant un travail. `canvas_render` vérifie la disparition des marques, l’image
réparée et Annuler en logiciel, Metal et OpenGL. La copie complète QImage reste
nécessaire à l’écriture d’un état modifié : cette étape déplace son coût hors du
GUI, sans réduire sa consommation mémoire. L’outil reste limité aux pixels 8 bits.

## Couverture douce de la Gomme magique

`MagicEraserTool` capture aussi le masque doux immuable de la sélection. Le worker
calcule la tolérance, la contiguïté, le lissage et l’opacité comme auparavant, puis
interpole les canaux prémultipliés de chaque pixel correspondant avec sa couverture.
Cette interpolation utilise les mêmes valeurs et arrondis natifs que
`SelectionCoverage::blend`, en 8 et 16 bits. Les pixels non correspondants ne sont
pas reparcourus pour appliquer la sélection, même entre deux zones éloignées.
Le rectangle modifié est accumulé par ligne après atténuation ; un résultat sans
modification n’ajoute pas d’historique.

Le callback installe l’image terminée sans repeindre ni détacher une seconde fois
le plan complet. Une copie native reste nécessaire dans le worker pour écrire les
pixels et conserver l’annulation ; le masque de recherche occupe toujours la zone
limitée par la sélection ou le document. Le prélèvement de tous les calques reste
un composite CPU préparé en arrière-plan. Le dessin et la précision finale ne sont
pas transférés au GPU par cette étape.

Changer tolérance, contiguïté, lissage ou échantillonnage abandonne le travail en
cours. `pixelEditBusy` regroupe désormais les états de Pièce, Déplacement basé sur
le contenu, Suppression et Gomme magique pour protéger la fermeture, désactiver
la sauvegarde/l’export et permettre Annuler/Rétablir pendant une tâche. Les drapeaux
propres à chaque outil restent disponibles pour leurs contrôles et leur annulation.

La suite `magic_eraser_coverage` compare le résultat fusionné à l’effacement suivi
d’un mélange de couverture séparé : 8/16 bits, lissage activé/désactivé, zones
contiguës ou distinctes, opacité, échantillonnage global et canaux 16 bits hors des
paliers 8 bits. Elle vérifie aussi les réglages périmés, Annuler/Rétablir, l’absence
de modification et la sauvegarde. `canvas_render` contrôle le rendu transparent,
les pixels non correspondants et Annuler en logiciel, Metal et OpenGL.

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
  de **256 × 2 pixels** (2 Kio) à partir des fonctions CPU de référence.
  La première ligne contient la correction ou palette ; la seconde conserve les
  décisions binaires de Mélange maximal aux frontières complémentaires. Un shader du graphe de scène applique
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
  Les blocs de masque hors champ suivent le même cache de navigation que la photo.
  Leur version est vérifiée avant le retour à l’écran.
- Après un changement de contour progressif, les tuiles visibles sont comparées
  à leurs snapshots précédents, bordures comprises. `render/TileSnapshot` compare
  les octets utiles des lignes, sans inclure le remplissage d’alignement. Quand
  les pixels et métadonnées sont identiques, il conserve la copie indépendante
  et sa clé ; aucun nouveau bloc mémoire ni texture n’est créé pour cette tuile.
  Les autres tuiles sont recopiées et retransférées individuellement. Le nœud de
  rendu suit la clé de chaque masque plutôt qu’une révision globale.
  Les changements de niveau/dimensions reconstruisent la grille ; la suppression
  ou désactivation du masque retire ses anciennes textures et leur liaison au shader.
  Le proxy de masque et son flou sont encore calculés intégralement par le worker :
  cette étape réduit les copies de tuiles et les transferts, sans régionaliser ce calcul.
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
- Ces douze fusions (avec Mélange maximal) demandent des octets d’entrée exacts. Elles sont éligibles
  avec Courbes, Niveaux, Luminosité/Contraste, Exposition, Balance des couleurs,
  Isohélie, Lumière, Transfert de dégradé, Seuil et Négatif, sans atténuation du
  masque : masque absent, désactivé, uniforme blanc ou densité à zéro. L’opacité
  du calque peut rester réduite. Les autres réglages et les masques visibles restent
  CPU : une différence d’un canal peut changer fortement une teinte près du gris
  ou un rapport près d’un dénominateur nul. Ce repli ne change pas le comportement de l’outil.
- Mélange maximal reprend la décision binaire de Lumière vive. Hors frontière,
  la somme des canaux entiers suffit ; lorsque base + source vaut 255, une ligne
  de la table contient le résultat exact de la fonction CPU partagée `BlendMath.h`.
  Cela conserve ses décisions en double précision, y compris celles sensibles à
  l’arrondi flottant. Les formules du moteur natif restent inchangées.
- Fondu utilise le même hachage des coordonnées du proxy que le CPU :
  `(x * 73856093 XOR y * 19349663) & 65535`, comparé strictement au seuil
  entier `opacity * 65535`. Le calcul par octets reste exact en GLSL 100 sans
  opérateurs binaires. L’origine de chaque tuile et ses dimensions occupent
  quatre composantes inutilisées du bloc existant ; le motif reste continu
  lors du déplacement et du rechargement des tuiles.
  Fondu accepte les dix-huit réglages, avec couverture de masque complète.
- Fondu et Mélange maximal demandent aussi un affichage sans interpolation.
  Le canevas transmet son mode d’échantillonnage au moteur avant de préparer
  le snapshot. Un zoom interpolé replie ces deux modes sur le CPU ; revenir à
  l’échantillonnage au plus proche réactive leur aperçu GPU.
- Les fusions RVB reprennent les formules CPU. Le masque est appliqué avant la
  conversion des pixels prémultipliés en couleurs droites, puis le mélange utilise
  l’alpha du masque et l’opacité. Normal conserve les arrondis de son chemin QPainter
  distinct. Le mode de fusion est codé dans les bits supérieurs de l’entier du
  shader ; le bloc reste de 224 octets et aucun sampler n’est ajouté.
- Le survol des modes de fusion d’un réglage éligible ne change que cette constante.
  Les calques de pixels restent CPU. Les modes conditionnels utilisent les
  règles plus strictes décrites ci-dessus. Cela porte à 27 les modes éligibles
  pour certains aperçus de réglage.
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
  complète synchrone pour suivre l’aperçu. Une demande auxiliaire ne remplace
  jamais une base ou un masque GPU déjà en attente : elle est replanifiée après
  leur mise à jour, pour conserver le dernier contour progressif demandé.
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
retransfère aucune texture ; le contour ne remplace que les textures de masque
ayant des pixels différents. Un cas de
1 280 pixels de large vérifie la frontière à 1 024 pixels, l’éviction/relecture et
le niveau de détail réduit. Les tests moteur vérifient aussi les contours demandés
rapidement, l’annulation pendant le calcul et le redimensionnement, en 8/16 bits.
Les quatorze modes de fusion sont comparés sur les dix-huit réglages avec masque
peint, contour progressif et opacité réduite ; Niveaux est aussi vérifié à 100 %.
Les compteurs doivent rester stables pendant le changement de mode. Les tests
moteur couvrent chaque mode en 8/16 bits avec et sans masque peint, l’historique,
l’annulation, le repli de Fondu interpolé et le retour au GPU après ce repli.
Les quatre fusions HSL sont comparées à QColor sur chacun des dix réglages
éligibles, avec des rampes grises/primaires et des couleurs saturées ou atténuées.
Le changement de fusion ne doit retransférer ni la photo ni la table. Les tests
moteur couvrent les dix réglages en 8/16 bits, l’historique, l’annulation, le repli
avec masque gris, le retour au GPU à densité zéro ou masque désactivé, ainsi que
le repli de chacun des huit autres réglages pour ces douze modes. Ils sont regroupés
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
La suite `gpu_dissolve_preview` couvre les dix-huit réglages en 8/16 bits,
la conservation des textures, l’annulation et les replis liés au masque ou au zoom.
Le test natif compare Fondu avec six opacités sur Niveaux/Négatif et 37 % sur les
seize autres réglages ; il contrôle aussi les rares hachages 0/65534/65535.
Il parcourt les 256 frontières complémentaires de Mélange maximal et leurs
voisins, puis vérifie Fondu sur plusieurs tuiles et après leur rechargement.
Un masque localisé sur une base écran 1 280 × 1 280 vérifie aussi qu’un changement
de contour ne retransfère qu’une tuile sur quatre, ou deux quand il traverse la
frontière. Les compteurs de photo et de table doivent rester stables. Ces contrôles
sont exécutés depuis des documents 8/16 bits, avec densité, niveau réduit et retrait
du masque, et comparent les pixels à une composition QPainter indépendante.
La suite `tile_snapshot` contrôle les gris 8 bits, le RVB, le RGBA prémultiplié
8/16 bits, les modifications hors tuile, les derniers pixels/lignes et les changements
de métadonnées, format et dimensions. Les snapshots ne retiennent pas le plan source.
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

## Surimpressions de mesure bornées

L’anneau de pipette et la règle n’utilisent plus de Canvas de la taille entière de
l’image zoomée. Leur affichage est séparé dans `SamplingRing.qml` et
`RulerOverlay.qml`, sans modifier l’échantillonnage ni les mesures du document.

L’anneau garde une surface de 84 × 84 pixels logiques, avec les deux demi-cercles
(couleur échantillonnée et couleur précédente), les liserés et le centre transparent.
La surface reste fixe au zoom, indépendamment des dimensions du document. Sur le
backend logiciel, la position du pointeur ne change que `x`/`y` et réutilise sa
texture ; une nouvelle couleur ou la réapparition demande un repeint. Le shader
procédural décrit ci-dessous remplace cette texture sur les backends RHI.

La règle utilise les coordonnées natives des deux points, multipliées par le zoom.
Son rectangle de dessin est l’intersection de l’emprise du segment, avec une marge
pour les poignées, et de la vue calculée par `ScreenOverlayBounds`. Le dessin est
translaté vers cette origine locale. Une petite mesure garde donc une petite
emprise, et une mesure traversant tout le document reste limitée à l’écran, même
après défilement ou rotation. Le premier rendu borné était raster ; le chemin GPU
décrit ci-dessous supprime maintenant cette surface sur les backends compatibles.

`MeasurementOverlaySmoke.h`, exécuté par `measurement_overlay`, affiche ces composants sur
une géométrie de document 16 384 × 16 384 à 16×, sans allouer cette image complète.
Il compare leurs pixels, observe le signal réel `painted` de l’anneau pour vérifier
la réutilisation au déplacement, et contrôle la règle courte, longue, défilée,
tournée et masquée. Les mêmes scénarios sont exécutés sur le backend logiciel,
Metal et OpenGL. Le smoke test QML vérifie aussi leur branchement dans l’interface
règle/pipette, en conservant les curseurs natifs et le panneau Informations.

## Contour du pinceau borné

`BrushCursor.qml` remplace le Canvas du curseur dont la surface grandissait avec le
diamètre zoomé et qui était repeint à chaque mouvement. Le diamètre reste exprimé
en coordonnées d’affichage ; le dessin conserve l’ellipse, son angle, son
aplatissement et les deux traits contrastés existants. Les signes +/− de Sélection
rapide sont placés dans l’empreinte locale, sans subir la rotation ni l’aplatissement
de l’ellipse.

Le rectangle du curseur est limité à la vue, calculée dans le repère du document
après défilement et rotation. Cette vue inclut les marges autour de l’image : un
pinceau près du bord de la photo garde son contour hors image si cette portion
reste visible à l’écran. Une empreinte énorme ne réserve donc pas une texture
proportionnelle au carré de son diamètre zoomé.

Sur le backend logiciel, tant que le contour reste entièrement dans la vue, son
centre local et sa taille ne changent pas : déplacer le pointeur déplace seulement
la texture existante dans le graphe de scène. En cas de découpage, seule la surface
bornée est repeinte. Les changements de diamètre, angle, aplatissement, signe ou
visibilité demandent aussi un repeint. Le chemin GPU procédural décrit ci-dessous
supprime cette texture. La peinture des pixels du document et les curseurs natifs
restent sur leurs chemins existants.

Le test `brush_cursor` compare les pixels avec/sans découpage, vérifie le centre
transparent, l’ellipse tournée, les signes +/−, le défilement, le repositionnement
du document et le masquage. Le signal réel `painted` contrôle la réutilisation au
déplacement et à la rotation de la vue. Une empreinte de 32 000 pixels affichés
est vérifiée dans une vue 512 × 320, sur logiciel, Metal et OpenGL ; aucun document
complet de cette taille n’est alloué par ce test. Le smoke test QML contrôle aussi
la liaison au pointeur, au zoom et au mode de Sélection rapide de l’application.

## Plume : tracé conservé pendant le survol

`PenOverlay.qml` sépare le dessin de la courbe, le segment provisoire pointillé et
les ancres/poignées, dans le même ordre que le dessin combiné précédent. Le
snapshot QML du tracé actif dépend des notifications de chemins, sans dépendre
de la position de survol : déplacer la souris ne redemande pas la liste native
de tous les chemins.

La courbe et les poignées conservent leur dessin tant que leurs points ne changent
pas. Seul le Canvas du segment provisoire est repeint au survol. Chaque surface
est limitée à l’intersection de son enveloppe (points de contrôle inclus pour les
courbes, tangentes sélectionnées pour les poignées), avec marge de trait, et de
la vue. Les enveloppes sont conservatrices : elles ne coupent pas une courbe qui
sort du rectangle de ses seules ancres. Le premier cache séparé pouvait conserver
trois surfaces bornées au lieu du dessin combiné précédent, afin d’éviter de
repeindre les éléments fixes. Le chemin GPU décrit ci-dessous remplace maintenant
la surface de la courbe principale. Les couches QML restent séparées du composite
photo et du budget LRU de ses tuiles ; l’édition des points reste côté CPU.

Une modification des ancres, des tangentes, du chemin actif ou une annulation
invalide les dessins fixes. Zoom et défilement ajustent les surfaces visibles.
Fermer le chemin, terminer sa construction ou désactiver le segment provisoire
le masque ; le survol d’un chemin terminé ne provoque plus de repeint.

Le test `pen_overlay` conserve une copie indépendante des Canvas bornés de la
Plume, avant l’ajout du tracé GPU, et compare les pixels du dessin séparé en 8/16 bits, avec/sans guide, après édition
et annulation, aux différents zooms et après défilement. Les signaux `painted`
vérifient que le survol ne repeint ni courbe ni poignées et ne modifie pas le
snapshot actif ; pixels et historique du document restent intacts. La rotation
est aussi contrôlée pour les bornes des surfaces, ainsi que le guide visible
quand le tracé fixe est hors champ. Le smoke test QML déplace la
souris dans l’application pour vérifier le branchement réel et le masquage par
Échap. Ces scénarios de rendu sont vérifiés sur logiciel, Metal et OpenGL.

## Guides de lasso bornés et points conservés

`LassoGuide.qml` reprend les guides encore nécessaires en QML : ligne libre
initiale, polygonal en construction et candidat des sélections combinées. Le
snapshot de points dépend de `selectionChanged`, sans relire `selectionPath` au
survol ou à chaque phase de pointillés. Il est vidé quand le contour passe au
canevas partagé : le lasso libre déjà couvert par le GPU ne subit pas une nouvelle
conversion de points pour ce guide caché.

La surface est limitée à l’intersection de l’enveloppe du guide, pointeur compris
pour le polygonal, et de la vue. La marge couvre les traits, leurs joints en onglet
et les poignées. Le repli Canvas dessine le guide en une seule passe pour conserver
le même anticrénelage aux joints et aux croisements. Sur ce repli, le déplacement
du pointeur redessine la surface bornée avec les points déjà en cache. Les guides
unis éligibles utilisent désormais le chemin GPU décrit plus bas.

L’animation commune des fourmis ne tourne plus sans sélection effective. Le guide
uni ignore aussi les changements de phase directs : un polygonal immobile sans
sélection ne repeint plus sa surface en continu. Les candidats combinés, dont la
transition Alt du lasso libre vers le polygonal, gardent le dessin fermé et les
pointillés complets ; leurs points restent en cache entre changements du candidat.
Le moteur de sélection reste commun aux chemins GPU et raster.

Le test `lasso_guide` compare aux pixels de l’ancien Canvas en 8/16 bits, y compris
les joints, les transitions entre guides, la sortie du pointeur, le zoom et le
défilement, l’ajout/retrait de points, les modes combinés, Alt, le masquage et
l’annulation. Les signaux `painted` et de snapshot contrôlent les repeints évités
et la réutilisation des points. Le smoke test QML vérifie aussi le survol réel,
l’absence de repeint au repos et l’arrêt de l’animation sur un polygonal uni.
Les scénarios de rendu sont vérifiés sur logiciel, Metal et OpenGL.

## Tranches : descriptions partagées et dessin des éléments visibles

`tools/SliceDescriptionCache` conserve une seule liste de descriptions par document,
avec pour clé les dimensions, les rectangles manuels et l’option des tranches
automatiques. Les données sont identiques à celles de l’ancien getter : tranches
manuelles dans leur ordre, puis complément automatique, jusqu’à 1 024 descriptions
au total avec la limite actuelle de 256 tranches manuelles.

`Document::slices()` retourne une copie partagée de cette liste. Un changement de
tranche active, un état occupé ou un rectangle provisoire ne change pas la clé et
ne reconstruit pas le complément géométrique. Une édition validée, une division,
Annuler/Rétablir, un changement de dimensions ou l’ouverture d’un projet provoquent
un nouveau calcul si la clé diffère. Le premier calcul reste CPU sur le thread
GUI ; le cache ne contient aucun pixel et ne modifie pas le format de projet.

`SliceOverlay.qml` conserve le snapshot, la liste manuelle pour le test du curseur
et la liste visible pour le dessin. Le rejet des tranches hors champ inclut une
marge pour les poignées et les étiquettes, qui peuvent dépasser les rectangles
très étroits. L’ordre de dessin et les indices sont conservés. Les contrôles de
`SliceOptions.qml` partagent aussi un snapshot entre leurs champs et leurs boutons.
Un changement de couleur ou d’opacité de brosse ne repeint plus cette surimpression ;
le passage Tranche/Sélection de tranche réactualise bien les poignées.

Le repli Canvas reste limité à la vue et parcourt seulement la liste visible.
Le cadre provisoire et, dans l’étape après v0.16 décrite plus bas, les cadres fixes
et poignées utilisent le graphe de scène. Les images du document gardent leur
moteur de rendu natif.

`slice_cache` vérifie la couverture automatique, l’ordre, le partage du buffer,
les clés, les gestes provisoires, Annuler/Rétablir et l’ouverture de projets. Une
partition très fragmentée vérifie aussi le plafond de 1 024 descriptions.
`slice_overlay` compare à l’ancien dessin en 8/16 bits avec 145 tranches manuelles,
les automatiques, les étiquettes débordantes, les poignées, les curseurs, les
gestes provisoires, le zoom, le défilement et la rotation. Les scénarios de rendu
sont exécutés sur logiciel, Metal et OpenGL. Le smoke test QML vérifie aussi les
contrôles de tranche réellement affichés dans l’application.

## Recadrage : voile et guides GPU

`CropOverlay.qml` partage un snapshot des quatre points entre le test du curseur,
le dessin des guides et le voile natif `CropShadeItem`. Le survol n’appelle plus
le getter `Document::cropPoints()` à chaque déplacement. Les points sont convertis
en `QPolygonF` sur le thread GUI ; le thread de rendu ne consulte aucun objet
JavaScript.

Sur un backend RHI, `CropShadeNode` conserve un rectangle de quatre sommets couvrant
la partie visible du document. Son shader produit le voile noir d’opacité 55 %
à l’extérieur du quadrilatère, sans texture intermédiaire. Les quatre plans de
bord sont normalisés sur CPU en double précision dans les coordonnées locales
de la vue, puis transmis comme constantes en flottants. Un cadre déplacé change
ces paramètres et la matrice, sans recharger les sommets si la vue ne change pas.
Une ouverture dégénérée produit un voile plein, comme au début d’un geste.

Les coordonnées locales du fragment utilisent un attribut distinct de la position
du sommet : Qt peut prétransformer cette dernière lors du regroupement des nœuds
ou du découpage d’une vue tournée. La transformation et le clipping restent ceux
du graphe de scène. Même si les guides sont entièrement hors champ, le voile
reste visible dans la vue par son nœud enfant.

Le cadre et la grille utilisent désormais quatre `ShapePath` dans un unique
`Shape` avec `CurveRenderer` : contour sombre puis clair, grille sombre puis claire.
Les quatre ou huit poignées sont des rectangles du graphe de scène. Le `Loader`
matériel ne crée aucun Canvas ni image de guides. Les chemins sont préparés sur
CPU par Qt (leur géométrie est petite et bornée en nombre de segments), puis
tracés sur GPU avec anticrénelage analytique ; cette étape ne supprime donc pas
le coût de préparation des chemins, ni le calcul CPU du recadrage final.

Les coordonnées des guides sont locales à l’enveloppe visible du quadrilatère,
avec une marge de 16 pixels logiques. Les poignées sont conservées au déplacement,
avec un nombre stable de délégués. Les guides entièrement hors champ ne sont pas
créés. La vue conserve le clipping et la rotation du graphe de scène.

Le backend logiciel conserve le Canvas complet d’origine pour le voile et les
guides. Les kits sans `preferredRendererType` (Qt 6.5) gardent également les guides
raster bornés, avec le voile natif sur RHI. Les propriétés introduites en Qt 6.6
sont consultées dynamiquement, sans imposer une nouvelle version minimale.
Le module QML `QtQuick.Shapes` fait partie des dépendances à déployer.
Le recadrage final reste asynchrone sur CPU en précision native 8/16 bits.

`crop_shade` vérifie les plans, le sens du quadrilatère, les ouvertures invalides,
les grandes coordonnées et les origines de vue. `crop_overlay` compare les pixels
à l’ancien Canvas sur documents 8/16 bits : recadrage classique et perspective,
zoom, défilement, rotation, grilles, fonds clairs/colorés, cadre hors champ,
début de geste et annulation. Des compteurs contrôlent la conservation des sommets
et la mise à jour des seules constantes au déplacement. Ces scénarios sont
exécutés sur logiciel, Metal et OpenGL ; les autres backends RHI restent à valider
sur leur matériel. Le smoke test QML contrôle aussi le curseur réel du recadrage
et l’absence de repeint/snapshot pendant son survol.

Les tests des guides matériels contrôlent l’absence de Canvas et de signal
`painted` pendant le déplacement, ainsi que le renderer demandé. Les comparaisons
au Canvas restent strictes sur logiciel (4/255 par canal). Sur GPU, les traits
analytiques sont lissés une fois, tandis que l’ancien Canvas est lissé puis filtré
comme texture sous rotation. Les pixels différents sont donc comparés après un
même filtre moyen de 5 × 5 pixels physiques, avec une tolérance de 12/255 par canal.
Ce contrôle porte sur l’aspect et le placement des guides ; il ne revendique pas
une identité d’anticrénelage avec QPainter. Les pixels et l’historique de l’image
restent comparés sans cette tolérance. Les compteurs vérifient aussi que le repli
raster repeint effectivement ses guides lors d’un déplacement.

Références Qt : [Shape](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html),
[ShapePath](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shapepath.html).

## Règle : trait et repères conservés sur GPU

Sur RHI avec CurveRenderer, `RulerOverlay.qml` ne crée plus de Canvas. Son trait
sombre de 4 pixels et clair de 2 pixels sont deux chemins dans `RulerGuides.qml` ;
les repères sont deux cercles de rayon 4, remplis et bordés, dans une seconde forme
pour conserver leur ordre au-dessus du segment. Changer les extrémités actualise
les petits chemins et leur géométrie, sans image intermédiaire à repeindre ou à
transférer. La préparation de ces chemins reste effectuée par Qt sur CPU.

`GpuGuideShape.qml` partage avec le recadrage la sélection dynamique de
CurveRenderer et le repli Qt 6.5. `ScreenOverlayBounds.qml` partage leurs calculs
de vue, en tenant compte du défilement, du zoom, de la rotation et de la position
de l’artboard. Le backend logiciel et les kits sans CurveRenderer gardent l’ancien
dessin Canvas limité à la vue. Les coordonnées et les lectures du panneau
Informations restent inchangées ; aucun pixel ni historique du document n’est
modifié par cet affichage.

`measurement_overlay` conserve l’ancien Canvas comme référence indépendante.
Les comparaisons utilisent les mêmes tolérances de lissage que le recadrage via
`OverlayReference.h` : logiciel strict, contrôle filtré sur GPU. Elles couvrent
une mesure courte, un changement d’extrémité, les zooms 0,5×/1×/4×/16×, le
scrolling, la rotation, les points inversés ou confondus, une règle traversant un
document 16K et son masquage. Les compteurs vérifient zéro signal `painted` sur
GPU pendant ces mises à jour, et des repeints effectifs sur le repli logiciel.
Le test du recadrage reste exécuté pour vérifier les composants communs.

## Anneau de pipette procédural sur GPU

`SamplingRing.qml` utilise un `ShaderEffect` sur Metal, Vulkan, OpenGL et Direct3D.
Les shaders `samplingring.vert` et `samplingring.frag` dessinent les deux moitiés
colorées et leurs liserés dans un simple rectangle, sans Canvas, texture source
ni échantillonnage d’image dans le shader. Le prélèvement des pixels du document
reste inchangé ; seule sa représentation est accélérée.

Le déplacement change la transformation du rectangle. Une nouvelle couleur ne
change que les paramètres du shader : couleur courante, couleur précédente et
dimensions du rectangle. Les coordonnées locales issues des UV restent correctes
lorsque Qt regroupe les dessins ou applique une rotation. Les rayons de 35, 41
et 29 pixels logiques et la bande colorée de 10 pixels restent indépendants du
zoom ; les dérivées écran lissent les contours. Les couleurs reçues par
`ShaderEffect` sont déjà prémultipliées : les deux moitiés sont interpolées en
une seule bande et l’opacité héritée est appliquée une seule fois après la
composition des liserés. Le centre et les pixels extérieurs restent transparents.

Le backend logiciel garde le Canvas borné de 84 × 84. Une erreur de chargement du
shader réactive aussi ce repli. Le test exige néanmoins un shader compilé et
l’absence de Canvas sur les backends matériels, pour ne pas masquer une régression.

`measurement_overlay` compare le rendu à une copie indépendante de l’ancien
Canvas : couleurs opaques et transparentes, alpha partiel, opacité héritée,
déplacement, dimensions non carrées, masquage avec changement de couleur et rotation.
Il contrôle aussi les couleurs au centre des bandes avec une tolérance de deux
niveaux et les compteurs de repeint : aucun repeint raster sur GPU, y compris
lorsque les couleurs changent. La comparaison des contours reprend les limites
explicites de lissage du test des guides, sans élargir leur tolérance.

Référence : [Qt ShaderEffect — attributs, paramètres prémultipliés et shaders QSB](https://doc.qt.io/qt-6.8/qml-qtquick-shadereffect.html).

## Contour de pinceau procédural sur GPU

Sur Metal, Vulkan, OpenGL et Direct3D, `BrushCursor.qml` utilise un `ShaderEffect`
avec `brushcursor.vert` et `brushcursor.frag`. Un rectangle limité à la vue remplace
le Canvas et sa texture, y compris lorsque le curseur est découpé par un bord.
Le déplacement actualise sa transformation, son centre local et ses dimensions ;
les changements de diamètre, aplatissement, angle ou signe ne changent que les
paramètres du shader. Une empreinte de 32 000 pixels reste limitée à la vue, sans
surface raster de cette taille ni transfert de texture à chaque déplacement.

Le shader ramène chaque fragment dans le repère du cercle avant la rotation et
l’aplatissement, puis calcule les deux traits contrastés. Il conserve les largeurs
anisotropes de l’ancien Canvas : 2/ratio et 1/ratio avant la transformation.
Les dérivées écran règlent le lissage des contours. Les signes +/− gardent leurs
traits à bouts droits, leur place dans l’empreinte et leur orientation dans le
repère du document, indépendamment de l’angle du pinceau. Les couches sont
composées en couleurs prémultipliées avant l’application de l’opacité héritée.

Le backend logiciel conserve le Canvas borné, également utilisé en cas d’erreur
de chargement du shader. Le test `brush_cursor` exige un shader compilé et aucun
Canvas sur le chemin matériel, puis compare les pixels à une copie indépendante
de l’ancien composant : cercle, ellipse tournée, rotation de la vue, opacité,
signes +/−, curseur partiellement visible, déplacement découpé, très grand diamètre,
défilement, marges autour de l’image, aplatissement minimal, diamètre d’un pixel,
position fractionnaire et réapparition après modification des réglages masqués. Les tolérances de lissage restent
celles du comparateur commun des surimpressions ; les couleurs et le centre vide
sont aussi vérifiés directement. Le compteur `painted` reste nul sur GPU lors des
changements de forme et du déplacement d’une empreinte découpée.

L’échantillonnage du pinceau, la peinture sur calques et masques, l’historique et
les curseurs natifs ne sont pas modifiés par cette étape.

## Tracé principal de la Plume conservé sur GPU

`PenOverlay.qml` utilise `PenCurve.qml` et le composant commun `GpuGuideShape`
pour ses deux traits principaux : contour sombre de 3 pixels logiques et trait
bleu de 1,5 pixel, sans remplissage. Le moteur CurveRenderer affiche ces chemins
sans Canvas ni texture intermédiaire. Le segment provisoire possède son propre
chemin GPU avec un repli Canvas, au-dessus du tracé. Les ancres et tangentes utilisent
la couche GPU décrite plus bas, avec un repli Canvas ; leur ordre de dessin et
leurs règles d’actualisation sont conservés.

La conversion native des points reste conservée entre notifications du document.
Sur le chemin matériel, une description SVG du tracé dépend des points, du zoom
et de l’origine découpée, sans dépendre de la position de survol. Le backend
logiciel n’effectue pas cette préparation SVG. Le survol conserve donc la même description
et le même objet graphique. Un changement d’ancre ou de tangente réactualise les
courbes GPU sans repeindre de surface raster. La préparation géométrique reste
sur le CPU ; cette étape supprime la rasterisation et les transferts de sa texture.

CurveRenderer approxime les cubiques en quadratiques. Sa conversion automatique
créait un décalage observable dans la comparaison à l’ancien Canvas ; le panneau
prépare donc ses propres quadratiques avant le rendu. Pour une cubique de contrôles
A, B, C, D, la quadratique passant par les mêmes extrémités et le même milieu a
pour contrôle `(3*(B+C)-A-D)/4`. Son erreur maximale de position vaut
`|D-3*C+3*B-A|/(12*sqrt(3))`. La subdivision de De Casteljau à mi-paramètre est
répétée jusqu’à obtenir une erreur au plus égale à 0,01 pixel logique, après
application du zoom. La profondeur est limitée à 12 par sécurité ; les coordonnées
acceptées par le document et les zooms de l’interface atteignent le seuil avant
cette limite. Les extrémités restent exactes, y compris au retour vers la première
ancre d’un chemin fermé, sans ajouter de fermeture absente du dessin précédent.

L’emprise du chemin garde son enveloppe de contrôle, intersectée avec les limites
de la vue et de l’image via `ScreenOverlayBounds`. Le conteneur GPU découpe
explicitement le dessin à cette emprise. Les coordonnées locales évitent un grand
rectangle de dessin au zoom fort. Ce découpage ne s’applique pas aux enfants
poignées/segment provisoire : le guide peut rester visible lorsque le tracé fixe
est hors champ. Le backend logiciel et les versions Qt sans CurveRenderer gardent
le dessin Canvas antérieur, avec le même trait et les mêmes points de contrôle.

`pen_overlay` compare le rendu à une copie indépendante des Canvas bornés de la
version précédente, y compris leurs origines sous rotation, en 8 et 16 bits :
édition, Annuler, fermeture, masquage/réactivation, ancre seule, segments droits,
angles et croisements, guide passant sur le tracé, zooms de 0,5× à 64×, défilement
fractionnaire et rotation. Le grand zoom produit une géométrie de 32 768 pixels
sans surface raster de cette taille. Le test exige le véritable CurveRenderer
sur les backends compatibles et l’absence de Canvas pour la courbe principale ;
les compteurs de repeint, de description SVG et de snapshot vérifient le cache au
survol. Le comparateur commun conserve ses tolérances de lissage ; aucun seuil
n’a été élargi pour masquer la différence de courbure. Les pixels et l’historique
du document restent sur leurs chemins existants.

Références Qt : [Shape et moteurs de rendu](https://doc.qt.io/qt-6.12/qml-qtquick-shapes-shape.html),
[ShapePath](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shapepath.html) et
[PathSvg](https://doc.qt.io/qt-6.8/qml-qtquick-pathsvg.html).

## Tranches : cadres et étiquettes conservés

`SliceDraft.qml` dessine le rectangle blanc de 2 pixels logiques du geste en cours
avec CurveRenderer, indépendamment des tranches validées. Ce chemin est livré
dans v0.16. Le snapshot partagé avec `SliceOptions` repose sur la révision monotone
`sliceDescriptionRevision` : seuls les changements de dimensions, de rectangles
validés ou d’affichage automatique reconstruisent les descriptions.

L’étape en développement après v0.16 remplace le grand Canvas des tranches fixes
par `SliceGuides.qml` : cadres et poignées sont des chemins GPU, les fonds des
étiquettes des rectangles conservés. L’ordre par tranche reste cadre, fond, numéro,
puis chacune des huit poignées, y compris lorsque celles-ci se superposent.
Le modèle visible est réutilisé quand les mêmes indices restent dans la vue à
révision identique. Un petit défilement déplace donc les objets existants sans
reconstruire le Repeater ni recalculer les descriptions natives.

Les chiffres gardent `Context2D.fillText` avec la police `11px sans-serif`, dans des Canvas de
**49 × 25 pixels logiques**. Ce sont de petits caches raster, pas des glyphes vectoriels.
Leur origine est alignée sur la grille physique du Canvas historique et leur phase
fractionnaire est intégrée au dessin. À phase et numéro constants, déplacer la
vue ne repeint pas ces caches ; zoom ou changement de résolution d’écran les
réactualisent. Cette solution conserve les formes de chiffres du rendu historique,
que le rendu natif de `Text` et les contours typographiques GPU ne reproduisaient
pas à l’identique.

Le chemin fixe GPU est utilisé avec CurveRenderer, à échelle d’écran entière,
pour **128 descriptions visibles au maximum**. Les autres cas utilisent un Canvas
limité à la vue, créé par Loader ; le retour GPU détruit ce grand tampon. Le
masquage des outils détruit également les petits caches de chiffres ; une vue
sans tranche visible ni cadre provisoire ne crée aucune surface raster. Le cadre
provisoire conserve son chemin GPU quand il est disponible, même si les tranches
fixes sont en repli. Une couche de composition bornée applique l’opacité après
la superposition de tous les éléments, y compris dans ce cas mixte.

`slice_overlay` compare au dessin historique en 8/16 bits : création, déplacement,
redimensionnement, annulation, validation, zoom jusqu’à 64×, rotation, étiquettes
hors rectangle et tranches automatiques. Des tranches de 2 × 3 pixels et leurs
poignées superposées vérifient l’ordre des aplats et traits, à opacité partielle.
Les compteurs de `painted`, les identités des objets et le modèle visible contrôlent
la réutilisation pendant un petit défilement et les gestes provisoires. Une grille
très dense vérifie le repli et la destruction des rangées GPU ; le retour à une
vue peu dense et le masquage vérifient la libération des surfaces raster.

Les contrôles sont exécutés sur logiciel, Metal 1×/2×, OpenGL et à échelle d’écran
fractionnaire. `slice_cache` garde les contrôles de partage des descriptions,
d’historique et d’ouverture de projet ; le smoke test QML couvre les outils réels.

## Panneau Histogramme dessiné sur GPU

Le panneau droit utilise `HistogramChart.qml` et `HistogramGraph.qml` au lieu de
son Canvas intégré à `Main.qml`. Les trois surfaces RVB et le trait de luminance
sont des chemins de 256 valeurs, affichés par le composant commun CurveRenderer.
Les aplats gardent l’ordre rouge, vert puis bleu, leurs couleurs et leurs alphas
respectifs ; la luminance est tracée par-dessus avec une largeur de 1 pixel logique.
Le fond reste `#14191f`, les abscisses vont de 0 à largeur pour les 256 classes et
la hauteur utile conserve la marge supérieure de 4 pixels du dessin précédent.

La luminance utilise un contour préparé par `HistogramGeometry` avec
`QPainterPathStroker` : largeur 1, extrémités plates, jointures en pointe et limite
10, puis remplissage GPU avec la règle d’enroulement. Cette préparation géométrique
CPU conserve les pointes étroites que le trait direct de CurveRenderer écourtait.
Elle ne produit aucune image raster. Les données et les dimensions invalides sont
ignorées ; le contour reste en cache dans la liaison QML entre deux changements.

Le fond et les quatre chemins sont composés dans une petite couche graphique
`layer.enabled`, à la taille du graphique. Ce tampon GPU garde le résultat opaque
avant l’application d’une éventuelle opacité héritée du panneau. Il évite que les
alphas des trois canaux soient réappliqués séparément lorsque le panneau est
atténué. Le conteneur Canvas reste présent pour le repli logiciel, mais le chemin
matériel n’y crée aucun contexte 2D ni aucune image raster. Il conserve une texture
intermédiaire et un passage de composition sur GPU. Cette ressource
Qt est distincte du budget LRU des tuiles photo ; elle est bornée au graphique,
réside sur GPU et reste réutilisée tant que son contenu ne change pas.

Pour les tailles fractionnaires, la surface logique est arrondie puis remise à
l’échelle aux dimensions du composant, comme la fenêtre de pixels du Canvas.
Les points restent calculés depuis les dimensions fractionnaires d’origine.
Ce traitement conserve l’alignement des classes et de la ligne de base sans
élargir la tolérance du comparateur de rendu.

Une nouvelle série de statistiques ou un redimensionnement réactualisent les
chemins. Le déplacement du panneau conserve les chemins et leur objet graphique ;
le masquage décharge le composant et sa réouverture utilise les dernières données.
Le backend logiciel et les versions Qt sans CurveRenderer utilisent une copie du
Canvas antérieur. L’identifiant d’objet historique `colorHistogramCanvas` reste
celui du composant public, pour les transitions ancré/flottant existantes.

Le calcul des statistiques `Document::colorHistogram()` et ses proxys CPU ne sont
pas modifiés. Cette étape concerne le panneau Histogramme ; les graphiques des
éditeurs Courbes, Niveaux et Seuil gardent leurs composants actuels.

Le test `histogram_render` utilise une copie indépendante du dessin Canvas comme
référence : superposition RVB vérifiée par calcul source-over, luminance, données
variables et incomplètes, pics isolés et classes extrêmes, déplacements, tailles
entières/fractionnaires, opacité, rotation, données modifiées pendant le masquage
et passage vers une seconde fenêtre puis retour. Il exige le véritable rendu GPU
sur les backends compatibles, sans contexte Canvas actif sur ce chemin, et observe
les repeints logiciels. Les comparaisons reprennent les tolérances du comparateur
commun des surimpressions. Le smoke test général conserve ses contrôles de mise en onglets,
masquage et transitions du panneau Histogramme.

Références Qt : [Shape](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html),
[couches graphiques et opacité](https://doc.qt.io/qt-6.8/qml-qtquick-item.html#item-layers)
et [Canvas](https://doc.qt.io/qt-6.8/qml-qtquick-canvas.html).
Préparation du contour : [QPainterPathStroker](https://doc.qt.io/qt-6.8/qpainterpathstroker.html).

## Règles du canevas : éléments conservés et graduations visibles

`ViewportRuler.qml` remplace les deux boucles Canvas intégrées à `Main.qml`.
Les dimensions du document, le zoom et la position de l’image relativement au
viewport sont des dépendances explicites. Les changements de zoom, de taille et
de défilement actualisent ainsi les deux règles sans les notifications manuelles
précédentes ; une modification des pixels à dimensions constantes ne les repeint
plus inutilement.

Le pas garde les seuils historiques : 500 en dessous de 0,3×, 200 en dessous de
0,65×, 100 en dessous de 1,5×, puis 50 pixels natifs. Deux indices calculés depuis
l’origine et la longueur visible bornent la liste des graduations. Une marge
conservatrice tenant compte du nombre de chiffres garde les étiquettes venant
d’un trait hors champ, notamment les chiffres verticaux orientés à −90°. La borne
du document et la graduation zéro restent incluses comme auparavant.

Sur RHI, lorsque les dimensions de la bande sont entières, les graduations
utilisent des rectangles de 1 × 7 pixels logiques et les chiffres des contours
CurveRenderer conservés. `CanvasFonts` résout la famille générique sans-serif
avec `QFont::SansSerif` et sa famille par défaut, comme le parseur CSS du Canvas ;
il ne fixe pas une famille particulière à macOS. Les contours sont préparés avec
`QPainterPath::addText`, comme `Context2D.fillText`, et leur cache LRU est borné à
128 étiquettes. Le déplacement d'une graduation ne recalcule pas sa police.
Traits et contours se déplacent dans le graphe de scène, sans repeindre ni
retransférer une image CPU de la règle. Une petite couche GPU limitée à la bande
compose le fond et les chiffres avant l'application de l'opacité héritée.

Le backend logiciel, Qt sans CurveRenderer et les bandes de dimensions
fractionnaires gardent le Canvas d'origine, mais sa boucle est elle aussi bornée
aux indices utiles. Ce dernier repli conserve la projection et l'échantillonnage
fractionnaires du Canvas : utiliser directement des éléments natifs déplacerait
légèrement les chiffres et le dernier bord. Le chemin matériel éligible n'ouvre
pas de contexte Canvas ; un contexte déjà créé par le repli peut rester alloué.
Le repli efface aussi le bord entier du tampon avant chaque dessin : le
`reset()` limité au rectangle fractionnaire pouvait laisser une couverture
résiduelle dépendante du nombre de repeints. Les surfaces Qt et le cache des
contours ne font pas partie du LRU des tuiles photo.

`viewport_ruler` compare aux deux boucles historiques indépendantes : chiffres,
traits, orientation verticale, défilement fractionnaire, seuils de zoom, tailles
fractionnaires, opacité, origine d’un document vide, document hors champ et reprise
après masquage. Un cas de coordonnées jusqu’à un milliard vérifie directement que
le nombre d’éléments reste lié à la vue ; il évite volontairement d’exécuter les
deux millions de passages de l’ancienne boucle de référence. Le test contrôle
les vrais signaux de repeint et les objets affichés. Le smoke test général vérifie
les liaisons des deux règles après ouverture d’une image, zoom et déplacement à
la molette. Le test Histogramme est également réexécuté pour contrôler le panneau
précédemment optimisé.

Références : [QPainterPath::addText](https://doc.qt.io/qt-6.8/qpainterpath.html#addText),
[Shape](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html) et
[implémentation Qt de Context2D](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/items/context2d/qquickcontext2d.cpp).


## Graphique du panneau Seuil conservé sur GPU

`ThresholdHistogram.qml` sépare le dessin des interactions de `ThresholdEditor`.
Sur RHI et pour les dimensions entières, `ThresholdGraph.qml` conserve les
chemins de la grille et des 256 classes de l'histogramme dans le graphe de scène.
Le chemin de l'histogramme dépend de ses valeurs et de la taille du graphique ;
il ne dépend pas du seuil. Déplacer celui-ci actualise les deux rectangles de la
bande noir/blanc et la position de la ligne et du triangle. Les couleurs, marges,
hauteurs, ordre de dessin et coordonnées des contrôles restent ceux du panneau
précédent. Les chemins sont dessinés avec CurveRenderer ; une couche GPU limitée
au graphique applique l'opacité après composition des éléments.

Le chemin éligible n'ouvre pas de contexte Canvas et n'effectue pas de repeint
raster du graphique pendant le geste. Le repli conserve le dessin Canvas pour
le backend logiciel, Qt sans CurveRenderer et les dimensions fractionnaires,
avec effacement complet du bord du tampon pour éviter les résidus de couverture.
Un contexte déjà créé par ce repli peut rester alloué. La mise en forme des
chemins reste sur CPU lorsque les données ou la taille changent ; cette étape
n'ajoute aucun calcul de statistiques ou filtre photo sur GPU. Les statistiques,
le rendu de l'image et l'historique suivent leurs chemins existants.

Le test `threshold_graph` compare le graphique à une copie indépendante de
l'ancien dessin, avec nettoyage du tampon de référence entre images. Il couvre
les seuils 0/1/128/254/255, les pics étroits, les données absentes ou de mauvaise
longueur, le redimensionnement, la rotation, l'opacité, les dimensions
fractionnaires et les modifications pendant le masquage. Des signaux et
identités d'objets vérifient séparément que la grille et l'histogramme ne changent
pas au déplacement du seuil et qu'aucun Canvas n'est peint sur le chemin GPU.
Les comparaisons gardent les tolérances communes des surimpressions, sans masquer
des zones du graphique. Le smoke test d'interface ajoute un vrai glisser : capture
conservée, accord entre calque, graphique et champ numérique, puis retour à la
valeur initiale avec un seul Annuler.

Références Qt : [Shape](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html)
et [composition des couches](https://doc.qt.io/qt-6.8/qml-qtquick-item.html#item-layers).


## Graphiques d'entrée et de sortie du panneau Niveaux sur GPU

`LevelsChart.qml` garde les fonctions de coordonnées et le repli Canvas, tandis
que `LevelsGraph.qml` conserve les éléments natifs des deux graphiques.
`LevelsEditor` continue de gérer le choix du triangle le plus proche, la capture
de souris, les contraintes entre points noir/blanc, la conversion du gamma et
les signaux de prévisualisation/validation. Les paramètres et le filtre restent
inchangés.

Sur RHI avec CurveRenderer et des dimensions entières, la grille, le cadre et
l'histogramme d'entrée restent dans le graphe de scène. Leurs chemins ne dépendent
pas directement des valeurs noir, gamma ou blanc. Les nouvelles statistiques
fournies par le document continuent toutefois d'actualiser l'histogramme : le
cache ne bloque pas ces changements. Les trois triangles sont des chemins fixes
remplis et bordés ; seule leur position change. En sortie, le dégradé noir–blanc
est un rectangle à gradient horizontal indépendant des valeurs ; les deux
triangles suivent les niveaux de sortie, y compris quand ils se superposent.
L'ordre noir/gris/blanc et noir/blanc conserve celui du dessin précédent.

Chaque graphique utilise une petite couche GPU limitée à ses dimensions pour
appliquer l'opacité après composition, avec filtrage linéaire pour les
transformations. Aucun contexte Canvas n'est ouvert lors d'une première
activation éligible, et les gestes n'y provoquent pas de repeints raster. Le
repli logiciel/Qt ancien/dimensions fractionnaires garde la projection du Canvas,
avec nettoyage entier du bord du tampon. Lors du retour au GPU, un tampon raster
déjà créé est vidé une fois : la sortie est transparente et ne doit pas laisser
les anciens triangles apparaître sous les nouveaux. Ce contexte et sa texture
vide peuvent rester alloués ; ce n'est pas un nouvel upload pendant chaque geste.

`levels_graph` compare les deux graphiques à des copies indépendantes des
anciens dessins, en logiciel, Metal et OpenGL. Il couvre les extrêmes du gamma,
les marqueurs coïncidents, une plage d'entrée d'un pixel, les pics étroits, les
histogrammes absents/invalides, les changements de taille, rotation, opacité,
dimensions fractionnaires et données modifiées pendant le masquage, sans
repeints raster des graphiques cachés. Le retour
fractionnaire→entier est testé avec les mêmes dimensions arrondies de tampon,
pour révéler les résidus transparents. Les signaux de géométrie, identités
d'éléments et signaux de repeint vérifient séparément que le fond de l'histogramme
et le dégradé sont réutilisés pendant les gestes. Les tolérances de comparaison
restent celles du comparateur commun, sans zone masquée.

Le smoke test général exerce séparément les cinq poignées dans le vrai dialogue :
noir/gamma/blanc en entrée et noir/blanc en sortie. Il contrôle la capture de
souris, les valeurs enregistrées dans le calque, la synchronisation des graphiques
et le retour à la valeur initiale avec une seule annulation par geste. Le calcul
des statistiques et le rendu de l'image suivent leurs chemins existants ; cette
étape optimise l'affichage des contrôles.

Références Qt : [Shape](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html),
[Gradient](https://doc.qt.io/qt-6.8/qml-qtquick-gradient.html) et
[couches graphiques](https://doc.qt.io/qt-6.8/qml-qtquick-item.html#item-layers).


## Graphique Courbes et interpolation mise en cache

`CurvesEditor` conserve la gestion des points et des quatre pipettes, tandis que
`CurvesChart.qml` porte le dessin et les conversions entre valeurs et pixels.
`CurveInterpolation.js` reprend l'arithmétique précédente : pentes harmoniques,
pentes nulles aux changements de signe, règles d'extrémité et polynôme d'Hermite
sont inchangés. Un snapshot associe la liste des points à ses pentes ; les 256
échantillons dépendent uniquement de ce snapshot. Une nouvelle liste de points
reconstruit ainsi une fois les pentes et une fois la table, au lieu de préparer
les pentes séparément pour chacun des 256 échantillons à chaque dessin. La
pipette couleur utilise le même évaluateur pour placer son nouveau point.

L'histogramme, la taille du graphique et le point actif ne réévaluent pas cette
table. Les statistiques reçues du document continuent d'actualiser normalement
l'histogramme. Les pentes, la table et la préparation de géométrie restent des
calculs CPU ; le traitement photo et les mécanismes de preview restent inchangés.

Sur RHI avec CurveRenderer et des dimensions entières, `CurvesGraph.qml`
conserve des éléments distincts pour la grille, l'histogramme, la diagonale, le
trait de transfert et les poignées. `CurvesGeometry` prépare le contour du trait
à partir des 256 valeurs avec `QPainterPathStroker` : largeur 2, bouts plats,
jonctions et limite de miter identiques au Canvas. Le GPU remplit ensuite ce
contour ; ce n'est pas une nouvelle image raster. Une modification des points
réactualise le trait et les poignées, sans changer les chemins de grille et
d'histogramme si leurs données restent identiques. Sélectionner un point sans
modifier la liste change seulement les poignées concernées.

La couche GPU est limitée au graphique, utilise un filtrage linéaire pour les
transformations et demande quatre échantillons MSAA. Cela conserve notamment
la couverture des traits presque verticaux et des pics étroits ; Qt peut adapter
le nombre d'échantillons aux capacités du backend. La surface multisample et sa
résolution consomment de la mémoire graphique supplémentaire hors du budget LRU
des tuiles photo. Aucun contexte Canvas n'est ouvert lors d'une première
activation éligible. Le logiciel, Qt sans CurveRenderer et les dimensions
fractionnaires gardent le Canvas et la table mise en cache ; les mises à jour
masquées ne repeignent pas. Un ancien tampon raster est vidé au retour GPU et
peut ensuite rester alloué.

`curves_graph` compare indépendamment les valeurs à l'ancien évaluateur, pour les
256 entrées entières et plusieurs entrées fractionnaires, avec un écart maximal
de 1e-9. Des compteurs de signaux vérifient une reconstruction par nouvelle liste
et aucune pour les changements de point actif, d'histogramme ou de taille. Les
comparaisons de pixels reprennent une copie indépendante du dessin précédent :
identité, inversion, plateau, pic sur un pixel, variations aux extrémités, points
avec abscisse identique, extrémités décalées et listes dégénérées. Elles couvrent
aussi les données d'histogramme absentes/invalides, la rotation, l'opacité, le
redimensionnement, le repli fractionnaire et les changements pendant le masquage.
Les tolérances sont celles du comparateur commun, sans zone masquée. Validations
locales en logiciel, Metal et OpenGL ; autres GPU matériels à vérifier.

Le smoke test de l'interface ajoute et déplace un point pendant les previews,
vérifie sa validation dans le calque, le supprime par clic droit puis le restaure
par Annuler. Une annulation supplémentaire après fermeture doit ramener la liste
d'avant le geste d'ajout/déplacement. Les contrôles existants des pipettes restent
exécutés.

Références Qt : [QPainterPathStroker](https://doc.qt.io/qt-6.8/qpainterpathstroker.html),
[Shape](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html),
[échantillonnage des couches](https://doc.qt.io/qt-6.8/qml-qtquick-item.html#layer.samples-prop).


## Statistiques d'histogramme : grille native et cache sans pixels

`Document::histogram()` et `colorHistogram()` délèguent leur travail à
`ImageHistogramCache`. L'ancienne méthode convertissait l'intégralité de
`displayComposite()` en ARGB32 avant de lire une grille espacée. La nouvelle
méthode rassemble d'abord les pixels de cette même grille dans leur format
natif, puis demande à Qt la conversion de cette petite image en ARGB32. Il n'y a
pas de redimensionnement, d'interpolation ni de changement des coordonnées
échantillonnées.

Les pas historiques sont conservés : maximum entre 1 et la partie entière de
la racine de `largeur × hauteur / budget`, avec 262 144 pour la luminance et
65 536 pour le RVB. Ces paramètres déterminent le pas ; ils ne constituent pas
un plafond strict de pixels, notamment sur une image très allongée. La grille
part de (0,0) et garde ses dernières positions lorsque les dimensions ne sont
pas divisibles par le pas. Avec un pas de 1, Qt convertit directement la source.

Les pixels de profondeur au moins 8 bits sont copiés sans décodage manuel de
leurs canaux ; les formats monochromes recopient les indices de palette. La
table de couleurs et l'espace colorimétrique sont transmis. La conversion Qt
garde ainsi son traitement du 16 bits, de l'alpha prémultiplié et des arrondis,
sans remplacer ce traitement par une approximation de `pixelColor()`.
Pondération par l'alpha 8 bits, coefficients de luminance, arrondi et
normalisation restent ceux du calcul précédent. Les statistiques restent sur
CPU et sont toujours basées sur le composite affiché ; les traitements photo
et le rendu GPU ne changent pas.

Chaque Document conserve au plus une entrée de luminance et une entrée RVB,
identifiées par `QImage::cacheKey()`, les dimensions et le format réels de la
source affichée. Seuls les 256 résultats de luminance et les quatre listes de
256 résultats couleur restent en cache : aucune QImage ni référence à ses
pixels n'est gardée. Les appels répétés partagent ces listes. Les changements
réels de pixels ou de vue affichée invalident naturellement la clé. Un échec
d'allocation pour une image valide n'est pas mémorisé comme un résultat valable.
Ce cache s'utilise sur le thread propriétaire de Document.

`image_histogram` compare la collecte à la conversion complète indépendante,
pour chaque format QImage disponible et plusieurs pas, y compris les lignes
avec padding et les palettes. Les histogrammes sont comparés exactement en
8/16 bits, avec alpha nul, très faible et partiel, et les tests vérifient la
réutilisation des listes, l'absence de référence conservée sur les pixels,
les mutations de palette et l'alternance des sources. Les tests Document
exercent retouche, changement de couche affichée, Annuler/Rétablir et nouveau
document en 8/16 bits. Le moteur, les graphiques Histogramme/Seuil/Niveaux/Courbes
et le smoke test général sont aussi réexécutés.

Sur le cas local de 6 000 × 4 000 pixels RGBA64 prémultipliés, les images à
convertir comptent respectivement 667 × 445 et 316 × 211 pixels, au lieu de
24 millions pour chacun des deux histogrammes. Une mesure sur M2 Pro/Qt 6.12
donne environ 30,9 ms pour les deux calculs de référence contre 2,9 ms avec la
grille native ; 1 000 paires de lectures déjà en cache prennent environ 0,04 ms
dans ce test C++. Ces temps indicatifs excluent la composition, les échanges QML
et le dessin ; aucun seuil de vitesse n'est imposé aux tests.

Références Qt : [identité du contenu QImage](https://doc.qt.io/qt-6.8/qimage.html#cacheKey),
[conversion de format](https://doc.qt.io/qt-6.8/qimage.html#convertToFormat),
[lecture des lignes](https://doc.qt.io/qt-6.8/qimage.html#constScanLine).


## Roue de Transfert de dégradé : spectre fixe et voile procédural

Le spectre teinte/saturation du `GradientMapEditor` ne dépend que de ses
dimensions : ses 180 secteurs et son dégradé blanc sont conservés dans le
Canvas existant. Les synchronisations HSV des couleurs et les changements de
poignée n'en demandent plus le repeint. Les pointeurs et la bande finale du
dégradé gardent leurs contrôles et leurs calculs actuels.

`ColorWheelShade.qml` remplace le Canvas de luminosité indépendant par un shader
procédural sur RHI, pour les dimensions entières. Le shader trace un disque noir
anticrénelé ; il conserve le centre historique basé sur la largeur pour les deux
axes et le rayon `min(largeur, hauteur) / 2 - 9`. La géométrie est un rectangle
fixe et le paramètre de luminosité ne change que son opacité. Il n'y a ni image
source pour ce voile, ni tessellation de courbe, ni repeint raster pendant le
geste. L'anticrénelage utilise les dérivées de la distance au centre. L'opacité
héritée reste prise en compte par `qt_Opacity`.

Le backend logiciel, les dimensions fractionnaires et les erreurs de préparation
du shader disposent du dessin Canvas historique. Le Canvas est chargé seulement
pour ce repli et retiré du graphe de scène au retour GPU : aucun ancien disque
transparent ne peut s'ajouter au nouveau. Les modifications faites pendant le
masquage sont reprises au prochain affichage. Le spectre reste une petite image
Canvas ; cette étape ne le remplace pas par une nouvelle roue analytique et ne
modifie pas la conversion HSV ni le filtre photo.

`color_wheel` compare le voile à une copie indépendante de l'ancien dessin sur
un fond coloré, avec noir, transparence complète, valeurs intermédiaires, opacité,
rotation, tailles rectangulaires et fractionnaires, rayon nul, masquage et retour
GPU à dimensions de tampon arrondies identiques. Il vérifie directement
l’assombrissement du centre, puis les pixels après recréation : Qt peut réutiliser
les informations de shader de son cache alors que le gestionnaire de la nouvelle
instance signale encore Uncompiled. Les signaux de taille, identités d'objets et
signaux de repeint contrôlent séparément l'absence de géométrie ou d'image
recréée pendant les changements de valeur.

Un second contrôle utilise le vrai éditeur : le centre de la roue doit être
peint avant la mesure du cache ; modifier ses couleurs et la poignée active doit
laisser le compteur de repeints du spectre inchangé. Le smoke test général
exerce un glisser de la couleur puis de la luminosité, avec capture de souris,
accord avec les paramètres du calque et une seule annulation par geste.

Références Qt : [ShaderEffect](https://doc.qt.io/qt-6.8/qml-qtquick-shadereffect.html),
[uniformes et opacité](https://doc.qt.io/qt-6.8/qml-qtquick-shadereffect.html#shader-inputs-and-resources),
[source du cache de shaders Qt](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/items/qquickshadereffect.cpp).

## Aperçu de pointe du pinceau : illustration procédurale bornée

`BrushTipPreview.qml` extrait l’illustration des paramètres du pinceau de
`Main.qml`. Sept propriétés visuelles remplacent la demande de repeint sur
chaque notification générale `settingsChanged`. Modifier le lissage, le mode
de fusion ou un autre outil sans changer ces propriétés conserve l’aperçu. Les
loaders sont inactifs quand le dialogue est fermé ; la réouverture utilise les
valeurs courantes, y compris après Annuler.

Le chemin matériel utilise un rectangle et un shader sans texture source.
Le fond reste `#151a20` et les empreintes `rgb(190,236,203)`. Le rayon est
toujours plafonné à 38 pixels logiques ; l’espacement garde son minimum d’un
pixel, son départ à 28 et sa limite `largeur - 20`. Le nombre d’empreintes est
préparé avec la boucle historique pour conserver son arrondi d’extrémité. Le
shader inverse la rotation et l’aplatissement, évalue le dégradé radial et
compose les empreintes dans leur ordre. Dureté et transparence sont intégrées
au bord anticrénelé ; l’opacité héritée s’applique au résultat complet.

Chaque fragment ne considère que les empreintes dont l’enveloppe circulaire
peut le couvrir. Le chemin GPU exige un petit rayon d’au moins deux pixels et
un espacement d’au moins un huitième du rayon : au plus 17 empreintes sont
évaluées par fragment, quelle que soit la largeur. Les dérivées de coordonnées
sont calculées avant les branchements dépendant du fragment. Les pointes plus
fines et les accumulations plus denses conservent le Canvas, ses arrondis
séquentiels en 8 bits et son rendu des détails sous-pixel. Les dimensions
fractionnaires, le backend logiciel et les erreurs de préparation du shader
utilisent aussi ce repli. Le Canvas est retiré au retour sur GPU.

Cette étape concerne seulement l’illustration du dialogue : elle ne modifie ni
les empreintes déposées dans le document, ni le moteur de peinture, ni le
calcul des pixels 8/16 bits. Aucun changement d’interaction ou de réglage
supplémentaire n’est introduit.

`brush_tip_preview` compare les pixels à une copie indépendante du Canvas
historique : pointes dures/douces, angle, aplatissement, tailles minimale et
maximale, faible débit/opacité, espacement limite, superposition dense,
transparence, rotation et opacité héritées, largeur étroite, dimensions
fractionnaires, masquage et retour GPU. Les tolérances partagées des autres
surimpressions bornent les différences de lissage entre Canvas texturé et
shader natif. L’identité du shader, ses changements de taille et les signaux
de repeint vérifient que les réglages conservent le rectangle sans rasterisation.
Le smoke test général contrôle les liaisons réelles du dialogue, l’absence de
repeint pour le lissage seul, Annuler et la réouverture après modification
pendant le masquage.

Références Qt : [ShaderEffect et sorties prémultipliées](https://doc.qt.io/qt-6.8/qml-qtquick-shadereffect.html#shader-inputs-and-resources),
[Canvas et coût des transferts de texture](https://doc.qt.io/qt-6.8/qml-qtquick-canvas.html#detailed-description).


## Guides unis du lasso dans le graphe de scène GPU

`LassoGuide.qml` sépare maintenant le conteneur borné de son dessin. Les points,
leur enveloppe et le préfixe du chemin fixe restent en cache entre mouvements
du pointeur ; l’enveloppe du guide s’obtient en ajoutant le seul point provisoire,
sans reparcourir ni recopier les points fixes. Les repères visibles sont filtrés
à partir des points et de la vue, indépendamment du survol.

Le chemin SVG et le modèle de repères GPU ne sont pas préparés pour le backend
logiciel ou les candidats combinés qui ne les utilisent pas.

Sur RHI et avec CurveRenderer, `LassoSolidGuide.qml` dessine le contour ouvert
et les repères dans le graphe de scène. Le contour et son segment provisoire
forment un seul chemin, pour conserver la jointure à leur raccord. Ce chemin
est réévalué quand le segment provisoire change ; les chemins des repères restent
indépendants et leurs nœuds sont repositionnés lorsque l’origine locale change.
Il reste donc une préparation de géométrie CPU, mais aucun repeint Canvas ni
transfert d’image raster pendant le survol éligible.

Une couche GPU limitée à l’intersection du guide et de la vue compose les traits
et les repères avant l’opacité héritée, comme l’ancien Canvas. Elle peut changer
de dimensions avec l’enveloppe du guide ; il ne s’agit pas d’un shader sans
surface intermédiaire. Le trait garde sa largeur d’un pixel logique, ses couleurs,
ses extrémités plates et ses onglets. Les repères gardent leur rayon de trois
pixels et leur ordre de dessin. Une enveloppe entièrement hors champ ne charge
ni la couche GPU ni le Canvas.

Les candidats polygonaux combinés gardent le Canvas et leurs deux traits pointillés
animés. Les candidats libres combinés éligibles utilisent le shader décrit ci-dessous.
Le backend logiciel, Qt sans CurveRenderer et les écrans fractionnaires utilisent
aussi le repli des guides unis.
Sur un écran fractionnaire, la surface reprend l’origine et la taille de la vue
historique : découper davantage le Canvas modifierait sa phase de filtrage.
Une enveloppe hors champ reste vide. Les pointillés natifs gardent leur propre
lissage adapté au facteur d’écran, y compris à 1,25×.
Les retours presque superposés, dont l’angle intérieur a un cosinus supérieur
à 0,99, restent en raster : le découpage des onglets extrêmes de CurveRenderer
diffère de celui de QPainter. Les angles des points fixes sont testés seulement
quand les points changent ; seul le dernier raccord dépend du pointeur. Retirer
le point problématique ou déplacer le segment provisoire réactive le GPU et
retire le Canvas. Aucun seuil ni comportement de sélection n’est modifié.

Les marges de dessin incluent désormais l’onglet complet et sa frange de lissage,
pas seulement les poignées : 12 pixels logiques pour le polygonal uni et 17 pour
le candidat à trait de 1,5 pixel. Cela évite de couper une pointe aiguë encore
visible dans la vue. Le repli réaffirme aussi le trait uni au début de chaque
peinture : un ancien décalage de pointillés conservé par la relecture Context2D
ne doit pas rendre les repères d’un nouveau polygonal pointillés.

Le test `lasso_guide` compare à un Canvas indépendant en 8/16 bits : survol,
repères proches, onglets aigus, repli du dernier raccord et des points fixes,
ajout/retrait, fin de geste, annulation, masquage et réaffichage. Il vérifie le
zoom jusqu’à 64×, le défilement, la rotation, l’opacité après composition et la
sortie complète de la vue. L’absence de contexte raster sur le chemin GPU et
les compteurs de repeints, snapshots, préfixes, enveloppes et modèles de repères
contrôlent les réutilisations. Les comparaisons GPU emploient les tolérances de
lissage partagées ; les replis gardent la comparaison raster directe, sauf en cas
de rotation, de facteur d’écran ou de dimensions de surface fractionnaires où les
textures ont des phases de filtrage différentes. Le smoke test général couvre aussi le vrai mouvement de souris et
l’arrêt de l’animation sans sélection pointillée.

Références Qt : [Shape et CurveRenderer](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html),
[styles des jointures](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shapepath.html#joinStyle-prop),
[relecture des pointillés de Context2D](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/items/context2d/qquickcontext2dcommandbuffer.cpp).


## Pointillés des candidats combinés du lasso libre sur GPU

`DashedGuideItem` prépare une géométrie native à partir du snapshot de points,
sur le thread GUI. La conversion des éventuelles valeurs JavaScript s’effectue
avant le rendu ; le thread graphique ne consulte aucun objet du moteur QML.
Points, zoom, origine locale, dimensions et facteur d’écran invalident le maillage.
Changer la phase ne demande qu’une mise à jour de ses paramètres graphiques.

Le maillage partagé de sélection est utilisé à l’échelle logique : largeur de
1,5 pixel et période de 15 pixels, comme les deux traits de l’ancien Canvas.
Le shader adapte les franges de lissage au facteur d’écran. Les distances sont
rebasées au découpage sans redémarrer le motif. La fermeture conserve deux
extrémités plates, comme `QDashStroker`, y compris lorsque la longueur totale
est un multiple de la période. Une petite extension du maillage permet leur
lissage sans recréer les sommets pendant l’animation.

`DashedGuideNode` conserve deux passes ordonnées : le trait noir complet, puis
le trait blanc complet. Cet ordre reste valable aux croisements. Leurs sommets
et buffers ne changent pas avec la phase. Une couche bornée à la vue compose
les deux passes avant l’opacité héritée lorsqu’un segment est visible ; il reste
donc une surface GPU, mais aucune image raster à repeindre ou retransférer à chaque pas.

Un contour peut entourer toute la vue alors que tous ses bords sont hors
champ. Son enveloppe reste grande, mais le maillage découpé est vide.
`hasGeometry` distingue ce cas d’un tracé non pris en charge : le chemin GPU
reste sélectionné pour éviter de charger le Canvas, sa couche de composition
est désactivée et les changements de phase ne demandent plus de synchronisation
de rendu. Les points et la phase courante restent conservés. Revenir sur un
bord prépare la géométrie visible et réactive la couche sur le même objet.

Le scénario à 8× vérifie la désactivation réelle de `layer.enabled`, l’absence
de rappel `updatePaintNode`, de reconstruction, de transfert et de repeint Canvas
pendant plusieurs phases. Il contrôle aussi les pixels de la vue vide, puis le
retour au bord avec la dernière phase et le même composant natif.

Le chemin reste volontairement conservateur : angles aigus inférieurs à 90°,
points dégénérés/non finis, raccords sous-pixel qui se chevauchent, plus de
8 192 points, facteurs d’écran hors de [1,8]
ou dépassement du budget de sommets gardent le Canvas. Les jointures aiguës
du moteur de traits Qt ne sont pas remplacées par une approximation différente.
Les candidats polygonaux combinés et la transition Alt restent aussi en raster.
Masquage et sortie complète de la vue détruisent le composant chargé ; un retour
à une géométrie éligible retire le Canvas.

Le test `lasso_guide` compare les pixels à un Canvas indépendant en 8/16 bits,
avec phases fractionnaires et négatives, fermeture, croisements, angles aigus,
zoom 64×, défilement, rotation, faible opacité et masquage. Des compteurs vérifient
qu’une animation ne reconstruit ni ne retransfère la géométrie et ne repeint pas
de Canvas. Les entrées non finies et le dépassement du budget sont aussi testés.

Référence Qt : [traitement des sous-chemins pointillés dans QDashStroker](https://github.com/qt/qtbase/blob/v6.8.3/src/gui/painting/qstroker.cpp).

## Ancres et tangentes de la Plume sur GPU

`PenHandles.qml` extrait la feuille de repères de `PenOverlay.qml`. Les points,
le zoom, l’origine découpée et les dimensions de la feuille déterminent une liste
ordonnée de primitives visibles. La position de survol n’en fait pas partie :
déplacer le segment provisoire conserve cette liste et ses objets graphiques.
Les changements d’ancre, de tangente ou de sélection de points la réactualisent.
Le backend logiciel et Qt sans CurveRenderer ne préparent pas de SVG inutilisé.

Chaque point conserve l’ordre précédent : ligne et cercle de la direction
entrante, ligne et cercle de la direction sortante, puis carré d’ancrage. Les
directions de moins de 0,1 pixel logique restent masquées. Les points non
sélectionnés ne dessinent que leur carré. Les couleurs, traits d’un pixel,
cercles de rayon trois et carrés de côté six sont ceux du Canvas. Les primitives
sont filtrées par leur enveloppe avec marge ; une tangente traversant la vue
reste présente même si son ancre et sa poignée sont toutes deux hors champ.

`PenHandleGuides.qml` utilise CurveRenderer pour ces lignes et formes. Une couche
GPU limitée à la feuille visible compose les éléments avant l’opacité héritée,
y compris lorsque deux poignées et leur ancre se superposent. Le conteneur
clipse le résultat ; il ne crée pas de surface de la taille du document zoomé.
Cette couche utilise une texture de composition GPU, mais aucun Canvas ni
transfert de texture raster sur le chemin matériel. Les primitives et leur
préparation géométrique restent des calculs CPU.

Les loaders sont inactifs lorsque la feuille est cachée ou entièrement hors
champ. La réactivation prépare les repères depuis les points courants. Le
repli Canvas garde son dessin précédent et ses coordonnées image ; ses repeints
sont transmis par le signal de contrôle commun. Les changements de points
pendant le masquage ne déclenchent pas de peinture.

Cette étape ne modifie pas le modèle de tracé, les raccourcis, les gestes,
l’historique ou les pixels 8/16 bits. La mise en conformité des interactions de
la Plume avec Photoshop reste une tâche séparée dans la roadmap. Le segment
provisoire pointillé dispose maintenant du chemin GPU décrit ci-dessous, au-dessus
du tracé principal et sous les repères.

`pen_overlay` conserve ses comparaisons indépendantes en 8/16 bits : édition,
Annuler, fermeture, réactivation, zoom jusqu’à 64×, défilement fractionnaire et
rotation. Il vérifie maintenant l’absence de contexte raster pour les repères
GPU et la conservation du modèle de primitives et de son premier objet au
survol. Des cas isolés couvrent les tangentes traversantes, les ancres au bord,
les contrôles superposés avec une opacité de 37 %, les points non sélectionnés,
les directions presque nulles, la sortie de vue et la modification cachée.
Le compte de primitives vérifie la suppression des éléments hors champ et
l’absence de préparation SVG sur le repli logiciel. Le smoke test général
exerce un vrai glisser d’ancre et l’annulation, puis vérifie que le mouvement
de souris n’invalide pas les repères fixes.

Références Qt : [Shape et CurveRenderer](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html),
[couches et opacité des Items](https://doc.qt.io/qt-6.8/qml-qtquick-item.html#layer-opacity).


## Segment provisoire pointillé de la Plume sur GPU

`PenRubberBand.qml` sépare le segment provisoire de ses deux voisins, le tracé
retenu et la feuille de repères. Ses entrées sont le dernier point, sa tangente
sortante, le point de survol et l’origine découpée. Le survol réactualise seulement
ce segment : les descriptions du tracé principal et des repères restent en cache.
Les règles d’affichage, l’ordre des couches et les interactions sont conservés.

Le chemin matériel garde la cubique historique, dont les deux derniers contrôles
sont le point de survol. `PenBandGeometry` prépare son contour pointillé avec
l’API publique `QPainterPathStroker` : largeur de 1,5 pixel logique, extrémités
plates, jointures en onglet avec limite 10, motif Qt `[4,3]` et phase initiale
nulle. Ce motif garde les unités du QPen utilisé par le Canvas ; il n’est pas
réinterprété comme une autre longueur de tirets.

Le chemin et la largeur du trait sont multipliés par le facteur de pixels de
l’écran avant cette préparation, puis le contour revient en coordonnées logiques.
Cela conserve la subdivision des courbes et le placement des pointillés du
rasteriseur HiDPI. Le contour est une description SVG remplie en WindingFill par
`PenRubberGuide.qml`, avec CurveRenderer. Une couche GPU limitée à l’emprise
visible applique l’opacité après composition, y compris aux retours en arrière.
Il n’y a aucun repeint Canvas ni transfert d’image raster sur ce chemin ; la
préparation du contour et de la géométrie reste sur CPU.

L’éligibilité est volontairement conservative. Les trois points de l’enveloppe
de contrôle doivent rester dans les limites stables de la vue et de l’image,
avec une marge de deux pixels logiques. Cette vérification ne dépend pas du
petit cadre du segment, qui change pendant le mouvement : l’objet GPU reste
conservé tant que la courbe est éligible. La longueur du polygone de contrôle,
qui borne la longueur de la courbe, est limitée à 8 192 pixels logiques pour
borner la préparation des pointillés. Le facteur de pixels accepté est positif
et au plus égal à huit.

Les courbes coupées par la vue, les courbes plus longues, le logiciel et Qt sans
CurveRenderer utilisent le Canvas précédent. La coupure raster peut modifier
la subdivision des tirets ; ce repli conserve son placement. Le retour à une
courbe entièrement visible recharge le GPU et retire le Canvas. Un segment nul,
entièrement hors champ ou caché n’alloue aucune surface de dessin. Le SVG n’est
pas préparé pour le repli logiciel ni pendant les changements de survol cachés.

`pen_overlay` compare les pixels en 8/16 bits à la cubique Canvas indépendante,
avec déplacement, édition, Annuler, zoom jusqu’à 64×, rotation, défilement
fractionnaire, fermeture et réactivation. Des cas isolés ajoutent les segments
droits, courbés, avec retour en arrière, presque nuls, nuls, coupés et très longs,
leur opacité réduite et le retour du repli vers le GPU. Les compteurs de repeints,
descriptions et identités d’objets vérifient la réutilisation des parties fixes
et du composant provisoire pendant le survol. Le test d’interface exerce aussi
les vrais gestes de souris, la désactivation du guide et Échap.

Références Qt : [QPainterPathStroker](https://doc.qt.io/qt-6.8/qpainterpathstroker.html),
[préparation des traits dans QPaintEngineEx](https://github.com/qt/qtbase/blob/v6.12.0/src/gui/painting/qpaintengineex.cpp),
[CurveRenderer](https://doc.qt.io/qt-6.8/qml-qtquick-shapes-shape.html).

## Opérations complètes en arrière-plan

`DocumentOperations.cpp` centralise les opérations entières coûteuses, indépendamment
du backend d’affichage. Quatre millions de pixels traités déclenchent un calcul
sur snapshot avec priorité basse : redimensionnement, rotations/miroirs, recadrage
sur sélection, conversion de profondeur depuis le menu, fusion vers le bas,
remplissages/effacement, inversion de masques et filtres directs. Les opérations
compactes (remplir un masque uniforme, effacer un calque entier) restent immédiates.
Le worker ne capture jamais le document vivant ; le résultat est appliqué seulement
si session et version correspondent encore. La validation est atomique et ajoute
une seule étape d’historique. Échap, Annuler et Annuler/Rétablir abandonnent le calcul.
Fermer, remplacer ou modifier le document invalide également son résultat. Le
début d’un aperçu de propriétés ou de fusion, ainsi que les changements de tracé,
tranche ou texte, invalident la demande avant même le commit du geste. Une édition
de texte en cours est finalisée avant capture du snapshot du worker.

La progression est indéterminée pour les appels Qt monolithiques, et avance entre
les calques ou lignes pour les boucles instrumentées. Annuler rend la main tout de
suite ; une routine Qt non interruptible peut encore terminer son plan en arrière-plan.
RAW, sélections, recadrage interactif et préparation des fréquences gardent leurs
workers spécialisés. Sauvegarde/chargement de projets et export restent synchrones.

## Budget partagé des textures

`GraphicsBudget` compte les textures de tous les canevas : photos, masques,
tables/effets/surimpressions, pile/intermédiaires, et damier. Le total, les catégories,
les ressources épinglées et le pic sont accessibles par `CanvasItem.graphicsMemoryUsage()`.
Le maximum des cibles demandées par les canevas constitue la cible commune ; chaque
canevas garde également sa propre borne de réserve. Les ressources visibles sont
épinglées et l’espace libre est partagé entre les caches hors écran, évincés par LRU.
Modifier cette répartition réveille les autres canevas pour qu’ils réduisent leur
réserve, sans détruire de texture sur le thread d’une autre fenêtre.

Cette mesure utilise les tailles et formats demandés, pas les allocations privées
du pilote. Elle ne compte ni les polices ni les contrôles Qt, et ne promet pas un
plafond VRAM strict : les ressources visibles et les remplacements de ressources
en vol peuvent dépasser la cible. Les piles dont le jeu de travail ne tient pas
utilisent le repli CPU. Détruire le graphe de scène retire ses ressources du compteur.
