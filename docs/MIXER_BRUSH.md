# Pinceau mélangeur

La barre suit les commandes documentées par Adobe : pointe, charge actuelle,
rechargement et nettoyage automatiques, combinaison, Humidité, Charge, Mélange,
Débit, aérographe, lissage, angle et échantillonnage de tous les calques.

## Utilisation

- Cliquer sur la vignette choisit une couleur et charge le pinceau. Sa flèche
  propose **Charger**, **Nettoyer** et **Charger les couleurs unies seulement**.
- **Option/Alt-clic** prélève sans peindre. Par défaut, la charge conserve les
  différentes couleurs sous la pointe et sa vignette les montre. Avec l’option
  couleurs unies, seul le pixel visé est prélevé.
- **Humidité** règle la reprise des couleurs du canevas et leur entraînement.
  À zéro, le pinceau peint à sec et Mélange est désactivé.
- **Charge** détermine combien de peinture reste disponible avant épuisement.
  **Mélange** dose charge et canevas : 0 % privilégie la charge, 100 % le canevas.
  Ce dernier cas ne consomme pas la réserve de peinture chargée.
- **Débit** dose chaque empreinte. Il reste indépendant des combinaisons.
- Les deux boutons près de la vignette nettoient et/ou rechargent **après chaque
  trait**. Les deux activés donnent un pinceau nettoyé puis rechargé. Sans
  nettoyage, la peinture prélevée peut être entraînée au trait suivant.
- **Aérographe** ajoute des empreintes tant que le bouton reste enfoncé, même
  sans mouvement. Désactivé, le dépôt dépend du déplacement.
- **Tous les calques** prélève la composition visible, tout en peignant sur le
  seul calque actif. Sur un masque, le mélange reste en niveaux de gris.
- **Maj-clic** continue en ligne droite depuis le dernier point. Un trait, même
  en aérographe, reste une seule étape d’annulation ; Échap l’abandonne.

Le menu contient Personnalisé et les douze combinaisons Sec/Humide/Mouillé/Très
mouillé, avec variantes de charge ou de mélange, correspondant aux familles
présentées dans la documentation Adobe. Modifier le débit ne passe pas la
combinaison à Personnalisé.

## Calcul et limites

Deux réserves sont distinctes : la charge de couleur et la peinture prélevée.
Chaque empreinte lit la région du canevas **déjà modifiée par le trait**. La
peinture fraîche peut donc être reprise et mélangée, y compris en échantillonnage
multicalque. Les autres calques gardent leurs versions de départ pendant le geste.
Un échange pondéré dans la zone de contact permet à la couleur rencontrée en
bord de pointe d’alimenter le mélange. Sélection, contour progressif et masque
limitent le dépôt. Le calcul ne prépare pas d’image complète par empreinte ;
seules les régions et tuiles touchées sont lues et écrites.

Les coefficients de mélange, d’échange et de consommation, ainsi que les valeurs
des combinaisons, sont propres à PhotoCrap. Adobe ne publie pas son algorithme
ni les valeurs numériques de ces combinaisons dans les références ci-dessous.
Cette implémentation ne prétend donc pas produire un rendu identique pixel par
pixel, ni simuler les soies physiques de Photoshop. Les pointes à soies, la
pression et l’inclinaison restent indiquées **NYI** dans les options.

Le mélangeur travaille encore sur les **pixels 8 bits** et les masques 8 bits.
Les pixels couleur 16 bits restent explicitement bloqués, sans conversion
silencieuse. Les masques des documents 16 bits restent utilisables.

## Références Adobe

- [Paint with Mixer Brush — Adobe Photoshop](https://helpx.adobe.com/photoshop/desktop/apply-painting-techniques/create-fill-with-patterns/paint-with-mixer-brush.html) :
  réserves, charge multicolore, commandes et interaction Humidité/Charge/Mélange.
- [Painting with the Mixer Brush — guide Adobe](https://helpx.adobe.com/ph_en/photoshop/using/painting-mixer-brush.html) :
  prélèvement, combinaisons et accumulation avec l’aérographe.
- [Support pédagogique Adobe, page 4 — menu des combinaisons](https://cdn.edex.adobe.com/exAttachments/115-9-30467.pdf).

## Validation

`MixerBrushTest.cpp` vérifie la reprise de peinture fraîche sur le calque et sur
le composite, le transport des couleurs, la charge multicolore/unie, les douze
combinaisons et le débit indépendant, l’épuisement, le nettoyage, l’aérographe,
l’annulation et les écritures régionales sur 24 Mpx. `MixerUiSmoke.h` exerce la
saisie réelle des pourcentages, les commandes et le dépôt sans mouvement.
