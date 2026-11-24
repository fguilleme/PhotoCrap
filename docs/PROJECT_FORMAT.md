# Format des projets `.pcr`

## Conteneur binaire v14 — depuis v0.19.0

Les nouvelles sauvegardes utilisent un conteneur binaire, sans chaînes Base64.
Les anciens fichiers JSON v1–v13 restent lisibles ; les réenregistrer les convertit
au nouveau conteneur. Les anciennes versions de PhotoCrap ne peuvent pas lire ces
nouvelles sauvegardes. La migration conserve le chemin et utilise un remplacement
atomique ; pour garder une copie compatible avec un ancien éditeur, conserver
l’ancien fichier séparément avant de l’enregistrer.

Les pixels éditables sont compressés **sans perte** avec **Zstandard 1.5.7**, niveau
3, après une décorrélation réversible rouge−vert / bleu−vert, un prédicteur
Paeth et une séparation des octets des canaux. Les échantillons prémultipliés sont conservés exactement, y compris les
faibles alphas et les valeurs 16 bits ; aucun aller-retour par le PNG ni réduction
à 8 bits ne sont nécessaires. Le niveau choisi privilégie le compromis taille /
vitesse, et ne garantit pas une taille inférieure au PNG sur toute image.

- Tuiles indépendantes de **256×256 pixels**, avec bords partiels.
- RGBA prémultiplié 8 ou 16 bits par canal ; masques en niveaux de gris 8 bits.
- Ordre RGBA canonique ; valeurs 16 bits en little-endian.
- Tuiles absentes représentées par la couleur de fond native du plan.
- Plans uniformes stockés par une couleur et leurs dimensions logiques.
- Un même plan partagé est référencé une fois ; les tuiles identiques entre plans
  différents sont dédupliquées par SHA-256, dimensions et format.
- Une tuile peu compressible est conservée en octets natifs si Zstandard
  l’agrandit. Son empreinte SHA-256 est toujours vérifiée à la lecture.
- Les sorties IA non retouchées conservent leur fichier d’origine PNG/JPEG/WebP
  comme bloc binaire, sans recompression ni Base64. Les retouches utilisent les
  tuiles natives ci-dessus.

L’aperçu Finder est un **JPEG d’affichage**, séparé des données éditables. Sa
compression avec perte n’affecte pas les pixels du document. Il est disponible
immédiatement après l’en-tête, sans charger l’index ni le codec Zstandard.

## Disposition et limites

Tous les nombres de l’en-tête sont des entiers non signés little-endian.

| Position | Taille | Contenu |
|---|---:|---|
| 0 | 8 | Signature `PCRBIN\r\n` |
| 8 | 8 | Version du conteneur : 14 |
| 16 | 8 | Taille de l’aperçu JPEG, 0 à 512 Kio |
| 24 | 8 | Position de l’index |
| 32 | 8 | Taille compressée de l’index |
| 40 | 8 | Taille décompressée de l’index |
| 48 | 8 | Taille exacte du fichier |
| 56 | 8 | Réservé, zéro |
| 64 | variable | Aperçu JPEG, puis blocs de données |
| position de l’index | variable | Index CBOR compressé avec Zstandard |

L’index CBOR contient `document`, `planes` et `blocks`. Les métadonnées du document
prolongent le schéma fonctionnel v9–v13 avec les versions logiques 14 à 16
décrites ci-dessous. Ces numéros de schéma ne sont
pas la version du conteneur. Les anciens noms internes `png`, `mask`, `source`
référencent désormais des identifiants de plans entiers ; ils ne contiennent
aucune image encodée en texte. `-1` représente l’absence d’un plan.

Un descripteur de plan est `[largeur, hauteur, octetsParPixel, fond, tuiles, métriques]`.
Les métriques conservent `[devicePixelRatio, dotsPerMeterX, dotsPerMeterY]`.
Le fond est une chaîne d’octets CBOR native. Une liste de tuiles vide représente
un plan uniforme ; sinon les identifiants sont ordonnés par lignes, avec `-1`
pour une tuile égale au fond. Un plan contenant un fichier IA original utilise
`[0, 0, 0, octetsVides, identifiantBloc, tableauVide]`.

Un bloc est décrit par `[position, tailleStockée, tailleNative, codec, prédicteur,
largeur, hauteur, octetsParPixel, sha256]`. Le codec vaut 1 pour Zstandard, 0 pour les octets bruts. Le prédicteur vaut 2
pour décorrélation couleur + Paeth + séparation des octets, 1 pour delta horizontal
avec séparation des octets, 0 pour les octets bruts. Les données originales
IA utilisent des dimensions et une profondeur nulles. Les blocs uniques sont
contigus, dans leur ordre d’écriture, avant l’index ; aucune entrée ne peut
pointer hors de cette zone ou chevaucher un autre bloc.

Limites du lecteur et de l’écriture :

- fichier binaire : **8 Gio** ; anciens fichiers JSON : **256 Mio** ;
- index compressé et décompressé : **32 Mio** chacun ;
- une tuile native : **512 Kio** maximum ; fichier IA original : **100 Mio** ;
- 512 calques, 128 Mpx, 16 384 px par côté et budget logique de plans de 2 Gio ;
- au plus 1 536 plans et 262 144 blocs uniques ;
- fenêtre de décompression Zstandard bornée à 8 Mio.

Les dimensions, formats, références et réservations mémoire sont validés avant
publication du document. Les checksums Zstandard et les SHA-256 des blocs détectent
les erreurs de données. Un chargement échoué ou annulé conserve le document ouvert.
Les lectures se font par fragments de 16 Kio et la compression / décompression
par tuile, sans tampon contenant toutes les images du projet. L’index et les
empreintes de déduplication restent en mémoire, dans les limites indiquées.

L’écriture utilise `QSaveFile`, avec mise à jour finale de l’en-tête. Annuler ou
échouer avant publication conserve le fichier précédent. Les grandes sauvegardes
restent exécutées en arrière-plan.

## Références

[Zstandard : dépôt officiel](https://github.com/facebook/zstd),
[API officielle](https://facebook.github.io/zstd/zstd_manual.html),
[CBOR / RFC 8949](https://www.rfc-editor.org/rfc/rfc8949.html).
Zstandard est embarqué statiquement avec sa licence BSD ; aucune installation
de codec système n’est nécessaire.

## Métadonnées des masques vectoriels

Les projets contenant un masque vectoriel utilisent la version logique **14**
dans l’enveloppe binaire existante. `parameters._vectorMask` contient un tracé
validé et ses dimensions de référence ; `_vectorEnabled`, `_vectorDensity` et
`_vectorFeather` conservent les propriétés du masque. Aucun bitmap vectoriel
pleine image ni aperçu transitoire de réglage direct n’est enregistré.
Les masques peints gardent leur bloc binaire distinct. Les anciens projets
restent lisibles ; une ancienne application doit refuser cette nouvelle version
plutôt qu’afficher un calque sans son masque.

## Calques 3D

Un projet qui contient un calque `model3d` utilise la version logique **15**
dans le même conteneur binaire. Le champ `model3d` référence un bloc original
qui contient le GLB complet. `parameters.pose3d` enregistre le placement,
l’échelle et l’orientation. `parameters.light3d` contient l’éclairage commun,
identique sur tous les calques 3D. Le rendu transparent est conservé comme
plan de pixels pour la composition, avec le masque éventuel dans son bloc.

Les projets avec de la peinture sur un modèle utilisent la version logique **17**.
Le champ `modelPaint` référence un bloc original contenant jusqu’à 8 192
empreintes de pinceau. Chaque empreinte contient douze flottants 32 bits en
petit-boutiste. Ils décrivent le centre et le rayon dans le repère du maillage
normalisé, la normale et la dureté, puis la couleur RVB linéaire et l’opacité.

Le prototype accepte les maillages triangulés avec couleurs et textures PNG/JPEG
intégrées. Pour un modèle articulé, il calcule la pose par défaut du squelette
à l’import et conserve les animations dans le GLB embarqué. Le calque affiche
une pose fixe. Les GLB sont limités à 512 Mio chacun, avec un plafond de
8 millions de sommets, 8 millions de triangles et 400 Mio de données décodées
par modèle. Le budget des modèles est de 1 Gio par document. Les textures
couleur se partagent un plafond de 16 777 216 pixels par modèle.
Quand une simplification Meshy est demandée, `parameters.triangleTarget`
conserve la cible et le projet garde le GLB source. La cible peut être dépassée
pour préserver des détails du maillage. L’éclairage est diffus, avec une
composante ambiante et des matériaux non éclairés selon le GLB. Les matériaux
semi-transparents sont découpés selon leur alpha. La superposition entre
objets suit l’ordre des calques.
[cgltf 1.15 (MIT)](https://github.com/jkuhlmann/cgltf/releases/tag/v1.15).

## Calques de peinture et de texte sur surface

Un projet contenant une carte de normales utilise la version logique **16**.
Sur un calque de pixels ou de texte, `surfaceNormals.png` référence un bloc PNG
RGB sans perte, limité à 768 px sur le grand côté. `surfaceNormals.width` et
`surfaceNormals.height` indiquent les dimensions du canevas de référence et
`surfaceNormals.transform` conserve les neuf coefficients qui alignent la carte
sur le canevas courant. Le masque de surface reste un plan raster éditable.
Un calque de texte conserve aussi son contenu, sa mise en forme et son rendu
déformé, ce qui permet de reprendre l’édition après réouverture.

Les projets contenant un calque de rééclairage utilisent la version logique 18. La liste `lights` contient la position X/Y/Z, la couleur, l’intensité et la douceur de chaque source. La carte de géométrie embarquée stocke les normales en RVB et la profondeur inverse relative dans le canal alpha (zéro indique une mesure invalide).

Les documents contenant des groupes imbriqués, masqués ou à opacité réduite utilisent la version logique 19. Un dossier conserve son identifiant et référence son parent par `parentGroupId`. Son masque utilise le même plan 8 bits que les autres masques. Pour les plans couleur binaires non uniformes, le budget compte les tuiles présentes, avec une réservation indépendante pour chaque calque.
