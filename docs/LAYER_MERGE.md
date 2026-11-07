# Fusion et copies fusionnées des calques

Les commandes sont disponibles dans **Calque** et dans le menu contextuel du
panneau Calques. Elles opèrent sur les calques entiers, indépendamment d’une
sélection de pixels ou de la visualisation isolée d’un canal/masque.

| Commande | Raccourci Mac / Windows–Linux | Résultat |
|---|---|---|
| Fusionner vers le bas | Cmd/Ctrl+E avec un seul calque actif | Réunit le calque actif et son voisin inférieur de pixels visible, dans le même groupe. Le calque inférieur donne le nom au résultat. |
| Fusionner les calques sélectionnés | Cmd/Ctrl+E avec plusieurs calques sélectionnés | Remplace les calques choisis par un calque de pixels ; les autres restent en place. |
| Fusionner le groupe | Cmd/Ctrl+E sur son en-tête | Remplace le groupe et ses membres par un calque de pixels. Les enfants cachés ne contribuent pas au rendu et sont supprimés avec le groupe. |
| Fusionner le masque d’écrêtage | Aucun par défaut | Depuis la base ou un membre écrêté, réunit la base de pixels et les membres visibles de sa chaîne d’écrêtage. |
| Fusionner les calques visibles | Cmd/Ctrl+Maj+E | Réunit les calques visibles en tenant compte de la visibilité de leur dossier. Conserve les calques cachés et les dossiers nécessaires pour les retrouver. |
| Créer un calque des sélectionnés fusionnés | Cmd/Ctrl+Option/Alt+E | Crée une copie raster au-dessus des calques choisis et conserve tous les originaux. |
| Créer un calque des visibles fusionnés | Cmd/Ctrl+Option/Alt+Maj+E | Crée une copie du rendu visible au-dessus des sources, sans utiliser le presse-papiers ni supprimer les originaux. |
| Aplatir l’image | Aucun par défaut | Remplace tout le document par le rendu visible sur fond blanc. Supprime les calques cachés ; une confirmation dans l’interface permet d’annuler avant cette suppression. |

Les raccourcis sont configurables dans Réglages. Les menus se désactivent lorsque
la commande n’a pas de cible valable. Les fusions qui remplacent les sources
respectent leur verrouillage et celui du dossier ; les copies fusionnées peuvent
lire des sources verrouillées. L’aplatissement, qui remplace explicitement toute
la pile, inclut aussi les sources verrouillées.

## Rendu, position et annulation

La fusion utilise le moteur natif du document : pixels 8/16 bits, modes de fusion,
opacités, masques activés, densité et contour progressif, réglages, remplissages,
texte, formes et fréquences. Les masques et les réglages deviennent des pixels
dans le résultat ; Annuler restaure leurs données éditables. Un réglage ou un
remplissage peut être fusionné vers une base de pixels, mais ne peut pas être la
base inférieure d’une fusion partielle. Fusionner vers le bas ne traverse pas la
frontière d’un groupe.

Une fusion partielle calcule ses sources sur fond transparent, puis place le
résultat en mode Normal, opacité 100 %. Les pixels hors de la sélection de
calques ne sont pas inclus. Avec des sources non contiguës, des modes dépendant
d’un fond extérieur ou des réglages agissant sur des calques non fusionnés,
l’apparence de la pile peut donc changer. Pour obtenir une copie exacte de toute
la composition courante, utiliser la copie des visibles. Comme tout nouveau
calque visible, une copie fusionnée peut augmenter l’opacité de zones déjà
partiellement transparentes si les originaux restent visibles dessous.

Une couche écrêtée doit être fusionnée avec sa base : une sélection partielle
qui l’exclut est refusée plutôt que de produire une image vide ou de changer de
base silencieusement. La commande dédiée prend la chaîne correspondante. Les
sources cachées sélectionnées explicitement ne participent pas au rendu de la
fusion ; elles sont retirées avec les sources remplacées. Les fusions visibles
préservent au contraire les sources cachées.

Un résultat reste dans son dossier si toutes ses sources sont des membres du
même dossier. Une fusion de groupe, de plusieurs dossiers ou de toute la pile
produit un calque racine au-dessus des blocs concernés. Les dossiers partiellement
conservés ne sont pas coupés en deux. Les pixels hors du canevas ne sont pas
conservés, comme pour les autres outils raster actuels de PhotoCrap.

Chaque commande ajoute une étape d’annulation, sans modifier le presse-papiers
ni la sélection de pixels. Les grandes fusions s’effectuent sur worker annulable.
Le résultat est préparé avant de remplacer les sources ; une erreur, une
annulation ou un changement de document laisse la pile initiale intacte. Le
rendu se fait par bandes natives de 256 lignes avec les halos du compositeur,
puis devient un plan tuilé compatible avec le budget RAM et le cache de travail.
Les piles qui nécessitent déjà un rendu intégral gardent ce repli. Les fonds
uniformes restent compacts ; les limites de calques et de stockage sont vérifiées
sur la pile résultante, en déduisant les sources supprimées.

## Références et différences avec Photoshop

- [Fusion et copies fusionnées Adobe](https://helpx.adobe.com/photoshop/desktop/create-manage-layers/get-started-layers/merge-and-stamp-layers.html) : variantes, groupes d’écrêtage, raccourcis et conservation des sources lors d’une copie fusionnée.
- [Fusion des réglages et remplissages](https://helpx.adobe.com/photoshop/desktop/create-manage-layers/color-adjustment-fill-layers/merging-adjustment-or-fill-layers.html) : application des réglages à une cible raster.
- [Aplatissement Adobe](https://helpx.adobe.com/photoshop/desktop/create-manage-layers/get-started-layers/flatten-layers-in-an-image.html) : exclusion des sources cachées et remplissage blanc de la transparence.

PhotoCrap n’a pas encore de calque Arrière-plan spécial : le résultat opaque de
l’aplatissement est un calque de pixels ordinaire nommé Arrière-plan. Les calques
liés, objets dynamiques, effets de calque avancés, groupes imbriqués et scripts
Adobe « Aplatir tous les effets/masques » restent NYI. Les fusions de calques liés
deviendront applicables avec leur modèle de liaison. Les menus actuels n’annoncent
pas ces variantes comme disponibles.
