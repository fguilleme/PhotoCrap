# Réglages directs et masques vectoriels

Références Adobe consultées le 10 novembre 2026 :

- [Modifier et affiner les masques de fusion](https://helpx.adobe.com/photoshop/desktop/create-masks/layer-masks/edit-and-refine-layer-masks.html).
- [Masquer les calques avec un masque vectoriel](https://helpx.adobe.com/photoshop/desktop/create-masks/layer-masks/mask-layers-with-vector-masks.html).

## Appliquer un réglage directement

Sélectionner la vignette du calque pour modifier ses pixels, ou sa vignette de
masque de fusion pour modifier uniquement le masque. **Réglages** ouvre les
éditeurs existants (Lumière, Niveaux, Courbes, Couleur, etc.) pour cette cible.
**Filtre** propose notamment **Flou gaussien**, Flou directionnel, Flou de boîte,
Netteté optimisée, Passe-haut, Mosaïque et Bruit.

Le panneau affiche un aperçu réduit asynchrone. **Appliquer** calcule les pixels
natifs en arrière-plan pour les grandes images et crée une étape d’historique.
**Annuler** ou Échap abandonne la session. Une sélection limite les pixels modifiés,
y compris ses niveaux de gris ; les filtres spatiaux lisent un halo autour de sa
région utile. Les calques gardent leur profondeur 8 ou 16 bits, les masques restent
en gris 8 bits. Les pixels couleur ne sont pas modifiés par un filtre sur le masque.

Pour créer un calque de réglage non destructif, utiliser **Calque → Nouveau calque
de réglage**, ou le menu **+** du panneau Calques. Le menu Réglages n’ajoute plus
un calque implicitement.

## Adoucir les deux côtés du masque

Créer un masque depuis une sélection convertit ses niveaux de gris, puis retire
la sélection active dans la même étape d’historique. **Resélectionner** permet de
la retrouver. Un **Flou gaussien** appliqué ensuite au masque peut ainsi produire
du gris des deux côtés de son ancien bord, au lieu de rester coupé par la sélection.

Si une sélection a été créée ou réactivée ensuite, le panneau de réglage affiche
**Limiter à la sélection**. Décocher cette case traite la cible entière sans
supprimer la sélection ; aperçu et application finale utilisent la même portée.
La case reste cochée par défaut pour respecter une sélection volontaire.

## Garder un contour vectoriel modifiable

1. Sélectionner le calque à masquer, puis dessiner un tracé avec la Plume en mode
   **Tracé** ou sélectionner un tracé enregistré dans le panneau Tracés.
2. Choisir **Créer : Masque** dans la barre de la Plume.
3. Une vignette supplémentaire apparaît dans Calques. Cliquer dessus sélectionne
   le contour et la Sélection directe ; déplacer une ancre ou une poignée modifie
   le masque sans modifier les pixels du calque.
4. Double-cliquer cette vignette pour ouvrir **Densité** et **Contour progressif**.
   Le rayon reste modifiable : il ne convertit pas le contour en masque peint.
5. Le clic droit, ainsi que **Calque → Masque vectoriel**, donne accès aux propriétés, à l’édition du tracé, à son
   activation/désactivation, à sa suppression et à la pixellisation explicite.

Le masque vectoriel et le masque de fusion peint sont indépendants et peuvent
coexister : leurs couvertures se multiplient. Les déplacements du calque entier,
redimensionnements, recadrages et rotations transforment aussi le contour.
Annuler/Rétablir et les projets conservent la géométrie et ses propriétés.

Un filtre direct nécessitant des pixels ne pixellise jamais silencieusement le
masque vectoriel. Utiliser son **Contour progressif** pour l’adoucir, ou
**Pixelliser le masque vectoriel** avant un traitement destructif. Cette conversion
est disponible quand il n’y a pas déjà un masque de fusion peint, pour ne pas
écraser ce dernier.

## Limites actuelles

Un masque vectoriel contient un seul contour ; les composants multiples,
opérations booléennes, masques sur dossiers et dissociation du déplacement du
calque restent à faire. Le contour progressif utilise le même flou de masque que
les propriétés des masques pixels ; il ne prétend pas reproduire le noyau Adobe
bit pour bit. L’affichage des piles contenant ces nouveaux masques ou un aperçu
direct utilise le compositeur CPU asynchrone ; leur prise en charge analytique
par les shaders de pile reste à faire.
