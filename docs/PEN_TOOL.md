# Plume et panneau Tracés

Références Adobe consultées les 3 et 4 novembre 2026 :

- [Dessiner avec la Plume](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/draw-lines-curves/draw-paths-with-the-pen-tool.html).
- [Sélectionner des tracés et des segments](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/create-shapes/select-paths-and-path-segments.html).
- [Modifier les segments](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/create-shapes/edit-path-segments.html).
- [Gérer les tracés dans le panneau Tracés](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/create-shapes/manage-paths-in-the-paths-panel.html).

- [Modes de dessin et formes éditables](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/draw-lines-curves/understand-shapes-paths-and-drawing-modes.html).
- [Réglages Fond et Contour](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/create-shapes/fill-and-stroke-shapes.html).
- [Options de la Plume](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/draw-lines-curves/overview-of-pen-tool-settings.html).

## Dessiner et corriger

**P** active la Plume. Cliquer crée un angle ; glisser tire les poignées d'une
courbe. Maj contraint les angles à 45°. Espace pendant le glisser déplace le
nouveau point. Cliquer le premier point ferme le tracé ; glisser sur ce point
ferme avec une courbe. Retour termine un tracé ouvert. Échap termine d'abord la
construction ; un autre Échap désélectionne le tracé. Pendant un glisser, Échap
annule uniquement ce geste.

Cliquer une extrémité reprend un tracé ouvert. On peut repartir de l'autre bout ;
les poignées sont inversées avec l'ordre des points. Glisser à la reprise règle
la tangente sortante. Option/Alt sur le dernier point pendant la construction
retire uniquement sa poignée sortante : la courbe déjà dessinée reste intacte.
Option/Alt-glisser sur ce même point règle cette poignée indépendamment.
Ailleurs, l'outil Conversion ou Option/Alt-clic supprime les deux poignées ; un
glisser les recrée. Option/Alt-glisser sur une poignée casse leur liaison.

Commande sur Mac, Ctrl ailleurs, active temporairement la sélection directe.
Glisser déplace l'ancre et ses poignées. Maj-clic ajoute ou retire des ancres.
Une poignée lisse entraîne l'autre en conservant sa longueur ; une poignée
cassée se déplace seule. Les flèches déplacent les ancres sélectionnées de 1 pixel,
ou 10 avec Maj. Ajout/Suppression auto permet d'insérer un point sur un segment
sans modifier la courbe, ou de retirer un point existant.

**A** active la flèche noire **Sélection de tracé** : cliquer l'intérieur d'un
contour fermé, une ancre ou son contour sélectionne tout le tracé, puis glisser
le déplace. Commande/Ctrl permet de modifier une ancre avec la flèche blanche.
La variante **Sélection directe**, accessible par clic droit ou Maj+A, déplace
les points, poignées et segments individuellement. Son cadre sélectionne les
ancres ; Maj conserve les ancres déjà sélectionnées. Option/Alt-clic sélectionne
tout le tracé. Un clic dans le vide désélectionne sans effacer.

Chaque modification crée une seule étape d'historique ; sélectionner, changer
de panneau ou afficher un contour ne modifie pas les pixels. Les curseurs de
Plume indiquent création, ajout, suppression, fermeture, reprise et conversion.
Les deux flèches ont une pointe alignée avec leur point chaud. L'aperçu élastique
reste optionnel, et le rendu GPU conserve ses caches au survol.

## Mode Forme

Le menu **Forme / Tracé** choisit ce que la Plume crée. En **Forme**, le premier
point crée automatiquement un calque de forme distinct. Son contour Bézier est
conservé et modifiable ; les points, poignées et pixels d'affichage changent
ensemble. Une forme ouverte se remplit en fermant implicitement son intérieur ;
le contour peint reste ouvert jusqu'à sa fermeture explicite.

En mode **Tracé**, la barre propose **Créer : Sélection… / Masque (NYI) /
Forme**. **Forme** crée un calque vectoriel à partir du tracé sélectionné, en
conservant les ancres, tangentes et la fermeture exacte. Le tracé source reste
enregistré et indépendant dans Tracés. Une seule annulation retire la conversion ;
Rétablir la recrée. La liste des modes sert au prochain dessin et ne convertit pas
implicitement ce qui existe déjà. La même conversion est proposée dans le menu
du panneau Tracés.

En mode **Forme**, la barre affiche **Fond**, **Contour**, épaisseur, type de trait,
largeur et hauteur. Cliquer un échantillon propose **Aucun**, une couleur unie,
le premier plan et des couleurs usuelles. Choisir une couleur active le fond ou
le contour, même s'il était absent. Le symbole barré indique **Aucun**. Le contour
est peint dans le document : ce n'est pas le repère bleu de la Plume.
L'épaisseur accepte une saisie décimale ou le curseur du menu adjacent, de 0,5 à
500 pixels document. L/H redimensionnent la géométrie autour du coin supérieur
gauche ; la liaison conserve les proportions. Ces modifications restent
annulables, avec leurs pixels et leurs points.

Le tracé d'édition reste visible, avec une ligne bleue fine pour les formes,
les ancres et les poignées. Échap, un clic vide avec la Sélection directe ou
**⚙ → Masquer le tracé d’édition** enlève uniquement les contrôles ; le contour
peint reste dans l'image. Sélectionner à nouveau l'entrée de la forme dans
Tracés permet de continuer l'édition. Les épaisseurs restent celles du document :
un contour de 5,1 px à 13 % représente environ 0,66 pixel écran logique ; augmenter
l'épaisseur ou le zoom permet de distinguer ses détails du guide.

**⚙** contient l'aperçu élastique et les commandes pour terminer ou fermer un
tracé. Ajout/Suppression auto reste dans la barre. Sélectionner le calque ou
utiliser **ƒ / Propriétés** réactive ses réglages. Son entrée **Forme** dans
Tracés représente son masque vectoriel ; les tracés enregistrés restent séparés.
Passer au mode Tracé ne pixellise pas la forme.

Les formes fonctionnent en 8/16 bits, avec les masques raster, l'opacité,
l'écrêtage et les modes de fusion existants. Déplacement, copie de calque,
redimensionnement et recadrage conservent la définition vectorielle ; la commande
contextuelle **Pixelliser la forme** permet une conversion explicite en pixels.
Une modification ou un glisser est annulable avec ses pixels et son calque.
Les groupes/calques verrouillés protègent aussi les points et le style.
Le cache natif est constitué de tuiles dans l'emprise de la forme, sans allocation
pleine image pour une petite forme sur un grand document.

Les projets avec formes utilisent **v11** : géométrie et style sont stockés dans
le calque, avec un cache d'affichage natif. L'entrée temporaire du panneau Tracés
est reconstruite depuis ce calque à la réouverture. Elle n'est pas enregistrée
comme une seconde copie indépendante du dessin.

## Panneau Tracés

**Affichage → Panneaux → Tracés**, ou **Tracés…** dans les options de la Plume,
affiche la liste. Dans un nouvel espace de travail, Calques, Couches et Tracés
partagent des onglets. Une disposition déjà enregistrée est conservée ; le nouvel
onglet est ajouté au groupe de Calques. Le panneau peut être réordonné, détaché,
réancré, masqué et redimensionné comme les autres. Sa disposition est sauvegardée
dans les réglages.

- Cliquer une ligne active la Sélection directe et affiche son contour, même si la brosse ou la Plume était utilisée auparavant. Cliquer une ancre permet ensuite de régler ses poignées. Cliquer la zone
  vide du panneau désélectionne. Double-cliquer le nom le renomme.
- Commande/Ctrl-clic sur une ligne charge sa sélection sans changer l’outil courant ; Maj ajoute, Option/Alt
  soustrait et les deux intersectent. Le bouton de sélection propose les mêmes
  combinaisons Maj et Option/Alt.
- Les boutons inférieurs remplissent au premier plan, tracent avec les réglages du
  pinceau, convertissent en sélection, créent un tracé vide ou suppriment le tracé.
- Le menu propose également un nouveau tracé nommé, la duplication et les commandes
  Monter/Descendre. Toutes les modifications de la liste sont annulables.
- Après conversion en sélection, seul le contour en fourmis reste affiché. Le tracé
  demeure dans le panneau et peut être choisi à nouveau.

Les opérations de remplissage et de contour respectent le calque ou masque actif,
sa sélection et la profondeur 8/16 bits. Les tracés ne sont pas des calques de
pixels et ne se retrouvent pas dans une exportation raster sans cette opération.

Les projets conservent les noms, courbes, ordre et tracé actif. Un projet contenant
un tracé nommé encore vide utilise **v10**, pour que les anciens lecteurs le
refusent explicitement. Les autres projets continuent d'utiliser v9. Le nouveau
lecteur accepte les versions 1 à 11 ; il conserve la validation stricte des tracés
non vides des versions 8/9.

## Écarts encore ouverts

Cette étape ne reproduit pas tout l'espace vectoriel de Photoshop. Chaque entrée
contient un seul contour ; les composants multiples, jonctions entre composants et
opérations booléennes restent NYI. Les entrées sont conservées : le remplacement
automatique d'un unique « Tracé de travail » temporaire de Photoshop n'est pas
reproduit. Les masques vectoriels sur les autres types de calques, conversion sélection → tracé,
sélection simultanée de plusieurs entrées, duplication par Option/Alt-glisser,
déplacement des lignes de la liste par glisser, découpe d'un segment sans retirer
ses ancres, simulation de pression et options de contour progressif/anticrénelage
lors de la conversion restent à faire. Le contour progressif peut déjà être appliqué
à la sélection après conversion. Les plumes libre et de courbure restent NYI.

Les remplissages de forme par dégradé/motif, les alignements de contour intérieur/extérieur et les opérations booléennes restent à faire ; la première version propose des couleurs unies et un contour centré.
