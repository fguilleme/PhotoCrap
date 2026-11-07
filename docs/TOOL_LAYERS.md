# Création de calques par les outils

## Règles de PhotoCrap

| Outil / mode | Cible du geste |
| --- | --- |
| Texte horizontal | Calque de texte éditable vide ; un clic ponctuel ou un glisser pour un paragraphe. Le calque est retiré si l’on valide sans saisir. |
| Rectangle, Ellipse, Triangle, Polygone, Étoile, Ligne — Nouveau calque | Nouveau calque transparent par geste, sélectionné automatiquement. C’est le mode par défaut. |
| Les mêmes formes — Pixels sur calque actif | Dessin dans le calque de pixels sélectionné ; les erreurs de cible et les verrouillages restent applicables. |
| Pinceau/Crayon sur contenu non peignable, préférence activée | Nouveau calque de pixels au premier geste ; les gestes suivants peignent ce calque. |
| Outil de peinture, retouche, dégradé, forme ou texte sur un masque sélectionné | Le masque est prioritaire, sans création de calque. Les couleurs sont converties en niveaux de gris. |
| Dégradé raster classique, Pot de peinture, gomme et retouches | Cible actuelle et restrictions propres à l’outil ; aucun contournement général par création de calque. |
| Plume en mode Forme | Nouveau calque vectoriel au premier point, fond et contour modifiables ; les points restent éditables après le dessin, projets v11. |
| Plume en mode Tracé, sélections, recadrage et mesures | Aucun nouveau calque pour définir leurs repères. |

La préférence Pinceau/Crayon est activée par défaut et se trouve dans **Réglages →
Outils**. Elle est conservée entre lancements. Un calque de pixels verrouillé
reste protégé ; cette option cible les types non peignables, sans convertir ni
rasteriser le contenu existant.

Le masque sélectionné reste la cible même si les formes sont réglées sur
« Nouveau calque » ou si le dégradé est en mode dynamique. La barre indique alors
« Masque de fusion ». Le dégradé et les formes y produisent des pixels, sans
modifier le contenu ou les paramètres du calque porteur. Le texte est éditable
pendant sa saisie (déplacement et mise en forme inclus), puis déposé dans le masque
à la validation ; Annuler restaure le masque initial. Les tracés de Plume restent
disponibles et leur remplissage, contour ou mode Forme peuvent peindre le masque.
La gomme, y compris magique, y utilise le gris de la couleur d’arrière-plan.
Les outils fondés sur une source lisent le masque, même si « Tous les calques »
était activé pour la retouche couleur. Œil rouge et Éponge n’ont pas d’effet couleur
sur ces valeurs grises. Les verrouillages, sélections et contours progressifs
restent applicables. Changer de cible annule une réparation encore en calcul.

Référence : [éditer un masque de fusion, Adobe](https://helpx.adobe.com/photoshop/using/editing-layer-masks.html).

Le calque indépendant est inséré au-dessus du calque actif, ou à l’intérieur du
groupe sélectionné, déplié pour montrer ce nouveau contenu. Un groupe masqué ou verrouillé reste intact : le nouveau
contenu est placé comme frère visible au-dessus de ce groupe. Le nouveau calque
n’hérite ni du masque, ni du verrouillage, ni de l’opacité du contenu sélectionné.
Un panneau de propriétés ouvert est validé et fermé avant la nouvelle création,
pour éviter qu’une ancienne session puisse effacer le dessin à sa fermeture.
Il devient la seule cible sélectionnée et quitte l’affichage du masque seul.
Les limites de 512 calques et du budget de pixels restent vérifiées avant création.

Le dessin et le calque créé appartiennent à la même étape d’annulation.
L’abandon d’une forme raster et le clic sans déplacement restaurent l’état précédent,
sans calque vide ni étape supplémentaire. Un texte vide, une opacité nulle ou une
forme sans remplissage ni contour ne crée pas de calque. La précision 8/16 bits
est conservée ; les calques de texte éditable nécessitent le projet v9.

## Références Adobe et écarts actuels

- [Calques de texte](https://helpx.adobe.com/photoshop/desktop/text-typography/type-layers-creation/type-layers-in-photoshop.html) : Photoshop crée automatiquement un calque de texte dédié et éditable. PhotoCrap conserve le contenu et la mise en forme dans un calque éditable, avec saisie sur le canevas. Voir [la couverture des fonctions de texte](TEXT_TOOL.md).
- [Formes, tracés et modes de dessin](https://helpx.adobe.com/photoshop/desktop/draw-shapes-paths/draw-lines-curves/understand-shapes-paths-and-drawing-modes.html) : le mode Forme crée un calque de forme, le mode Tracé un tracé de travail, et Remplir les pixels agit sur les pixels courants. PhotoCrap propose Nouveau calque et Pixels sur calque actif pour ses formes raster ; le premier n’est pas un calque vectoriel. La Plume propose également le mode Forme, avec définition Bézier et styles conservés dans le calque. [Comportements et limites](PEN_TOOL.md).
- [Créer un calque avec Pinceau/Crayon](https://helpx.adobe.com/ca/photoshop/desktop/apply-painting-techniques/fill-objects-selections-layers/create-a-new-layer-when-brushing.html) : la préférence Adobe crée un calque de pixels lorsqu’on peint sur un contenu non peignable, par exemple un objet dynamique ou un réglage. PhotoCrap applique cette règle aux types qu’il prend en charge ; les objets dynamiques restent absents.
- [Création et groupes](https://helpx.adobe.com/photoshop/desktop/create-manage-layers/create-layer-compositions/create-layers-and-layer-groups.html) : les nouveaux calques se placent au-dessus de la sélection ou dans le groupe sélectionné. Le placement hors d’un groupe protégé/masqué décrit ci-dessus est la règle explicite de PhotoCrap.

Les masques vectoriels généraux et les opérations booléennes restent des étapes
distinctes. Un dégradé sur un masque est raster ; ses poignées ne restent pas
éditables après validation, contrairement au calque de remplissage Dégradé.

## Vérifications

Les tests du document couvrent chaque forme et le texte en 8/16 bits au-dessus
d’un réglage, ainsi que les outils ciblant son masque sélectionné, les pixels d’origine préservés, Annuler /
Rétablir, l’abandon et le clic simple, le mode Pixels, les groupes, la capacité,
la préférence persistante et Pinceau/Crayon sur masque ou réglage. Le smoke test
QML passe par les événements souris réels de la fenêtre Qt, le choix du mode de
forme et la case des réglages.

Le test ciblé `tool_layers` est aussi exécuté sur Cocoa/Metal et Cocoa/OpenGL,
avec les événements Qt de la fenêtre et sans déplacer la souris physique.
Le smoke QML général reste validé sur backend logiciel. Un essai complet en
Cocoa/Metal avec Qt 6.12.0 a planté dans
`QQuickMenuPrivate::setNativeMenuVisible` lors de l’ouverture automatisée du menu
Fichiers récents, avant les scénarios de création de calques ; le test ciblé
permet de vérifier ces scénarios indépendamment de ce problème de menu natif.
