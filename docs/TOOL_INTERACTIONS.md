# Souris et clavier des outils

Comportements disponibles dans v0.15. Les références sont les pages Adobe Photoshop pour ordinateur. Option correspond à Alt ; Commande est la touche principale sur macOS, Ctrl sur Windows/Linux.

## Règles communes

| Geste | Comportement |
|---|---|
| Lettre d’un groupe | Active sa dernière variante visible et disponible |
| Maj + lettre | Passe à la variante suivante, sans activer un outil NYI ou masqué |
| Option/Alt-clic sur le bouton d’outil | Passe à la variante suivante |
| Clic droit ou appui prolongé sur un groupe | Ouvre ses variantes |
| Maintenir la lettre plus de 200 ms | Utilise l’outil temporairement ; le relâchement revient à l’outil précédent |
| Commande/Ctrl + molette ou défilement au pavé tactile | Zoome autour du pointeur dans toute la zone du canevas, marges comprises, quel que soit l’outil |
| Molette ; Maj + molette | Déplace la vue verticalement ; horizontalement |
| Espace + glisser, hors tracé en cours | Déplace la vue |
| Commande/Ctrl + Espace + clic/glisser | Zoom temporaire ; Option/Alt inverse le sens |
| Commande/Ctrl avec un outil de sélection ou de peinture | Déplacement temporaire au prochain glisser |
| Verr. Maj | Alterne avec un curseur de précision en croix |

Dans **Réglages → Barre d’outils**, les options de cycle avec Maj et de retour après un raccourci maintenu sont sauvegardées. Les raccourcis sont suspendus dans les champs de texte et les dialogues modaux. Les séquences personnalisées à plusieurs frappes restent utilisables, sans cycle ni retour temporaire.

## Sélections

- Maj **avant le clic** ajoute ; Option/Alt avant le clic soustrait ; Maj + Option/Alt intersecte. Le mode est temporaire et revient à sa valeur initiale après le geste, y compris pour un lasso à plusieurs clics.
- Rectangle/ellipse : Maj pendant le tracé contraint carré/cercle. Option/Alt **pressé après le début** dessine depuis le centre ; Espace pendant le tracé déplace le cadre sans déplacer la vue.
- Cliquer à l’intérieur d’une sélection géométrique sans modificateur déplace son contour. Les flèches déplacent le contour d’un pixel ; Maj + flèche de dix pixels. Commande/Ctrl + flèche déplace les pixels sélectionnés.
- Rangée/colonne : le glisser positionne la sélection d’un pixel d’épaisseur.
- Lasso polygonal : clics pour les sommets, Maj pour les angles multiples de 45°, double-clic/Retour pour terminer, Retour arrière pour retirer un sommet, Échap pour abandonner. Option/Alt + glisser ajoute une partie libre.
- Lasso libre : Option/Alt maintenu après le début permet de poursuivre par segments au relâchement de la souris ; les clics ajoutent des sommets. Reprendre un glisser sans Option/Alt revient au tracé libre ; double-clic/Retour termine.
- Échap pendant une création restaure la sélection précédente. Les modifications du contour ne changent pas les pixels ; elles ont maintenant une étape annulable sans rendre le contenu sauvegardé modifié.

Les commandes **Sélection → Modifier** offrent Contour progressif (Maj+F6), Frange, Lisser, Dilater et Contracter avec paramètres et calcul en arrière-plan ; Échap annule un calcul en cours. **Affichage → Afficher → Contour de la sélection** cache les fourmis sans supprimer la sélection. Une nouvelle sélection réaffiche le contour. **Sélection → Plage de couleurs** prélève dans l’aperçu ou sur le canevas, sans fermer le dialogue ; Maj ajoute et Option/Alt retire une couleur. Tolérance, portée localisée, zones contiguës et inversion actualisent l’aperçu du masque. Les deux aperçus partagent zoom et déplacement : molette, −/+, 100 %, Adapter et glisser. Le dialogue se déplace par son en-tête ; les raccourcis de zoom du canevas restent actifs et Échap annule. Les bords progressifs influencent réellement les opérations sur les pixels et les masques. Voir [l’inventaire Adobe, options et limites](SELECTIONS.md).

## Peinture et retouche

Les champs numériques de la barre valident la valeur par **Retour** ou quand on
quitte la saisie ; px et % restent affichés à côté du nombre, hors du texte
éditable. Pour le pinceau, la dureté modifie la douceur du bord, l’opacité
plafonne un passage et le débit règle le dépôt par empreinte. Le crayon garde
un bord net sans anticrénelage : la dureté n’y est pas réglable ; opacité et
débit modifient toujours sa peinture. Référence : [peindre avec le pinceau ou
le crayon, Adobe](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/brushes-presets/paint-with-brush-or-pencil-tool.html).

Correcteur localisé, Suppression, Correcteur, Pièce, Déplacement basé sur le contenu
et Œil rouge sont disponibles sur les calques de pixels 8 ou 16 bits. Leurs
raccourcis, sources, modes et gestes restent identiques ; leurs pixels de travail
conservent la profondeur native, même avec « Tous les calques ». Le menu du groupe
J les active donc aussi en 16 bits, sans le suffixe de limitation « 8 bits ».


- Maj-clic relie le dernier point de peinture au nouveau point. Maj pendant un glisser contraint le trait horizontalement ou verticalement.
- Les chiffres règlent l’opacité (0 = 100 %, 4 puis 5 = 45 %) ; Maj + chiffres règle le débit. Avec Flou/Netteté/Doigt/Densité/Éponge, les chiffres règlent l’intensité.
- `[` / `]` parcourent les tailles usuelles de brosse ; Maj + `[` / `]` change la dureté par pas de 25 %. D rétablit noir/blanc ; X échange premier et arrière-plan.
- **Mac : Control + Option + glisser** ; **Windows/Linux : Alt + bouton droit + glisser** : horizontalement pour la taille, verticalement pour la dureté. Ce geste ne peint pas. Le clic droit simple ouvre les réglages de la brosse.
- Option/Alt avec pinceau, crayon, gomme, formes d’historique, remplissage ou dégradé prélève le premier plan, sans peindre. La pipette normale prélève l’arrière-plan avec Option/Alt ; Maj ajoute un échantillon.
- Les variantes existantes restent en place : Option/Alt définit la source du tampon/correcteur, charge le pinceau mélangeur et prélève la couleur de remplacement ; il inverse Flou/Netteté et Densité −/+.

## Pot de peinture

Le groupe G contient **Dégradé** et **Pot de peinture**. Le pot remplit avec la
couleur de premier plan ou un motif (damier, points, rayures, ou une sélection
définie comme motif). Sa barre propose mode de fusion, opacité, tolérance de 0
à 255, anticrénelage, **Pixels contigus** et **Tous les calques**.

- Pixels contigus coché : seules les couleurs similaires reliées au clic sont
  remplies. Décoché : les zones séparées de couleur similaire le sont aussi.
- Une sélection limite toujours le remplissage, avec sa couverture progressive.
- Tous les calques détecte les couleurs dans la composition visible ; les pixels
  sont déposés uniquement sur le calque actif. Cette option est désactivée lors
  de l’édition d’un masque, qui reste une surface en niveaux de gris.
- Les calques 8 et 16 bits gardent leur profondeur. Les gros remplissages passent
  en arrière-plan et sont annulables pendant le calcul ; un remplissage achevé
  correspond à une seule étape d’historique.

Référence : [remplir avec le Pot de peinture, Adobe](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/fill-objects-selections-layers/fill-paint-bucket-tool.html).

## Formes, déplacement, recadrage et tranches

- Formes : Maj pour des proportions égales, Option/Alt pour dessiner depuis le centre, Espace pour repositionner pendant le tracé. Ligne/dégradé : Maj contraint les angles par pas de 45°.
- Déplacement : Maj contraint l’angle ; Option/Alt-glisser copie la sélection sur le même calque, ou duplique le calque entier s’il n’y a pas de sélection. Une seule annulation restaure l’état précédent.
- Recadrage : Maj maintient les proportions du cadre initial, Option/Alt redimensionne depuis le centre, Espace repositionne un cadre pendant sa création. Retour applique, Échap abandonne. Les flèches déplacent le cadre d’un pixel, Maj + flèche de dix.
- Tranche : Maj crée un carré ; Option/Alt pressé après le début dessine depuis le centre ; Espace repositionne pendant la création. Option/Alt **avant le clic** active la sélection de tranche. Les flèches déplacent la tranche sélectionnée.

## Limites du modèle actuel

PhotoCrap reste un éditeur raster : tracés Bézier conservés et calques de forme de la Plume, sans masques vectoriels généraux sur les autres types de calques ; absence de transformation libre, rotation/redressement du recadrage et données hors canevas. Le déplacement actuel concerne le calque de pixels actif ; le déplacement simultané des calques sélectionnés et l’auto-sélection sous la souris ne sont pas encore équivalents à Photoshop. La barre de motifs/pointes Adobe et son sélecteur contextuel ne sont pas reproduits : le clic droit ouvre notre dialogue de brosse. La disponibilité de Commande/Ctrl + Espace dépend aussi des raccourcis réservés par le système. Les outils NYI restent désactivés.

## Références Adobe

- [Sélection et cycle des outils](https://helpx.adobe.com/photoshop/using/tools.html)
- [Raccourcis temporaires](https://helpx.adobe.com/ca/photoshop/desktop/get-started/set-up-toolbars-panels/spring-loaded-shortcuts.html)
- [Sélections géométriques](https://helpx.adobe.com/uk/photoshop/using/selecting-marquee-tools.html)
- [Lassos](https://helpx.adobe.com/photoshop/desktop/make-selections/get-started-selections/isolate-specific-areas-in-images-with-lasso-tool.html)
- [Déplacer une sélection](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/move-selection-or-selection-border.html)
- [Peinture](https://helpx.adobe.com/photoshop/using/painting-tools.html)
- [Curseurs et réglage de brosse à la souris](https://helpx.adobe.com/in/photoshop/desktop/get-started/settings-and-preferences/change-tool-pointers.html)
- [Formes](https://helpx.adobe.com/id_en/photoshop/using/drawing-shapes.html)
- [Recadrage](https://www.adobe.com/learn/photoshop/web/crop-straighten-to-improve-composition)
- [Guide et tableau complet des raccourcis, notamment les tranches](https://helpx.adobe.com/content/dam/help/en/pdf/photoshop_reference.pdf)

## Gomme magique

Référence : [Gomme magique, documentation Adobe](https://helpx.adobe.com/photoshop/using/tool-techniques/magic-eraser-tool.html). Le groupe E contient désormais Gomme et Gomme magique ; Maj+E parcourt les variantes disponibles. Le clic droit choisit la variante.

Un clic efface les couleurs proches de l’échantillon sur le calque de pixels actif. Tolérance va de 0 à 255 ; Contigu limite l’effacement à la zone connectée. Tous les calques prélève dans la composition visible, mais seul le calque actif est modifié. Opacité règle la force d’effacement ; Lissage adoucit les bords. Le calcul est asynchrone, annulable par Échap, respecte la sélection et crée une seule étape d’historique. Les masques n’ont pas de transparence à effacer : utiliser la gomme normale ou le pinceau sur un masque.

La comparaison de couleurs et le lissage sont des algorithmes propres à PhotoCrap ; leurs résultats ne sont pas garantis identiques à Photoshop. Les composantes 16 bits restent à leur précision native. Le traitement des pixels est limité aux zones correspondantes ; la copie nécessaire à l’historique conserve encore un plan entier en mémoire.

## Plume et tracés Bézier

Références : [dessiner à la Plume](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/draw-lines-curves/draw-paths-with-the-pen-tool.html), [modifier les tracés](https://helpx.adobe.com/photoshop/using/editing-paths.html), [formes et modes de dessin](https://helpx.adobe.com/photoshop/using/drawing.html).

- **P** active le groupe Plume. Le clic droit sur son icône propose Plume, Ajout de point, Suppression de point et Conversion de point ; Plume libre et Plume de courbure restent NYI. Le raccourci du groupe est configurable ; une ancienne personnalisation de P est conservée.
- Un clic place un point anguleux ; clic-glisser crée les deux poignées d’un point lisse. Maj contraint les segments et poignées par pas de 45°. Espace pendant le glisser repositionne le point sans déplacer le canevas.
- Cliquer le premier point ferme le tracé ; glisser sur ce point ferme avec une courbe. Retour ou Échap termine un tracé ouvert sans le supprimer. Cliquer une extrémité permet de le poursuivre, depuis l’un ou l’autre bout.
- Commande/Ctrl-glisser déplace un point avec ses poignées. Commande/Ctrl + Maj-clic ajoute ou retire un point de la sélection ; les flèches déplacent les points sélectionnés d’un pixel, Maj + flèche de dix pixels.
- Option/Alt-clic sur un point supprime ses poignées ; Option/Alt-glisser sur ce point en crée de nouvelles. Option/Alt-glisser sur une poignée la déplace indépendamment de l’autre. Sans Option/Alt, les poignées d’un point lisse restent alignées et la longueur de la poignée opposée est conservée.
- Avec **Ajout/Suppression auto**, cliquer un segment ajoute un point par subdivision Bézier sans modifier sa forme ; cliquer un point existant le supprime. Pendant la construction, le premier point sert à fermer et le dernier à supprimer la poignée sortante pour continuer en ligne droite. Retour arrière/Suppr retire les points sélectionnés. Échap pendant un glisser annule ce geste.
- **A** active la Sélection de tracé (flèche noire), pour déplacer tout un contour. La Sélection directe (flèche blanche, variante du même groupe) sélectionne les points ou segments, par clic ou cadre ; Option/Alt-clic choisit le contour entier. Un clic vide ou Échap hors construction désélectionne. Les glissers de reprise règlent désormais la poignée sortante ; Option/Alt sur le dernier point conserve sa poignée entrante. [Détails et écarts restants](PEN_TOOL.md).
- **Aperçu** active l’élastique du prochain segment. Le curseur indique création, fermeture, reprise, ajout, suppression, conversion ou déplacement ; Verr. Maj donne la croix de précision.
- Le panneau **Tracés**, disponible depuis Affichage et le bouton **Tracés…**, remplace la liste de la barre supérieure. Il affiche des vignettes et permet de choisir, créer, renommer, dupliquer, réordonner et supprimer les tracés. Il partage par défaut les onglets de Calques/Couches et conserve les capacités de disposition et de fenêtre flottante. Les tracés restent indépendants des calques et sont conservés en `.pcr` v9, ou v10 si un tracé nommé est encore vide ; les formes utilisent v11 et le lecteur accepte les versions 1 à 12.
- **Sélection** ou Commande/Ctrl + Retour transforme le tracé en sélection et désactive son affichage : seules les fourmis de sélection restent visibles. Le tracé est conservé ; le choisir à nouveau dans le panneau Tracés réaffiche ses points et poignées. Maj ajoute, Option/Alt soustrait et Maj + Option/Alt intersecte avec le raccourci. Le remplissage ferme implicitement un tracé ouvert. **Remplir** utilise le premier plan et l’opacité de peinture ; **Contour** utilise les réglages actuels du pinceau. Ces deux opérations écrivent dans le calque de pixels ou le masque actif, respectent une sélection existante et conservent les pixels 8/16 bits.
- Un geste d’édition devient une étape d’annulation et ne recalcule pas la composition de l’image. Le rendu de l’overlay est limité au viewport. Les limites sont de 128 tracés, 4 096 points par tracé et 32 768 points au total.

Les modes disponibles sont **Tracé** et **Forme**. Le mode **Forme** crée désormais un calque éditable avec fond/contour et un masque vectoriel propre au calque. Les masques vectoriels sur les autres calques, opérations booléennes entre sous-tracés, tracés composés, tracé de travail temporaire, simulation de pression lors du contour et conversion en sélection à contour progressif/anticrénelage fractionnaire restent NYI. Le tracé n’est pas un calque de pixels : il faut le remplir, tracer son contour ou le convertir en sélection pour agir sur l’image.


### Dégradé dynamique

Le premier glisser crée un calque de remplissage avec les paramètres du dégradé.
Les deux poignées restent visibles quand ce calque est sélectionné avec l’outil
Dégradé. Glisser une poignée modifie la direction ou la longueur. Cliquer sur
la ligne ajoute une couleur. Glisser un point intermédiaire change sa position,
un double-clic ouvre sa couleur et un clic droit supprime ce point. Chaque geste
peut être annulé. La sélection de départ devient un masque de fusion.

Les projets qui contiennent un dégradé dynamique utilisent le format **v12**.
Le moteur GPU calcule les cinq formes en 8 et 16 bits et conserve la palette
lors des changements de géométrie. Le moteur CPU conserve le rendu de référence
pour les exports et les compositions de secours.
