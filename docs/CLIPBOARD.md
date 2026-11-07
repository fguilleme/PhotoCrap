# Couper, copier et coller des sélections

Les commandes opèrent sur la sélection de pixels réelle : rectangle, ellipse,
rangée/colonne, lassos, baguette, sélection rapide, plage de couleurs ou tracé
converti en sélection. Les trous, îlots, inversions, anticrénelage et contours
progressifs sont conservés. Les coordonnées et dimensions sont celles du document,
pas celles de son affichage réduit.

## Commandes

Commande sur Mac correspond à Ctrl sur Windows/Linux. Les combinaisons ci-dessous
sont personnalisables dans Réglages → Raccourcis clavier.

| Commande | Raccourci | Comportement |
|---|---|---|
| Couper | Cmd/Ctrl+X | Copie les pixels du calque actif, puis retire la couverture sélectionnée. L’extérieur reste intact ; une seule annulation restaure pixels et sélection. |
| Copier | Cmd/Ctrl+C | Copie le canal de pixels actif, sans les autres calques ni la visualisation Rouge/Vert/Bleu. Ne modifie pas l’historique. |
| Copier avec fusion | Cmd/Ctrl+Maj+C | Copie le composite visible, avec ses réglages, opacités et masques. |
| Coller | Cmd/Ctrl+V | Crée et sélectionne un calque de pixels, centré dans le document ou dans le rectangle de la sélection de destination. Cette sélection ne découpe pas le collage normal. |
| Coller sur place | Cmd/Ctrl+Maj+V | Conserve les coordonnées d’origine d’une copie PhotoCrap, y compris entre documents. Une image externe sans coordonnées est centrée. |
| Coller dans la sélection | Cmd/Ctrl+Option/Alt+Maj+V | Crée un calque et un masque correspondant à la sélection de destination. Le contenu sous le masque reste disponible. |
| Coller hors de la sélection | Aucun par défaut | Même principe avec le masque inversé. |
| Nouveau calque par copie de sélection | Cmd/Ctrl+J | Extrait la sélection à sa position d’origine, sans utiliser le presse-papiers. Sans sélection, duplique le calque entier comme auparavant. |
| Nouveau calque par couper | Cmd/Ctrl+Maj+J | Déplace la sélection sur un nouveau calque à sa position d’origine, en une seule étape d’annulation. |

Couper demande une sélection et un calque de pixels modifiable, ou un masque
raster actif modifiable. Un calque verrouillé reste copiable. Une sélection
entièrement transparente ne remplace pas le presse-papiers et ne supprime rien.
Les opérations refusées n’ajoutent pas de calque. Après un découpage ou collage,
la sélection est retirée et reste récupérable par Resélectionner ou Annuler.

Sélectionner un masque raster puis Copier lit ses niveaux de gris ; Couper y
retire la sélection en noir ; Coller et Coller sur place écrivent dans ce masque
sans créer de calque, en respectant la sélection de destination si elle existe.
Les collages spéciaux Dans/Hors créent, eux, un nouveau calque masqué.

## Précision, placement et calcul

Les pixels et la transparence restent natifs en 8/16 bits ; le collage s’adapte
à la profondeur du document de destination. Les masques de calque restent en
8 bits. Une image copiée en 16 bits conserve aussi ses alphas intermédiaires dans
le presse-papiers interne et dans le format PNG privé échangé entre instances.
Le format image standard reste disponible aux autres applications ; leur prise
en charge de la profondeur et de la transparence dépend de leur lecteur.

Le collage ne redimensionne pas l’image : la même sélection garde ses dimensions
en pixels dans un autre document. Dans PhotoCrap, les portions hors canevas sont
rognées ; un collage sur place entièrement extérieur est ramené au bord pour
rester partiellement visible. Le choix **Presse-papiers** de Nouveau document
reprend les dimensions et la profondeur de l’image copiée.

Les lectures sont limitées à l’emprise de la sélection. Le découpage écrit par
tuiles, sans développer systématiquement tout le calque. Les grandes copies et
éditions passent sur worker ; un Coller demandé pendant Couper/Copier est différé
jusqu’au résultat courant. Annuler le découpage ou changer de document abandonne
ce collage en attente. Une copie asynchrone ne remplace pas un presse-papiers
externe modifié entre-temps. Les pixels d’une grande coupe sont préparés avant son
effacement asynchrone.

## Références Adobe et différences restantes

- [Copier et coller une sélection](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/copy-and-paste-selections.html) : distinction Copier/Copier avec fusion, nouveau calque et dimensions en pixels.
- [Supprimer ou couper les pixels sélectionnés](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/delete-or-cut-selected-pixels.html) : suppression de la sélection et transparence. PhotoCrap n’a pas de calque « Arrière-plan » verrouillé spécial qui substituerait sa couleur de fond à la transparence.
- [Coller dans ou hors d’une sélection](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/paste-one-selection-into-or-outside-another.html) : création d’un masque plutôt qu’effacement du contenu caché.
- [Copier et coller des calques](https://helpx.adobe.com/photoshop/desktop/create-manage-layers/create-layer-compositions/copy-and-paste-layers.html) : collage sur place et options du presse-papiers. **La copie complète de métadonnées de calque (texte éditable, forme vectorielle, effets, groupes et masques associés), le presse-papiers vectoriel de tracés et les profils couleur multiples restent NYI.** Copier sans sélection produit actuellement une image raster du calque actif ; Dupliquer conserve les données de calque dans le document.
- [Modifier les masques de calque](https://helpx.adobe.com/photoshop/desktop/create-masks/layer-masks/edit-and-refine-layer-masks.html) : collage dans le canal de masque. Alt/Option-clic sur sa vignette permet aussi de l’afficher seul.

La rasterisation des textes/formes par Copier ne transforme pas leur calque
source. Couper leurs pixels exige une pixellisation explicite ; leur masque
raster peut être coupé indépendamment. Les gestes de duplication Option/Alt avec
Déplacement restent disponibles et distincts du presse-papiers.
