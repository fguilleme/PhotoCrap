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
La position du pointeur ne change que `x`/`y` : le graphe de scène déplace la texture
existante. Une nouvelle couleur ou la réapparition de l’anneau demande un repeint.
La surface reste fixe au zoom, indépendamment des dimensions du document.

La règle utilise les coordonnées natives des deux points, multipliées par le zoom.
Son rectangle de dessin est l’intersection de l’emprise du segment, avec une marge
pour les poignées, et de la vue calculée par `ScreenOverlayCanvas`. Le dessin est
translaté vers cette origine locale. Une petite mesure garde donc une petite
surface, et une mesure traversant tout le document reste limitée à l’écran, même
après défilement ou rotation. Son dessin reste raster ; les changements des points
repeignent cette surface bornée.

`MeasurementOverlaySmoke.h`, exécuté par `measurement_overlay`, affiche ces composants sur
une géométrie de document 16 384 × 16 384 à 16×, sans allouer cette image complète.
Il compare leurs pixels, observe le signal réel `painted` de l’anneau pour vérifier
la réutilisation au déplacement, et contrôle la règle courte, longue, défilée,
tournée et masquée. Les mêmes scénarios sont exécutés sur le backend logiciel,
Metal et OpenGL. Le smoke test QML vérifie aussi leur branchement dans l’interface
règle/pipette, en conservant les curseurs natifs et le panneau Informations.
