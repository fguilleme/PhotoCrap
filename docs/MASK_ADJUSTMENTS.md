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

## Personnaliser le menu + des calques

Dans **Réglages → Menu des calques**, cochez les types de calques de réglage que vous souhaitez retrouver sous le bouton **+**. Vous pouvez aussi ouvrir cette page avec **+ → Personnaliser le menu…**. Une famille apparaît dès qu’au moins un de ses réglages est coché. Les choix sont enregistrés automatiquement et **Valeurs par défaut** réaffiche tous les réglages.

## Importer un rendu sur la photo ouverte

### À partir d’un fichier Adobe XMP

Ouvrez la photo à retoucher, puis choisissez **Calque → Importer des réglages Adobe XMP…**. Sélectionnez le fichier `.xmp` associé à la photo ou un préréglage Lightroom ou Camera Raw. PhotoCrap crée un calque pour chaque famille de réglages active.

| Calque | Réglages repris |
| --- | --- |
| Exposition | Correction en indices de lumination (IL) |
| Lumière | Contraste, hautes lumières, ombres, blancs et noirs |
| Couleur | Vibrance et corrections relatives de température et de teinte |
| Teinte / Saturation | Saturation globale |
| Noir et blanc | Conversion en niveaux de gris |
| Courbes | Courbe à points RVB et courbes rouge, verte et bleue |
| Texture | Une valeur négative adoucit les détails, une valeur positive les accentue |
| Clarté / Correction du voile | Clarté actuelle ou ancienne et correction du voile, positives ou négatives |
| Grain | Quantité, taille et rugosité. La taille Adobe de 0 à 100 est convertie en 1 à 8 pixels |
| Virage partiel | Teinte et saturation des ombres et des hautes lumières. Une balance positive favorise les hautes lumières, une balance négative les ombres |
| Netteté optimisée | Gain et rayon de l’accentuation |

Si un réglage est ignoré, le bilan indique son nom et le nombre de calques créés. Cela concerne notamment la température absolue en kelvins, les profils de caméra et les masques locaux. Le rendu utilise les filtres de PhotoCrap et peut différer du développement RAW d’Adobe.

### À partir d’une photo de référence

Ouvrez la photo à retoucher, puis choisissez **Calque → Reproduire le rendu d’une image…** et sélectionnez la photo dont vous souhaitez retrouver l’aspect. PhotoCrap compare les pixels visibles pour estimer l’exposition, le voile, la teinte dominante et la saturation. Des échantillons à la résolution d’origine servent à estimer la clarté et le grain supplémentaire. Les courbes ajustent les écarts de tonalité et de couleur restants. Seuls les réglages retenus sont ajoutés. Des sujets, des textures et des éclairages proches facilitent la comparaison.

Les deux commandes ajoutent leurs calques en haut de la pile. Double-cliquez sur un calque pour modifier ses réglages ou réduisez son opacité pour atténuer l’effet. **Annuler** retire tout l’import en une étape. Enregistrez en `.pcr` pour retrouver les réglages à la réouverture.

## Réduction du bruit

À l’ouverture ou à l’importation d’un RAW, cochez **Réduire le bruit à l’import** dans la fenêtre **Importer un RAW**, puis cliquez sur **Importer**. Le même choix s’applique à tous les RAW de la sélection. PhotoCrap mémorise le dernier choix, initialement désactivé. RawNIND reconstruit les couleurs et débruite les mosaïques Bayer en une passe. Pour les autres capteurs pris en charge par LibRaw, il intervient sur le RVB linéaire avant conversion en sRGB. Le fichier original est conservé.

Pour un JPEG, un TIFF ou une composition en cours, sélectionnez le calque voulu puis **+ → Détails et grain → Réduction du bruit RVB**. Le réglage NAFNet traite la composition située sous lui. Déplacez-le pour choisir les calques concernés, ajustez son intensité ou peignez sur son masque. Jugez les détails à 100 % de zoom. Les modèles sont fournis avec l’application et le calcul se fait sur cet ordinateur.
